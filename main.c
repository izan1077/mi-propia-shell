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

// Función auxiliar para parsear un comando simple (separa por espacios)
void parsear_comando(char *linea, char **args) {
    int i = 0;
    char *token = strtok(linea, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

int main(void) {
    char linea[MAX_LINE];
    int ejecucion = 1;
    char ruta_actual[1024];

    while (ejecucion) {
        if (getcwd(ruta_actual, sizeof(ruta_actual)) != NULL) {
            char *ultima_carpeta = strrchr(ruta_actual, '/');
            printf("mi_shell [%s]> ", ultima_carpeta ? ultima_carpeta + 1 : ruta_actual);
        } else {
            printf("mi_shell> ");
        }
        fflush(stdout);

        if (fgets(linea, MAX_LINE, stdin) == NULL) break;
        linea[strcspn(linea, "\n")] = 0;
        if (strlen(linea) == 0) continue;
        if (strcmp(linea, "exit") == 0) { ejecucion = 0; continue; }

        // --- DETECTAR SI HAY PIPE (|) ---
        char *parte_izquierda = NULL;
        char *parte_derecha = NULL;
        int hay_pipe = 0;

        char *pos_pipe = strchr(linea, '|');
        if (pos_pipe != NULL) {
            hay_pipe = 1;
            *pos_pipe = '\0'; // Cortamos la string original en dos partes
            parte_izquierda = linea;
            parte_derecha = pos_pipe + 1;
        }

        // Si NO hay pipe, ejecutamos el flujo normal de un solo comando
        if (!hay_pipe) {
            char *args[MAX_ARGS];
            parsear_comando(linea, args);

            // Comando interno: cd
            if (strcmp(args[0], "cd") == 0) {
                if (args[1] == NULL) fprintf(stderr, "mi_shell: argumento esperado\n");
                else if (chdir(args[1]) != 0) perror("mi_shell");
                continue;
            }

            // Redirección simple (>)
            char *archivo_salida = NULL;
            int redirigir = 0;
            for (int j = 0; args[j] != NULL; j++) {
                if (strcmp(args[j], ">") == 0) {
                    if (args[j + 1] != NULL) {
                        archivo_salida = args[j + 1];
                        args[j] = NULL;
                        redirigir = 1;
                    }
                    break;
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                if (redirigir) {
                    int fd = open(archivo_salida, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                if (execvp(args[0], args) < 0) {
                    printf("mi_shell: comando no encontrado: %s\n", args[0]);
                    exit(1);
                }
            }
            wait(NULL);
        } 
        else {
            // --- FLUJO CON PIPE (|) ---
            char *args1[MAX_ARGS];
            char *args2[MAX_ARGS];
            parsear_comando(parte_izquierda, args1);
            parsear_comando(parte_derecha, args2);

            int pipefd[2];
            if (pipe(pipefd) < 0) {
                perror("mi_shell: falló el pipe");
                continue;
            }

            // Fork para el PRIMER comando (escribe en el pipe)
            if (fork() == 0) {
                dup2(pipefd[1], STDOUT_FILENO); // Redirige stdout al tubo de escritura
                close(pipefd[0]); // El primer hijo no lee, cerramos lectura
                close(pipefd[1]); // Cerramos el original
                execvp(args1[0], args1);
                perror("mi_shell");
                exit(1);
            }

            // Fork para el SEGUNDO comando (lee del pipe)
            if (fork() == 0) {
                dup2(pipefd[0], STDIN_FILENO); // Redirige stdin al tubo de lectura
                close(pipefd[1]); // El segundo hijo no escribe, cerramos escritura
                close(pipefd[0]); // Cerramos el original
                execvp(args2[0], args2);
                perror("mi_shell");
                exit(1);
            }

            // El proceso padre (la shell) cierra ambos extremos para no bloquearse
            close(pipefd[0]);
            close(pipefd[1]);

            // Espera a que terminen ambos hijos
            wait(NULL);
            wait(NULL);
        }
    }
    printf("Saliendo de mi_shell. ¡Hasta luego!\n");
    return 0;
}

/* Flujo de ejemplo de redireccion de salida:
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

/* Ejemplo de comando: ls | grep txt
1. La shell lee: "ls | grep txt"
2. Encuentra "|"
3. Parte la línea:
   izquierda: "ls "
   derecha: " grep txt"

4. Parseo:
   args1 = ["ls", NULL]
   args2 = ["grep", "txt", NULL]

5. Crea un pipe:
   pipefd[0] = lectura
   pipefd[1] = escritura

6. Crea el primer hijo:
   stdout de ls -> pipefd[1]
   ejecuta ls

7. Crea el segundo hijo:
   stdin de grep <- pipefd[0]
   ejecuta grep txt

8. El padre cierra ambos extremos del pipe

9. El padre espera a ambos hijos

10. Resultado:
   grep muestra en pantalla solo las líneas de ls que contienen "txt"
*/