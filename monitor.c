#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>

volatile sig_atomic_t got_signal = 0;
char command_file[] = "monitor_cmd.txt";


void handle_usr1(int sig) {
    got_signal = 1;
}


void list_hunts_safe() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("Eroare la deschiderea directorului curent");
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

            printf("%s - %d comori\n", entry->d_name, count);
        }
    }

    closedir(dir);
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

    printf("[monitor] pornit. astept comenzi...\n");

    while (1) {
        pause();
        if (got_signal) {
            got_signal = 0;

            char cmd[256];
            FILE *f = fopen(command_file, "r");
            if (!f) {
                perror("nu pot citi comanda");
                continue;
            }
            if (!fgets(cmd, sizeof(cmd), f)) {
                fclose(f);
                continue;
            }
            fclose(f);

            cmd[strcspn(cmd, "\n")] = 0;

            if (strcmp(cmd, "stop_monitor") == 0) {
                printf("[monitor] oprire monitor... astept 2 secunde.\n");
                usleep(2000000);
                break;

            } else if (strcmp(cmd, "list_hunts") == 0) {
                list_hunts_safe();

            } else if (strncmp(cmd, "list_treasures", 14) == 0) {
                char hunt[100];
                if (sscanf(cmd, "list_treasures %99s", hunt) == 1) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        execlp("./treasure_manager", "treasure_manager", "--list", hunt, NULL);
                        perror("Nu am putut executa treasure_manager --list");
                        exit(1);
                    } else {
                        waitpid(pid, NULL, 0);
                    }
                } else {
                    printf("[monitor] comanda invalida: %s\n", cmd);
                }

            } else if (strncmp(cmd, "view_treasure", 13) == 0) {
                char hunt[100];
                int id;
                if (sscanf(cmd, "view_treasure %99s %d", hunt, &id) == 2) {
                    char id_str[10];
                    snprintf(id_str, sizeof(id_str), "%d", id);
                    pid_t pid = fork();
                    if (pid == 0) {
                        execlp("./treasure_manager", "treasure_manager", "--view", hunt, id_str, NULL);
                        perror("Nu am putut executa treasure_manager --view");
                        exit(1);
                    } else {
                        waitpid(pid, NULL, 0);
                    }
                } else {
                    printf("[monitor] comanda invalida: %s\n", cmd);
                }

            } else {
                printf("[monitor] comanda necunoscuta: %s\n", cmd);
            }
        }
    }

    printf("[monitor] terminat.\n");
    return 0;
}

