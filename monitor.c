#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdarg.h>

// variabila globala care retine daca am primit semnal
volatile sig_atomic_t got_signal = 0;

// fisierul partajat cu hub-ul pentru comenzi
char command_file[] = "monitor_cmd.txt";

// descriptorul pentru scriere in pipe-ul catre treasure_hub
int pipe_write_fd = -1;

// buffer pentru colectarea output-ului inainte de trimitere
#define OUTPUT_BUFFER_SIZE 4096
char output_buffer[OUTPUT_BUFFER_SIZE];
size_t buffer_pos = 0;

// handler pentru semnalul SIGUSR1
void handle_usr1(int sig) {
    got_signal = 1;
}

// adauga text in buffer-ul de iesire folosind format
void buffer_output(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    size_t remaining = OUTPUT_BUFFER_SIZE - buffer_pos - 1;
    int written = vsnprintf(output_buffer + buffer_pos, remaining, format, args);
    
    if (written > 0 && written < remaining) {
        buffer_pos += written;
    }
    
    va_end(args);
}

// trimite continutul buffer-ului catre treasure_hub prin pipe
void send_buffer_to_main() {
    if (pipe_write_fd != -1 && buffer_pos > 0) {
        write(pipe_write_fd, output_buffer, buffer_pos);
        buffer_pos = 0;
    }
}

// afiseaza lista de hunt-uri existente (directoare cu treasure.dat)
void list_hunts_safe() {
    DIR *dir = opendir(".");
    if (!dir) {
        buffer_output("eroare la deschiderea directorului curent\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") != 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/treasures.dat", entry->d_name);

            FILE *f = fopen(path, "r");
            if (!f) continue;

            int count = 0;
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                count++;
            }
            fclose(f);

            buffer_output("%s - %d comori\n", entry->d_name, count);
        }
    }

    closedir(dir);
    send_buffer_to_main();
}

// executa o comanda externa si captureaza iesirea in buffer
void exec_and_capture(char *args[]) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        buffer_output("eroare la crearea pipe-ului\n");
        send_buffer_to_main();
        return;
    }
    
    pid_t child_pid = fork();
    if (child_pid == -1) {
        buffer_output("eroare la fork\n");
        close(pipefd[0]);
        close(pipefd[1]);
        send_buffer_to_main();
        return;
    }
    
    if (child_pid == 0) {
        // proces copil
        close(pipefd[0]); // inchidem capatul de citire
        dup2(pipefd[1], STDOUT_FILENO); // redirectionam stdout catre pipe
        close(pipefd[1]);
        execvp(args[0], args);
        perror("eroare la exec");
        exit(1);
    } else {
        // proces parinte
        close(pipefd[1]); // inchidem capatul de scriere

        char temp_buffer[1024];
        ssize_t bytes;
        
        while ((bytes = read(pipefd[0], temp_buffer, sizeof(temp_buffer) - 1)) > 0) {
            temp_buffer[bytes] = '\0';
            buffer_output("%s", temp_buffer);
        }
        
        close(pipefd[0]);
        waitpid(child_pid, NULL, 0);
        send_buffer_to_main();
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_usr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
    // obtinem descriptorul pipe-ului din variabila de mediu
    char* pipe_fd_str = getenv("MONITOR_PIPE_FD");
    if (pipe_fd_str) {
        pipe_write_fd = atoi(pipe_fd_str);
    }

    buffer_output("[monitor] pornit. astept comenzi...\n");
    send_buffer_to_main();

    while (1) {
        pause(); // asteapta semnalul
        if (got_signal) {
            got_signal = 0;

            char cmd[256];
            FILE *f = fopen(command_file, "r");
            if (!f) {
                buffer_output("nu pot citi comanda\n");
                send_buffer_to_main();
                continue;
            }

            if (!fgets(cmd, sizeof(cmd), f)) {
                fclose(f);
                continue;
            }

            fclose(f);
            cmd[strcspn(cmd, "\n")] = 0;

            // comanda: stop_monitor
            if (strcmp(cmd, "stop_monitor") == 0) {
                buffer_output("[monitor] oprire monitor... astept 2 secunde.\n");
                send_buffer_to_main();
                usleep(2000000);
                break;

            // comanda: list_hunts
            } else if (strcmp(cmd, "list_hunts") == 0) {
                list_hunts_safe();

            // comanda: list_treasures <hunt>
            } else if (strncmp(cmd, "list_treasures", 14) == 0) {
                char hunt[100];
                if (sscanf(cmd, "list_treasures %99s", hunt) == 1) {
                    char *args[] = {"./treasure_manager", "--list", hunt, NULL};
                    exec_and_capture(args);
                } else {
                    buffer_output("[monitor] comanda invalida: %s\n", cmd);
                    send_buffer_to_main();
                }

            // comanda: view_treasure <hunt> <id>
            } else if (strncmp(cmd, "view_treasure", 13) == 0) {
                char hunt[100];
                int id;
                if (sscanf(cmd, "view_treasure %99s %d", hunt, &id) == 2) {
                    char id_str[10];
                    snprintf(id_str, sizeof(id_str), "%d", id);
                    char *args[] = {"./treasure_manager", "--view", hunt, id_str, NULL};
                    exec_and_capture(args);
                } else {
                    buffer_output("[monitor] comanda invalida: %s\n", cmd);
                    send_buffer_to_main();
                }

            // comanda necunoscuta
            } else {
                buffer_output("[monitor] comanda necunoscuta: %s\n", cmd);
                send_buffer_to_main();
            }
        }
    }

    buffer_output("[monitor] terminat.\n");
    send_buffer_to_main();

    if (pipe_write_fd != -1) {
        close(pipe_write_fd);
    }

    return 0;
}

