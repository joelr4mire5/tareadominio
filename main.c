// Carga de librerías
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#define N 8  // Tamaño de la matriz 8x8

// Función para cargar un archivo con una matriz 8x8
void cargar_dominio(const char *filename, char domain[N][N]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("No se pudo abrir el archivo");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Leer el archivo y cargar cada valor en la matriz 'domain'
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            fscanf(file, " %c", &domain[i][j]);  // Espacio antes de %c para ignorar espacios en blanco
        }
    }

    fclose(file);
}

// Función para contar los vecinos próximos
void conteo_vecinos(char dominio[N][N], int coordenada_x, int coordenada_y, int *conteo_arbol, int *conteo_lago, int *conteo_desierto) {
    *conteo_arbol = *conteo_lago = *conteo_desierto = 0;

    // Revisar las 8 posiciones vecinas
    for (int desplazamiento_x = -1; desplazamiento_x <= 1; desplazamiento_x++) {
        for (int desplazamiento_y = -1; desplazamiento_y <= 1; desplazamiento_y++) {
            if (desplazamiento_x == 0 && desplazamiento_y == 0) continue;  // Saltar la celda actual

            int nueva_coordenada_x = coordenada_x + desplazamiento_x;
            int nueva_coordenada_y= coordenada_y + desplazamiento_y;

            // Verificar que el vecino esté dentro de los límites
            if (nueva_coordenada_x >= 0 && nueva_coordenada_x < N && nueva_coordenada_y >= 0 && nueva_coordenada_y < N) {
                if (dominio[nueva_coordenada_x][nueva_coordenada_y] == 'a') (*conteo_arbol)++;
                else if (dominio[nueva_coordenada_x][nueva_coordenada_y] == 'l') (*conteo_lago)++;
                else if (dominio[nueva_coordenada_x][nueva_coordenada_y] == 'd') (*conteo_desierto)++;
            }
        }
    }
}

// Función para aplicar las reglas y evolucionar el dominio un día
void aplicacion_reglas(char dominio[N][N], char actualizacion_dominio[N][N]) {
    int conteo_arbol, conteo_lago, conteo_desierto;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            conteo_vecinos(dominio, i, j, &conteo_arbol, &conteo_lago, &conteo_desierto);

            // Aplicar reglas basadas en el estado actual y los vecinos
            if (dominio[i][j] == 'a') {
                if (conteo_lago >= 4) actualizacion_dominio[i][j] = 'l';
                else if (conteo_arbol > 4) actualizacion_dominio[i][j] = 'd';
                else actualizacion_dominio[i][j] = 'a';
            }
            else if (dominio[i][j] == 'l') {
                if (conteo_lago < 3) actualizacion_dominio[i][j] = 'd';
                else actualizacion_dominio[i][j] = 'l';
            }
            else if (dominio[i][j] == 'd') {
                if (conteo_arbol >= 3) actualizacion_dominio[i][j] = 'a';
                else actualizacion_dominio[i][j] = 'd';
            }
        }
    }
}

// Función principal para simular varios días
void simulacion_dias(char dominio[N][N], int days) {
    char actualizacion_dominio[N][N];

    for (int day = 0; day < days; day++) {
        aplicacion_reglas(dominio, actualizacion_dominio);

        // Copiar el dominio actualizado al original para el próximo día
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                dominio[i][j] = actualizacion_dominio[i][j];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    char dominio[N][N];  // Matriz para almacenar el dominio completo
    int dias = 10;  // Número de días de simulación, ajustar según se desee

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Solo el proceso raíz (rank 0) lee el archivo y carga el dominio
    if (rank == 0) {
        cargar_dominio("data/input.txt", dominio);

        // Mostrar el dominio inicial en el proceso 0 (opcional)
        printf("Dominio inicial en proceso 0:\n");
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                printf("%c ", dominio[i][j]);
            }
            printf("\n");
        }
    }

    // Transmitir el número de días a todos los procesos
    MPI_Bcast(&dias, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Distribuir la matriz de dominio a todos los procesos (simplificado para matrices pequeñas)
    MPI_Bcast(dominio, N * N, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Cada proceso simula la evolución del dominio durante los días especificados
    simulacion_dias(dominio, dias);

    // Recolectar el dominio final en el proceso 0
    MPI_Gather(dominio, N * N / size, MPI_CHAR, dominio, N * N / size, MPI_CHAR, 0, MPI_COMM_WORLD);

    // El proceso 0 imprime o guarda el dominio final
    if (rank == 0) {
        printf("\nDominio final después de %d días:\n", dias);
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                printf("%c ", dominio[i][j]);
            }
            printf("\n");
        }
    }

    MPI_Finalize();
    return 0;
}
