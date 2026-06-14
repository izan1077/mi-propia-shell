#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// para funciones POSIX como fork() o execvp()
#include <sys/wait.h>
// permite usar wait()

#define MAX_LINE 80 /* La longitud máxima del comando */
#define MAX_ARGS 10 /* Máximo número de argumentos */

int main(void) {
    char *args[MAX_ARGS]; /* Array de strings para los argumentos del comando */
    char linea[MAX_LINE];
    int ejecucion = 1;

    while (ejecucion) {
        printf("mi_shell> ");
        fflush(stdout);

        // 1. LEER el comando del usuario
        if (fgets(linea, MAX_LINE, stdin) == NULL) {
            break; // Si hay error o EOF (Ctrl+D), salimos
        }
        // fgets(destino, tamaño_maximo, origen)

        // Eliminar el salto de línea al final
        linea[strcspn(linea, "\n")] = 0;

        // Si el usuario pulsa intro sin escribir nada, volvemos a empezar
        if (strlen(linea) == 0) {
            continue;
        }

        // Comando especial para salir
        if (strcmp(linea, "exit") == 0) {
            ejecucion = 0;
            continue;
        }

        // 2. ANALIZAR (Parsear) la línea para separar el comando de sus argumentos
        int i = 0;
        char *token = strtok(linea, " "); // Separamos por espacios
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }
        args[i] = NULL; // El último argumento siempre debe ser NULL para execvp

        // 3. EJECUTAR creando un proceso hijo
        pid_t pid = fork();

        if (pid < 0) {
            // Error al hacer el fork
            fprintf(stderr, "Error al duplicar el proceso (Fork failed)\n");
            return 1;
        } 
        else if (pid == 0) {
            // --- ESTAMOS EN EL PROCESO HIJO ---
            // Intentamos ejecutar el comando que está en args[0]
            if (execvp(args[0], args) < 0) {
                printf("mi_shell: comando no encontrado: %s\n", args[0]);
                exit(1); // Si falla, el hijo muere con error
            }
        } 
        else {
            // --- ESTAMOS EN EL PROCESO PADRE ---
            // El padre espera a que el hijo termine antes de volver a mostrar el prompt
            wait(NULL);
        }
    }
    
    printf("Saliendo de mi_shell. ¡Hasta luego!\n");
    return 0;
}