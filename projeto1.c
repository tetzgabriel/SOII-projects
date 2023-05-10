// Marina Barbosa Américo 201152509
// João Victor Fleming 
// Gabriel Tetzlaf Mansano  201150956
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 1024

volatile sig_atomic_t should_run = 1;

void signal_handler(int signum) {}

int main() {
    char buffer[BUFFER_SIZE];
    char *token;
    char *args[BUFFER_SIZE];
    char *args2[BUFFER_SIZE];

    // Registra os handlers dos sinais
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGQUIT, signal_handler);

    while (should_run) {
        char *user = getenv("USER"); // nome do usuario
        char host[100];
        gethostname(host, 100); // nome do hospedeiro
        char *dir = getcwd(NULL, 0); // diretorio atual
        char *home = getenv("HOME"); // diretorio de home
        char prompt[200];
        char *replace; // possivel caracter de ~ 
        if ((replace = strstr(dir, home)) != NULL) { // se o caminho for dentro de home
            char temp[200];
            sprintf(temp, "~%s", replace + strlen(home)); 
            sprintf(prompt, "%s@%s:%s$ ", user, host, temp);
        } else { //caminho fora de home
            sprintf(prompt, "%s@%s:%s$ ", user, host, dir);
        }
        printf("[MySh]%s", prompt);
        fgets(buffer, BUFFER_SIZE, stdin);

        // Remove a quebra de linha final do buffer
        buffer[strcspn(buffer, "\n")] = 0;

        // exit ou ctrl+d
        if (feof(stdin) || buffer[0] == EOF || strncmp(buffer, "exit", 4) == 0) {
            free(dir); // libera o que o getcwd ocupou
            return 0;
        }

        // Tratamento do comando cd
        if (strncmp(buffer, "cd", 2) == 0) {
            // Divide a entrada em tokens separados por espaço
            token = strtok(buffer, " ");

            // Salva o segundo token, que é o diretório para onde deve mudar
            token = strtok(NULL, " ");

            // Se não houver segundo token, ou se for "~", muda para o diretório home do usuário
            if (token == NULL || strcmp(token, "~") == 0) {
                char *home_dir = getenv("HOME");
                if (chdir(home_dir) != 0) {
                    printf("Erro ao mudar para o diretório home.\n");
                }
            }
            else { // Se houver um segundo token, tenta mudar para o diretório especificado
                if (chdir(token) != 0) {
                    printf("Erro ao mudar para o diretório %s.\n", token);
                }
            }

            continue;
        }

        // Verifica se a tecla Ctrl+Z ou Ctrl+C foi pressionada
        if (buffer[0] == 3 || buffer[0] == 26) {
            continue;
        }

        // Divide a entrada em tokens separados por espaço
        token = strtok(buffer, " ");
        int i = 0;
        int pipe_fd[2] = {-1, -1}; // Inicializa os descritores de arquivos do pipe

        // Preenche o vetor de argumentos
        while (token != NULL) {
            if (strcmp(token, "|") == 0) { // Verifica se o token é um pipe
                if (pipe(pipe_fd) == -1) { // Cria o pipe
                    printf("Erro ao criar o pipe.\n");
                    return -1;
                }
                break;
            }
            args[i++] = token;
            token = strtok(NULL, " ");
        }

        if (pipe_fd[0] != -1) { // Se o pipe foi criado
            // Cria um novo processo para executar o segundo comando
            pid_t pid2 = fork();
            pid_t pid = fork();

            if (pid2 < 0) { // Falha ao criar o processo
                printf("Erro ao criar processo filho.\n");
                return -1;
            }
            else if (pid2 == 0) { // Código do processo filho
                // Redireciona a entrada do processo para o pipe
                dup2(pipe_fd[0], 0);
                close(pipe_fd[1]);

                // Executa o segundo comando
                if (execvp(args2[0], args2) == -1) {
                    printf("Comando inválido.\n");
                }
                exit(0);
            }
            else { // Código do processo pai
                close(pipe_fd[0]);
                close(pipe_fd[1]);

                // Espera pelos dois processos filho terminarem
                waitpid(pid, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
        }
        else { // Se o pipe não foi criado
            // Cria um novo processo para executar o comando
            pid_t pid = fork();

            if (pid < 0) { // Falha ao criar o processo
                printf("Erro ao criar processo filho.\n");
                return -1;
            }
            else if (pid == 0) { // Código do processo filho
                if (execvp(args[0], args) == -1) {
                    printf("Comando inválido.\n");
                }
                exit(0);
            }
            else { // Código do processo pai
                wait(NULL);
            }
        }

    }
    return 0;
}