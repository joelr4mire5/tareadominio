// carga de librerias
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#define N 8  // Tamaño de la matriz 8x8


// Funcion para cargar un archivo con una matriz 8x8

// Función para leer el archivo y cargar el dominio en una matriz
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

int main(int argc, char *argv[]) {
    int rank, size;
    char dominio[N][N];  // Matriz para almacenar el dominio completo

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Solo el proceso raíz (rank 0) lee el archivo y carga el dominio
    if (rank == 0) {
        cargar_dominio("data/input.txt", dominio);

        // Opcional: Mostrar el dominio cargado en el proceso 0
        printf("Dominio inicial en proceso 0:\n");
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
