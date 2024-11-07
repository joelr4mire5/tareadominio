#!/bin/bash
#SBATCH --job-name=simulacion_dominio    # Nombre del trabajo
#SBATCH --output=salida_simulacion.txt    # Archivo de salida
#SBATCH --error=error_simulacion.txt      # Archivo de errores
#SBATCH --ntasks=4                        # Número de tareas (procesos MPI)
#SBATCH --time=00:10:00                   # Tiempo límite de ejecución (10 minutos en este caso)
#SBATCH --partition=nu               # Tipo de partición (ajustar según Kabré)
#SBATCH --mail-type=END                   # Notificaciones al final del trabajo
#SBATCH --mail-user=joel.ramirez@ulead.ac.cr  # Tu correo para recibir notificaciones

# Cargar el módulo de MPI (ajustar si Kabré tiene un módulo específico)
module load mpi

# Ejecutar el programa con mpirun
mpirun -np 4 ./main data/input.txt  # Cambiar "mi_programa" al nombre de tu ejecutable
