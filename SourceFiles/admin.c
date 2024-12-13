//admin.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>

#define PORT 8080
#define MOVIE_FILE "movies.txt"
#define USER_FILE "users.txt"

int sockfd;
sem_t file_sem; // Semaphore to ensure file operations aren't conflicted

// Function prototypes
void caller();
void create_seat_matrix(char *movie_name, char *movie_time);
void delete_seat_matrix(char *movie_name);
void update_movies();
void update_users();
int movie_exists(char *movie_name);
int user_exists(int user_id);
void connect_to_server();
void send_server_message(char *message);

// Function to connect to server via TCP
void connect_to_server() {
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection to server failed\n");
        exit(0);
    }
}

// Function to send a message to the server
void send_server_message(char *message) {
    send(sockfd, message, strlen(message), 0);
}

// Menu caller function
void caller() {
    int choice;
    while (1) {
        printf("\nAdmin Menu:\n1. Update Movies Database\n2. Update Users Database\n3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                update_movies();
                break;
            case 2:
                update_users();
                break;
            case 3:
                printf("Exiting...\n");
                close(sockfd);
                exit(0);
            default:
                printf("Invalid choice!\n");
        }
    }
}

// Function to create a new seat matrix in seat_matrix.txt
void create_seat_matrix(char *movie_name, char *movie_time) {
    FILE *file = fopen("seat_matrix.txt", "a");
    if (file == NULL) {
        printf("Error opening seat matrix file.\n");
        return;
    }

    fprintf(file, "%s %s\n", movie_name, movie_time); // Label the matrix with the movie name and timing
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 6; j++) {
            fprintf(file, "_ ");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n"); // Add spacing between different matrices for clarity
    fclose(file);
    printf("Seat matrix created for %s.\n", movie_name);
}

// Function to delete a seat matrix for a specific movie from seat_matrix.txt
void delete_seat_matrix(char *movie_name) {
    FILE *file = fopen("seat_matrix.txt", "r");
    FILE *temp = fopen("temp_seat_matrix.txt", "w");
    if (file == NULL || temp == NULL) {
        printf("Error opening seat matrix file.\n");
        if (file != NULL) fclose(file);
        if (temp != NULL) fclose(temp);
        return;
    }

    char line[256];
    int skip = 0;

    while (fgets(line, sizeof(line), file)) {
        // Check if line contains the movie name and start skipping lines until the end of this matrix
        if (strncmp(line, "Movie:", 6) == 0) {
            if (strstr(line, movie_name) != NULL) {
                skip = 1; // Start skipping this matrix
                continue;
            } else {
                skip = 0; // Not the matrix to delete
            }
        }

        // Copy lines to the temp file if not in the movie's matrix we want to delete
        if (!skip) {
            fputs(line, temp);
        }
    }

    fclose(file);
    fclose(temp);

    // Replace the original seat matrix file with the updated temp file
    remove("seat_matrix.txt");
    rename("temp_seat_matrix.txt", "seat_matrix.txt");
    printf("Seat matrix for %s deleted.\n", movie_name);
}

