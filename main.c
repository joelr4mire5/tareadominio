#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 8
#define COLS 8

int main(int argc, char *argv[]) {
    int rank, size;
    char dominio[ROWS][COLS];
    int dias;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Verificar los argumentos correctos
    if (argc != 3) {
        if (rank == 0) {
            printf("Uso: mpirun -np <num_procesos> %s <archivo_entrada.txt> <numero_de_dias>\n", argv[0]);
        }
        MPI_Finalize();
        return 0;
    }

    dias = atoi(argv[2]);

    // Lectura del archivo input_file localizado en la carpea data
    if (rank == 0) {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            printf("Error al abrir el archivo %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                fscanf(fp, " %c", &dominio[i][j]);
            }
        }
        fclose(fp);
    }

    // Broadcast del dominio a todos los procesos


    MPI_Bcast(&dominio[0][0], ROWS * COLS, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Calcular el número de filas por proceso
    int fila_por_proceso = ROWS / size;
    int remanente = ROWS % size;

    int *conteos = malloc(size * sizeof(int));
    int *desplazamiento = malloc(size * sizeof(int));

    int offset = 0;
    for (int i = 0; i < size; i++) {
        int rows = fila_por_proceso;
        if (i < remanente) {
            rows++;
        }
        conteos[i] = rows * COLS; // Número total de elementos
        desplazamiento[i] = offset;
        offset += conteos[i];
    }

    int elementos_locales = conteos[rank];
    int filas_locales = elementos_locales / COLS;

    // Asignar memoria para datos locales
    char **dominio_local = malloc((filas_locales + 2) * sizeof(char *));
    char **nuevo_dominio_local = malloc((filas_locales + 2) * sizeof(char *));
    for (int i = 0; i < filas_locales + 2; i++) {
        dominio_local[i] = malloc(COLS * sizeof(char));
        nuevo_dominio_local[i] = malloc(COLS * sizeof(char));
    }

    // Inicializar ghost rows
    memset(dominio_local[0], 0, COLS * sizeof(char));
    memset(dominio_local[filas_locales + 1], 0, COLS * sizeof(char));

    // Scatter del dominio a todos los procesos
    MPI_Scatterv(&dominio[0][0], conteos, desplazamiento, MPI_CHAR,
                 &dominio_local[1][0], elementos_locales, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Bucle de simulación
    for (int dia = 0; dia < dias; dia++) {
        // Intercambio de ghost rows con vecinos
        MPI_Request reqs[4];
        if (rank > 0) {
            // Enviar primera fila al proceso anterior
            MPI_Isend(dominio_local[1], COLS, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, &reqs[0]);
            // Recibir ghost row del proceso anterior
            MPI_Irecv(dominio_local[0], COLS, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &reqs[1]);
        } else {
            reqs[0] = MPI_REQUEST_NULL;
            reqs[1] = MPI_REQUEST_NULL;
        }
        if (rank < size - 1) {
            // Enviar última fila al siguiente proceso
            MPI_Isend(dominio_local[filas_locales], COLS, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &reqs[2]);
            // Recibir ghost row del siguiente proceso
            MPI_Irecv(dominio_local[filas_locales + 1], COLS, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD, &reqs[3]);
        } else {
            reqs[2] = MPI_REQUEST_NULL;
            reqs[3] = MPI_REQUEST_NULL;
        }

        // Esperar a que la comunicación termine
        MPI_Waitall(4, reqs, MPI_STATUSES_IGNORE);

        // Aplicar las reglas
        for (int i = 1; i <= filas_locales; i++) {
            for (int j = 0; j < COLS; j++) {
                int arbol_adyacente = 0;
                int lagos_adyacente = 0;

                // Verificar vecinos
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0)
                            continue;
                        int ni = i + di;
                        int nj = j + dj;
                        if (nj >= 0 && nj < COLS && ni >= 0 && ni < filas_locales + 2) {
                            char vecino = dominio_local[ni][nj];
                            if (vecino == 'a')
                                arbol_adyacente++;
                            else if (vecino == 'l')
                                lagos_adyacente++;
                        }
                    }
                }

                char current = dominio_local[i][j];

                // Aplicar reglas
                if (current == 'a' && lagos_adyacente >= 4) {
                    nuevo_dominio_local[i][j] = 'l';
                } else if (current == 'l' && lagos_adyacente < 3) {
                    nuevo_dominio_local[i][j] = 'd';
                } else if (current == 'd' && arbol_adyacente >= 3) {
                    nuevo_dominio_local[i][j] = 'a';
                } else if (current == 'a' && arbol_adyacente > 4) {
                    nuevo_dominio_local[i][j] = 'd';
                } else {
                    nuevo_dominio_local[i][j] = current;
                }
            }
        }

        // Actualizar dominio local
        for (int i = 1; i <= filas_locales; i++) {
            memcpy(dominio_local[i], nuevo_dominio_local[i], COLS * sizeof(char));
        }
    }

    // Reunir los resultados
    MPI_Gatherv(&dominio_local[1][0], filas_locales * COLS, MPI_CHAR,
                &dominio[0][0], conteos, desplazamiento, MPI_CHAR, 0, MPI_COMM_WORLD);

    // El proceso 0 escribe el dominio final en el archivo
    if (rank == 0) {
        FILE *fp = fopen("final.txt", "w");
        if (fp == NULL) {
            printf("Error al abrir el archivo final.txt\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                fprintf(fp, "%c ", dominio[i][j]);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }

    // Liberar memoria asignada
    for (int i = 0; i < filas_locales + 2; i++) {
        free(dominio_local[i]);
        free(nuevo_dominio_local[i]);
    }
    free(dominio_local);
    free(nuevo_dominio_local);
    free(conteos);
    free(desplazamiento);

    MPI_Finalize();
    return 0;
}
