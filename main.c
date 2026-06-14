#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 80
#define MAX_ARGS 10

int main(void) {
    char *args[MAX_ARGS];
    char linea[MAX_LINE];
    int ejecucion = 1;
    char ruta_actual[1024];

    while (ejecucion) {
        // Conseguimos la ruta actual para mostrarla en el prompt de forma pro
        if (getcwd(ruta_actual, sizeof(ruta_actual)) != NULL) {
            // Buscamos el nombre de la última carpeta para que no sea un prompt larguísimo
            char *ultima_carpeta = strrchr(ruta_actual, '/');
            if (ultima_carpeta != NULL) {
                printf("mi_shell [%s]> ", ultima_carpeta + 1);
            } else {
                printf("mi_shell [%s]> ", ruta_actual);
            }
        } else {
            printf("mi_shell> ");
        }
        fflush(stdout);

        if (fgets(linea, MAX_LINE, stdin) == NULL) {
            break;
        }

        linea[strcspn(linea, "\n")] = 0;

        if (strlen(linea) == 0) {
            continue;
        }

        // 1. Comando interno: exit
        if (strcmp(linea, "exit") == 0) {
            ejecucion = 0;
            continue;
        }

        // Parsear la línea
        int i = 0;
        char *token = strtok(linea, " ");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        // 2. Comando interno: cd (¡LA SOLUCIÓN!)
        /* cd no puede ejecutarse igual que ls, echo o cat, porque cd tiene que cambiar 
        el directorio de trabajo de la propia shell. Por eso se ejecuta en el proceso 
        padre y se evita hacer fork() */
        
        if (strcmp(args[0], "cd") == 0) {
            // Si el usuario escribe solo 'cd', por ahora le avisamos
            if (args[1] == NULL) {
                fprintf(stderr, "mi_shell: se esperaba un argumento para \"cd\"\n");
            } else {
                // chdir() cambia el directorio del proceso actual (el padre)
                if (chdir(args[1]) != 0) {
                    perror("mi_shell"); // Muestra el error del sistema automáticamente
                }
            }
            continue; // Volvemos al inicio del bucle sin hacer fork
        }

        // 3. Comandos externos (Hacen fork)
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Fork failed\n");
            return 1;
        } 
        else if (pid == 0) {
            if (execvp(args[0], args) < 0) {
                printf("mi_shell: comando no encontrado: %s\n", args[0]);
                exit(1);
            }
        } 
        else {
            wait(NULL);
        }
    }
    
    printf("Saliendo de mi_shell. ¡Hasta luego!\n");
    return 0;
}