// Function to update the movies database
void update_movies() {
    int choice;
    printf("\nMovies Database:\n1. Add Movie\n2. Delete Movie\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);

    char movie_name[50], genre[20], rating[10], timing[10];
    int audience_score;

    if (choice == 1) { // Add a movie
        printf("Enter movie name: ");
        scanf("%s", movie_name);
        printf("Enter rating (PG-13, R, etc.): ");
        scanf("%s", rating);
        printf("Enter genre: ");
        scanf("%s", genre);
        printf("Enter audience score (0-100): ");
        scanf("%d", &audience_score);
        printf("Enter timing (e.g., 10am): ");
        scanf("%s", timing);

        sem_wait(&file_sem);  // Lock semaphore for file update
        FILE *file = fopen(MOVIE_FILE, "a");
        if (file == NULL) {
            printf("Error opening movies file.\n");
            sem_post(&file_sem);
            return;
        }
        
        // Write the new movie to the file
        fprintf(file, "%s %s %s %d %s\n", movie_name, rating, genre, audience_score, timing);
        fclose(file);
        sem_post(&file_sem);  // Unlock semaphore

        // Notify the server of the change
        char server_message[100];
        sprintf(server_message, "Admin added movie: %s (%s)", movie_name, timing);
        send_server_message(server_message);

        printf("Movie added successfully!\n");
        
        create_seat_matrix(movie_name, timing);
        
    } else if (choice == 2) { // Delete a movie
        printf("Enter movie name to delete: ");
        scanf("%s", movie_name);

        sem_wait(&file_sem);  // Lock semaphore for file update
        if (!movie_exists(movie_name)) {
            printf("Movie '%s' does not exist.\n", movie_name);
            sem_post(&file_sem);  // Unlock semaphore
            return;
        }

        // Read file content and exclude the movie to delete
        FILE *file = fopen(MOVIE_FILE, "r");
        FILE *temp = fopen("temp.txt", "w");
        if (file == NULL || temp == NULL) {
            printf("Error opening file.\n");
            sem_post(&file_sem);  // Unlock semaphore
            return;
        }

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            char name[50];
            sscanf(line, "%s", name);
            if (strcmp(name, movie_name) != 0) {
                fprintf(temp, "%s", line);  // Write all movies except the one to be deleted
            }
        }

        fclose(file);
        fclose(temp);
        remove(MOVIE_FILE);  // Delete original file
        rename("temp.txt", MOVIE_FILE);  // Rename temp to original
        
        delete_seat_matrix(movie_name);

        sem_post(&file_sem);  // Unlock semaphore

        // Notify the server of the change
        char server_message[100];
        sprintf(server_message, "Admin deleted movie: %s", movie_name);
        send_server_message(server_message);

        printf("Movie deleted successfully!\n");
    } else {
        printf("Invalid choice!\n");
    }
}

// Function to check if a movie exists in the file
int movie_exists(char *movie_name) {
    FILE *file = fopen(MOVIE_FILE, "r");
    if (file == NULL) {
        return 0;
    }

    char line[256], name[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s", name);
        if (strcmp(name, movie_name) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

// Function to update the users database
void update_users() {
    int choice;
    printf("\nUsers Database:\n1. Add User\n2. Delete User\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);

    int user_id;
    char user_name[50], password[50];

    if (choice == 1) { // Add a user
        printf("Enter user ID: ");
        scanf("%d", &user_id);
        printf("Enter user name: ");
        scanf("%s", user_name);
        printf("Enter password: ");
        scanf("%s", password);

        sem_wait(&file_sem);  // Lock semaphore for file update
        FILE *file = fopen(USER_FILE, "a");
        if (file == NULL) {
            printf("Error opening users file.\n");
            sem_post(&file_sem);  // Unlock semaphore
            return;
        }

        // Write the new user to the file
        fprintf(file, "%d %s %s\n", user_id, user_name, password);
        fclose(file);
        sem_post(&file_sem);  // Unlock semaphore

        // Notify the server of the change
        char server_message[100];
        sprintf(server_message, "Admin added user: %s (ID: %d)", user_name, user_id);
        send_server_message(server_message);

        printf("User added successfully!\n");
    } else if (choice == 2) { // Delete a user
        printf("Enter user ID to delete: ");
        scanf("%d", &user_id);

        sem_wait(&file_sem);  // Lock semaphore for file update
        if (!user_exists(user_id)) {
            printf("User with ID '%d' does not exist.\n", user_id);
            sem_post(&file_sem);  // Unlock semaphore
            return;
        }

        // Read file content and exclude the user to delete
        FILE *file = fopen(USER_FILE, "r");
        FILE *temp = fopen("temp.txt", "w");
        if (file == NULL || temp == NULL) {
            printf("Error opening file.\n");
            sem_post(&file_sem);  // Unlock semaphore
            return;
        }

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            int id;
            sscanf(line, "%d", &id);
            if (id != user_id) {
                fprintf(temp, "%s", line);  // Write all users except the one to be deleted
            }
        }

        fclose(file);
        fclose(temp);
        remove(USER_FILE);  // Delete original file
        rename("temp.txt", USER_FILE);  // Rename temp to original

        sem_post(&file_sem);  // Unlock semaphore

        // Notify the server of the change
        char server_message[100];
        sprintf(server_message, "Admin deleted user with ID: %d", user_id);
        send_server_message(server_message);

        printf("User deleted successfully!\n");
    } else {
        printf("Invalid choice!\n");
    }
}

// Function to check if a user exists in the file
int user_exists(int user_id) {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        return 0;
    }

    char line[256];
    int id;
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%d", &id);
        if (id == user_id) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int main() {
    sem_init(&file_sem, 0, 1);  // Initialize semaphore for file operations
    connect_to_server();  // Establish connection with the server
    caller();  // Show the admin menu and handle their actions

    return 0;
}
