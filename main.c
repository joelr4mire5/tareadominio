#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 8
#define COLS 8

int main(int argc, char *argv[]) {
    int rank, size;
    char domain[ROWS][COLS];
    int days;

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

    days = atoi(argv[2]);

    // El proceso 0 lee el archivo de entrada
    if (rank == 0) {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            printf("Error al abrir el archivo %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                fscanf(fp, " %c", &domain[i][j]);
            }
        }
        fclose(fp);
    }

    // Broadcast del dominio a todos los procesos
    MPI_Bcast(&domain[0][0], ROWS * COLS, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Calcular el número de filas por proceso
    int rows_per_proc = ROWS / size;
    int remainder = ROWS % size;

    int *counts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));

    int offset = 0;
    for (int i = 0; i < size; i++) {
        int rows = rows_per_proc;
        if (i < remainder) {
            rows++;
        }
        counts[i] = rows * COLS; // Número total de elementos
        displs[i] = offset;
        offset += counts[i];
    }

    int local_elements = counts[rank];
    int local_rows = local_elements / COLS;

    // Asignar memoria para datos locales
    char **local_domain = malloc((local_rows + 2) * sizeof(char *));
    char **new_local_domain = malloc((local_rows + 2) * sizeof(char *));
    for (int i = 0; i < local_rows + 2; i++) {
        local_domain[i] = malloc(COLS * sizeof(char));
        new_local_domain[i] = malloc(COLS * sizeof(char));
    }

    // Inicializar ghost rows
    memset(local_domain[0], 0, COLS * sizeof(char));
    memset(local_domain[local_rows + 1], 0, COLS * sizeof(char));

    // Scatter del dominio a todos los procesos
    MPI_Scatterv(&domain[0][0], counts, displs, MPI_CHAR,
                 &local_domain[1][0], local_elements, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Bucle de simulación
    for (int day = 0; day < days; day++) {
        // Intercambio de ghost rows con vecinos
        MPI_Request reqs[4];
        if (rank > 0) {
            // Enviar primera fila al proceso anterior
            MPI_Isend(local_domain[1], COLS, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, &reqs[0]);
            // Recibir ghost row del proceso anterior
            MPI_Irecv(local_domain[0], COLS, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &reqs[1]);
        } else {
            reqs[0] = MPI_REQUEST_NULL;
            reqs[1] = MPI_REQUEST_NULL;
        }
        if (rank < size - 1) {
            // Enviar última fila al siguiente proceso
            MPI_Isend(local_domain[local_rows], COLS, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &reqs[2]);
            // Recibir ghost row del siguiente proceso
            MPI_Irecv(local_domain[local_rows + 1], COLS, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD, &reqs[3]);
        } else {
            reqs[2] = MPI_REQUEST_NULL;
            reqs[3] = MPI_REQUEST_NULL;
        }

        // Esperar a que la comunicación termine
        MPI_Waitall(4, reqs, MPI_STATUSES_IGNORE);

        // Aplicar las reglas
        for (int i = 1; i <= local_rows; i++) {
            for (int j = 0; j < COLS; j++) {
                int adjacent_trees = 0;
                int adjacent_lakes = 0;

                // Verificar vecinos
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0)
                            continue;
                        int ni = i + di;
                        int nj = j + dj;
                        if (nj >= 0 && nj < COLS && ni >= 0 && ni < local_rows + 2) {
                            char neighbor = local_domain[ni][nj];
                            if (neighbor == 'a')
                                adjacent_trees++;
                            else if (neighbor == 'l')
                                adjacent_lakes++;
                        }
                    }
                }

                char current = local_domain[i][j];

                // Aplicar reglas
                if (current == 'a' && adjacent_lakes >= 4) {
                    new_local_domain[i][j] = 'l';
                } else if (current == 'l' && adjacent_lakes < 3) {
                    new_local_domain[i][j] = 'd';
                } else if (current == 'd' && adjacent_trees >= 3) {
                    new_local_domain[i][j] = 'a';
                } else if (current == 'a' && adjacent_trees > 4) {
                    new_local_domain[i][j] = 'd';
                } else {
                    new_local_domain[i][j] = current;
                }
            }
        }

        // Actualizar dominio local
        for (int i = 1; i <= local_rows; i++) {
            memcpy(local_domain[i], new_local_domain[i], COLS * sizeof(char));
        }
    }

    // Reunir los resultados
    MPI_Gatherv(&local_domain[1][0], local_rows * COLS, MPI_CHAR,
                &domain[0][0], counts, displs, MPI_CHAR, 0, MPI_COMM_WORLD);

    // El proceso 0 escribe el dominio final en el archivo
    if (rank == 0) {
        FILE *fp = fopen("final.txt", "w");
        if (fp == NULL) {
            printf("Error al abrir el archivo final.txt\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                fprintf(fp, "%c ", domain[i][j]);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }

    // Liberar memoria asignada
    for (int i = 0; i < local_rows + 2; i++) {
        free(local_domain[i]);
        free(new_local_domain[i]);
    }
    free(local_domain);
    free(new_local_domain);
    free(counts);
    free(displs);

    MPI_Finalize();
    return 0;
}
