// Marina Barbosa Américo 201152509
// Joao Victor Fleming 
// Gabriel Tetzlaf Mansano  201150956
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define MAX_COMMANDS 10

void quit_signal_handler(int signum) {
    if(signum == SIGQUIT) { // se for SIGQUIT, termina o programa de maneira async-safe(eu acho)
        signal(signum, SIG_DFL);
        raise(signum);
        return;
     }
}

// funcao para executar os comandos dentro das pipes
void execute_command(char *comando, int input_fd, int output_fd) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // processo filho
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // separa comando de argumento
        char *args[64];
        int i = 0;

        char *token = strtok(comando, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        // executa o comando
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

int main() {
    char buffer[BUFFER_SIZE];
    char *token;
    char *args[BUFFER_SIZE];
    int num_commands = 0;

    // Registra os handlers dos sinais
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    
    while (1) {
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
            quit_signal_handler(SIGQUIT);
        }

        // Tratamento do comando cd separado
        if (strncmp(buffer, "cd", 2) == 0) {
            // Divide a entrada em tokens separados por espaco
            token = strtok(buffer, " ");

            // Salva o segundo token, que é o diretorio para onde deve mudar
            token = strtok(NULL, " ");

            // Se nao houver segundo token, ou se for "~", muda para o diretorio home do usuario
            if (token == NULL || strcmp(token, "~") == 0) {
                char *home_dir = getenv("HOME");
                if (chdir(home_dir) != 0) {
                    printf("Erro ao mudar para o diretorio home.\n");
                }
            }
            else { // Se houver um segundo token, tenta mudar para o diretorio especificado
                if (chdir(token) != 0) {
                    printf("Erro ao mudar para o diretorio %s.\n", token);
                }
            }

            continue;
        }
        
        // funcionalidade de pipe
        char *token = strtok(buffer, "|");
        while (token != NULL) {
            args[num_commands++] = token;
            token = strtok(NULL, "|"); // Separa o buffer em token separados por '|'
        }

        int fds[MAX_COMMANDS - 1][2];  // Descritor de arquivo pras pipes

        for (int i = 0; i < num_commands - 1; i++) {
            if (pipe(fds[i]) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        int input_fd = STDIN_FILENO;

        for (int i = 0; i < num_commands; i++) {
            int output_fd = STDOUT_FILENO;

            if (i < num_commands - 1) {
                output_fd = fds[i][1];
            }

            execute_command(args[i], input_fd, output_fd);

            if (i < num_commands - 1) {
                close(fds[i][1]);
                input_fd = fds[i][0];
            }
        }

        for (int i = 0; i < num_commands - 1; i++) {
            close(fds[i][0]);
            close(fds[i][1]);
        }

        for (int i = 0; i < num_commands; i++) {
            wait(NULL);
        }

        num_commands = 0;
        memset(buffer, 0, BUFFER_SIZE);
        memset(args, 0, sizeof(args));
    }
    return 0;
}