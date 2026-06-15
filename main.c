/* Version mejorada de la mini shell. Añade dos cosas:
    - El prompt muestra la carpeta actual
    - Soporta redirección de salida con >
Por ejemplo:
    echo hola > salida.txt
en vez de imrpimir hola en pantalla, lo guarda en el archivo salida.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h> // se necesita para usar open() y constes como O_WRONLY
                   // O_CREAT O_TRUNC. Sirve para abrir achivos a bajo nivel

#define MAX_LINE 80
#define MAX_ARGS 10

int main(void) {
    char *args[MAX_ARGS];
    char linea[MAX_LINE];
    int ejecucion = 1;
    char ruta_actual[1024]; // Se guarda la ruta actual de la shell

    while (ejecucion) {
        // prompt con carpeta actual
        if (getcwd(ruta_actual, sizeof(ruta_actual)) != NULL) {
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

        // Comando interno: cd
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                fprintf(stderr, "mi_shell: se esperaba un argumento para \"cd\"\n");
            } else {
                if (chdir(args[1]) != 0) {
                    perror("mi_shell");
                }
            }
            continue;
        }

        // --- DETECTAR REDIRECCIÓN (>) ---
        char *archivo_salida = NULL;
        int redirigir = 0;

        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], ">") == 0) {
                if (args[j + 1] != NULL) {
                    archivo_salida = args[j + 1]; // El siguiente argumento es el archivo
                    args[j] = NULL; // Cortamos los argumentos ahí para que execvp no vea el '>'
                    redirigir = 1;
                } else {
                    fprintf(stderr, "mi_shell: error de sintaxis cerca del elemento inesperado '>'\n");
                }
                break;
            }
        }

        // Comandos externos (Fork)
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Fork failed\n");
            return 1;
        } 
        else if (pid == 0) {
            // --- ESTAMOS EN EL PROCESO HIJO ---
            if (redirigir) {
                // Abrimos el archivo. Si no existe, lo crea (O_CREAT). Si existe, lo vacía (O_TRUNC).
                // Con permisos de escritura (O_WRONLY) e introduciendo permisos estándar (0644).
                int fd = open(archivo_salida, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                /* open() abre un archivo y devuelve un descriptor de archivo
                echo hola > salida.txt sobrescribe salida.txt. No añade al final, lo remplaza 
                El 0644 son permisos del archivo. Es decir rw-r--r-- (read & write, read, read) */

                if (fd < 0) {
                    perror("mi_shell: error al abrir el archivo de salida");
                    exit(1);
                }
                // Duplicamos el descriptor del archivo en el canal 1 (stdout)
                dup2(fd, STDOUT_FILENO);
                // Aquí hace que el descriptor 1, que es stdout, apunte al archivo.
                close(fd); // Ya no necesitamos el descriptor original abierto
            }

            if (execvp(args[0], args) < 0) {
                fprintf(stderr, "mi_shell: comando no encontrado: %s\n", args[0]);
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

/* Flujo de ejemplo:
Comando --> echo hola > saludo.txt
Pasa esto:
1. La shell lee la línea.
2. La separa:
   args[0] = "echo"
   args[1] = "hola"
   args[2] = ">"
   args[3] = "saludo.txt"
   args[4] = NULL

3. Detecta ">".
4. Guarda:
   archivo_salida = "saludo.txt"

5. Corta args:
   args[0] = "echo"
   args[1] = "hola"
   args[2] = NULL

6. Hace fork().
7. En el hijo:
   abre saludo.txt
   redirige stdout hacia saludo.txt
   ejecuta echo hola

8. En el padre:
   espera a que termine el hijo.

9. El archivo saludo.txt contiene:
   hola
*/