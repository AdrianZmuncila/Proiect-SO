#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int pipefd[2]; // 0: read, 1: write
pid_t monitor_pid = -1;

void handle_sigusr1(int sig) {
    char buffer[256];
    int n = read(pipefd[0], buffer, sizeof(buffer));
    if (n > 0) {
        buffer[n] = '\0';

        if (strcmp(buffer, "list_hunts") == 0) {
            printf("---vanatori disponibile--\n");
            system("ls -d */ 2>/dev/null | grep -v '^logged_hunt' | sed 's#/##'");
        } else if (strncmp(buffer, "list_treasures ", 15) == 0) {
            char cmd[300];
            snprintf(cmd, sizeof(cmd), "./treasure_manager --list %s", buffer + 15);
            system(cmd);
        } else if (strncmp(buffer, "view_treasure ", 14) == 0) {
            char hunt[100];
            int id;
            sscanf(buffer + 14, "%s %d", hunt, &id);
            char cmd[300];
            snprintf(cmd, sizeof(cmd), "./treasure_manager --view %s %d", hunt, id);
            system(cmd);
        } else if (strcmp(buffer, "stop") == 0) {
            printf("monitorul se opreste...\n");
            exit(0);
        }
    }
}

void start_monitor() {
    if (pipe(pipefd) == -1) {
        perror("eroare la pipe");
        exit(1);
    }

    monitor_pid = fork();
    if (monitor_pid == -1) {
        perror("eroare la fork");
        exit(1);
    }

    if (monitor_pid == 0) {
        // procesul monitor
        close(pipefd[1]); // inchide scrierea
        signal(SIGUSR1, handle_sigusr1);
        while (1) pause();
    } else {
        // procesul principal
        close(pipefd[0]); // inchide citirea
        printf("monitorul a fost pornit cu pid %d\n", monitor_pid);
    }
}

void send_command(const char *cmd) {
    if (monitor_pid == -1) {
        printf("monitorul nu este pornit\n");
        return;
    }
    write(pipefd[1], cmd, strlen(cmd));
    kill(monitor_pid, SIGUSR1);
}

void stop_monitor() {
    if (monitor_pid != -1) {
        send_command("stop");
        waitpid(monitor_pid, NULL, 0);
        monitor_pid = -1;
    }
}

int main() {
    char line[256];

    while (1) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(line, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strncmp(line, "list_hunts", 10) == 0) {
            send_command("list_hunts");
        } else if (strncmp(line, "list_treasures ", 15) == 0) {
            send_command(line);
        } else if (strncmp(line, "view_treasure ", 14) == 0) {
            send_command(line);
        } else if (strcmp(line, "exit") == 0) {
            if (monitor_pid != -1) {
                printf("inchide mai intai monitorul cu 'stop_monitor'\n");
            } else {
                break;
            }
        } else {
            printf("comanda necunoscuta\n");
        }
    }

    return 0;
}
//start_monitor
//list_hunts
//list_treasure game1
//wiew_treasure game1 1
//stop_monitor
//exitSSS