#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    char username[50];
    float latitude;
    float longitude;
    char clue[256];
    int value;
} Treasure;

typedef struct {
    char username[50];
    double total_score;
} UserScore;

UserScore* get_user_scores(const char* hunt_name, int* user_count) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt_name);
    
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open %s\n", path);
        return NULL;
    }

    Treasure t;
    UserScore* scores = malloc(sizeof(UserScore) * 100); // max 100 users
    int count = 0;

    while (fread(&t, sizeof(Treasure), 1, file) == 1) {
        // Check if user exists
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(scores[i].username, t.username) == 0) {
                scores[i].total_score += t.value;
                found = 1;
                break;
            }
        }

        if (!found && count < 100) {
            strncpy(scores[count].username, t.username, 50);
            scores[count].total_score = t.value;
            count++;
        }
    }

    fclose(file);
    *user_count = count;
    return scores;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_name>\n", argv[0]);
        return 1;
    }

    int user_count = 0;
    UserScore* scores = get_user_scores(argv[1], &user_count);

    if (!scores) return 1;

    printf("Scores for hunt '%s':\n", argv[1]);
   
    printf("User | Score\n");
    printf("-------------------\n");

    for (int i = 0; i < user_count; i++) {
        printf("%s | %.2f\n", scores[i].username, scores[i].total_score);
    }

    printf("---------------------\n");

    free(scores);
    return 0;
}
