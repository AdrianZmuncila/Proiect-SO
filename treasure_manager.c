#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     // pentru unlink, symlink, write, read, close
#include <fcntl.h>      // pentru open, O_RDONLY, O_WRONLY 
#include <sys/stat.h>   // pentru mkdir, stat
#include <time.h>       // pentru time, localtime, strftime

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
#define MAX_CLUE_LEN 256

typedef struct {
    int id;
    char username[50];
    float latitude;
    float longitude;
    char clue[MAX_CLUE_LEN];
    int value;
} Treasure;

// ---  Log actiuni ---
void log_action(const char *hunt_id, const char *action) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_id, LOG_FILE);

    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("eroare la deschiderea fisierului de log");
        return;
    }

    dprintf(log_fd, "[%ld] %s\n", time(NULL), action);
    close(log_fd);
}

// ----------- Add treasure ---
void add_treasure(const char *hunt_id, Treasure treasure) {
    mkdir(hunt_id, 0755); // creeaza directorul daca nu exista

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Eroare la deschiderea fisierului de comori");
        return;
    }

    write(fd, &treasure, sizeof(Treasure));
    close(fd);

    // creare symlink pentru logged_hunt
    char log_symlink[256];
    snprintf(log_symlink, sizeof(log_symlink), "logged_hunt-%s", hunt_id);
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_id, LOG_FILE);
    symlink(log_path, log_symlink);

    log_action(hunt_id, "Comoara adaugata");
}

// ---- List treasures -----
void list_treasures(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    printf("Vanatoare: %s\n", hunt_id);

    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Eroare la obtinerea informatiilor despre fisier");
        return;
    }

    //afisarea timpului ultimei modificari
    char mod_time[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("Dimensiune fisier: %ld bytes\n", st.st_size);
    printf("Ultima modificare: %s\n", mod_time);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Eroare la deschiderea fisierului de comori");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) > 0) {
        printf("ID: %d, User: %s, Locatie: (%.2f, %.2f), Hint: %s, Valoare: %d\n",
               t.id, t.username, t.latitude, t.longitude, t.clue, t.value);
    }

    close(fd);
    log_action(hunt_id, "Vizualizare lista comori");
}

// ---- View treasure ------
void view_treasure(const char *hunt_id, int id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Eroare la deschiderea fisierului de comori");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) > 0) {
        if (t.id == id) {
            printf(">>> Comoara gasita:\n");
            printf("ID: %d\nUser: %s\nLocatie: (%.2f, %.2f)\nHint: %s\nValoare: %d\n",
                   t.id, t.username, t.latitude, t.longitude, t.clue, t.value);
            close(fd);
            log_action(hunt_id, "Vizualizare comoara aleasa");
            return;
        }
    }

    printf("Nu s-a gasit comoara cu ID-ul %d.\n", id);
    close(fd);
}

// ------Remove treasure ----
void remove_treasure(const char *hunt_id, int id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("eroare la deschiderea fisierului original");
        return;
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s/temp.dat", hunt_id);
    int temp_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    Treasure t;
    int found = 0;

    while (read(fd, &t, sizeof(Treasure)) > 0) {
        if (t.id != id) {
            write(temp_fd, &t, sizeof(Treasure));
        } else {
            found = 1;
        }
    }

    close(fd);
    close(temp_fd);

    if (found) {
        rename(temp_path, file_path);
        printf("comoara cu ID %d a fost stearsa.\n", id);
        log_action(hunt_id, "comoara stearsa");
    } else {
        unlink(temp_path);
        printf("nu s-a gasit comoara cu ID-ul %d.\n", id);
    }
}

// ----Remove entire hunt ----
void remove_hunt(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_id, LOG_FILE);

    unlink(file_path);
    unlink(log_path);

    // sterge director
    if (rmdir(hunt_id) == 0) {
        printf("Vanatoarea '%s' a fost stearsa complet.\n", hunt_id);
    } else {
        perror("Eroare la stergerea directorului");
    }

    // sterge symlink-ul
    char symlink_path[256];
    snprintf(symlink_path, sizeof(symlink_path), "logged_hunt-%s", hunt_id);
    unlink(symlink_path);
}

// ------main-----
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Utilizare: %s [--add | --list | --view | --remove_treasure | --remove_hunt] <hunt_id> [parametri]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0 && argc == 9) {
        Treasure t;
        t.id = atoi(argv[3]);
        strncpy(t.username, argv[4], sizeof(t.username) - 1);
        t.username[sizeof(t.username) - 1] = '\0';
        t.latitude = atof(argv[5]);
        t.longitude = atof(argv[6]);
        t.value = atoi(argv[7]);
        strncpy(t.clue, argv[8], sizeof(t.clue) - 1);
        t.clue[sizeof(t.clue) - 1] = '\0';

        add_treasure(argv[2], t);
    } else if (strcmp(argv[1], "--list") == 0) {
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "--view") == 0 && argc == 4) {
        view_treasure(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "--remove_treasure") == 0 && argc == 4) {
        remove_treasure(argv[2], atoi(argv[3]));
    } else if (strcmp(argv[1], "--remove_hunt") == 0) {
        remove_hunt(argv[2]);
    } else {
        fprintf(stderr, "comanda necunoscuta sau parametri insuficienti.\n");
        return 1;
    }

    return 0;
}

/*
gcc -o treasure_manager treasure_manager.c
./treasure_manager --add game1 1 user1 45.76 23.89 10 "primul indiciu"
./treasure_manager --list game1
./treasure_manager --view game1 1
./treasure_manager --remove_treasure game1 1
./treasure_manager --remove_hunt game1
*/