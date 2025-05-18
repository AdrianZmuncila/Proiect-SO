#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

pid_t monitor_pid = -1;
int monitor_running = 0;
char command_file[] = "monitor_cmd.txt";

// Descrierea capetelor pipe-ului: pipe_fd[0] = capatul de citire, pipe_fd[1] = capatul de scriere
int pipe_fd[2];

// Handler pentru terminarea procesului monitor
void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
        printf("[hub] monitorul s-a terminat cu status %d.\n", status);
        monitor_running = 0;
        monitor_pid = -1;
        
        // Inchidem capatul de citire al pipe-ului cand monitorul se termina
        close(pipe_fd[0]);
    }
}

// Trimite un semnal catre procesul monitor
void send_signal_to_monitor(int signal) {
    if (monitor_running) {
        kill(monitor_pid, signal);
    } else {
        printf("[eroare] monitorul nu ruleaza.\n");
    }
}

// Scrie comanda in fisierul de comenzi partajat cu monitorul
void write_command(const char *cmd) {
    int fd = open(command_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        dprintf(fd, "%s\n", cmd);
        close(fd);
    } else {
        perror("nu pot scrie in fisierul de comenzi");
    }
}

// Functie pentru a citi si afisa rezultatele transmise de monitor prin pipe
void read_from_monitor_pipe() {
    char buffer[4096];
    ssize_t bytes_read;
    
    // Setam pipe-ul pentru citire non-blocanta (ca sa nu blocheze daca nu exista date disponibile imediat)
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
    
    // Incercam sa citim datele trimise de monitor
    bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer); // Afisam continutul citit
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[256];

    // Cream un pipe anonim pentru comunicarea intre treasure_hub si monitor
    if (pipe(pipe_fd) == -1) {
        perror("Nu pot crea pipe-ul");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf(">> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_running) {
                printf("[eroare] monitorul deja ruleaza.\n");
                continue;
            }

            monitor_pid = fork();
            if (monitor_pid == 0) {
                // Procesul copil - monitorul

                // Inchidem capatul de citire (nu il folosim in monitor)
                close(pipe_fd[0]);

                // Transmitem capatul de scriere al pipe-ului catre monitor printr-o variabila de mediu
                char pipe_fd_str[16];
                sprintf(pipe_fd_str, "%d", pipe_fd[1]);
                setenv("MONITOR_PIPE_FD", pipe_fd_str, 1);

                // Lansam executia programului monitor
                execlp("./monitor", "monitor", NULL);
                perror("Eroare la exec monitor");
                exit(1);
            } else if (monitor_pid > 0) {
                // Procesul parinte

                monitor_running = 1;
                printf("[hub] monitorul a fost pornit cu PID %d.\n", monitor_pid);
                
                // Inchidem capatul de scriere in procesul parinte
                close(pipe_fd[1]);
            } else {
                perror("Eroare la fork pentru monitor");
            }

        } else if (strcmp(input, "stop_monitor") == 0 ||
                   strcmp(input, "list_hunts") == 0 ||
                   strncmp(input, "list_treasures", 14) == 0 ||
                   strncmp(input, "view_treasure", 13) == 0) {
            if (monitor_running) {
                // Scriem comanda in fisier si trimitem semnal
                write_command(input);
                send_signal_to_monitor(SIGUSR1);
                
                // Asteptam putin si apoi citim raspunsul din pipe
                usleep(100000); // 100ms delay pentru a permite monitorului sa scrie
                read_from_monitor_pipe();
            } else {
                printf("[eroare] monitorul nu este activ.\n");
            }

        } else if (strncmp(input, "calculate_score", 15) == 0) {
            // Comanda noua: calcularea scorului
            char hunt_name[100];
            if (sscanf(input, "calculate_score %99s", hunt_name) == 1) {
                pid_t score_pid = fork();
                
                if (score_pid == 0) {
                    // Proces copil care incearca sa ruleze score_calculator
                    
                    char path_buf[512];
                    getcwd(path_buf, sizeof(path_buf));
                    strcat(path_buf, "/score_calculator");
                    
                    execl(path_buf, "score_calculator", hunt_name, NULL);
                    execl("./score_calculator", "score_calculator", hunt_name, NULL);
                    execlp("score_calculator", "score_calculator", hunt_name, NULL);
                    
                    perror("Eroare la lansarea calculatorului de scor");
                    exit(1);
                } else if (score_pid > 0) {
                    int status;
                    waitpid(score_pid, &status, 0);
                    if (WIFEXITED(status)) {
                        printf("[hub] calculatorul de scor a terminat cu codul %d\n", WEXITSTATUS(status));
                    } else {
                        printf("[hub] calculatorul de scor a terminat anormal\n");
                    }
                } else {
                    perror("Eroare la fork pentru calculatorul de scor");
                }
            } else {
                printf("[eroare] sintaxa corecta: calculate_score <hunt_name>\n");
            }

        } else if (strcmp(input, "exit") == 0) {
            if (monitor_running) {
                printf("[eroare] monitorul inca ruleaza.\n");
            } else {
                break;
            }

        } else {
            printf("[eroare] comanda necunoscuta.\n");
        }
    }

    // Curatare finala: terminam monitorul si inchidem pipe-ul
    if (monitor_running) {
        kill(monitor_pid, SIGKILL);
        waitpid(monitor_pid, NULL, 0);
        close(pipe_fd[0]); // Inchidem capatul de citire al pipe-ului
    }

    return 0;
}

/*
   
    - start_monitor
    - list_hunts
    - list_treasures game1
    - view_treasure game1 1
    - calculate_score game1
    - stop_monitor
    - exit
*/
