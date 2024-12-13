//runner.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USER_FILE "users.txt"

// Function prototypes
void user_registration();
void login();
int count_users();
void run_user();
void run_admin();

// Function to count the number of users (to generate unique user IDs)
int count_users() {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        return 0;  // No users file yet, first registration
    }

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        count++;
    }
    fclose(file);

    return count;
}

// Function for user registration
void user_registration() {
    char username[50], password[50];

    printf("Enter a username: ");
    scanf("%s", username);
    printf("Enter a password: ");
    scanf("%s", password);

    int user_id = count_users();  // Assign user ID (starts from 1, since ID 0 is reserved for admin)

    FILE *file = fopen(USER_FILE, "a");
    if (file == NULL) {
        printf("Error opening users file.\n");
        return;
    }

    // Write the new user data into the users.txt file
    fprintf(file, "%d %s %s\n", user_id, username, password);
    fclose(file);

    printf("Registration successful! Your user ID is: %d\n", user_id);
}

// Function for user/admin login
void login() {
    char username[50], password[50];
    int user_id;
    int login_successful = 0;

    printf("Enter username: ");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", password);

    // Open users.txt to validate login
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        printf("Error: No users registered yet!\n");
        return;
    }

    char file_username[50], file_password[50];
    while (fscanf(file, "%d %s %s", &user_id, file_username, file_password) != EOF) {
        if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
            login_successful = 1;
            break;
        }
    }
    fclose(file);

    if (login_successful) {
        printf("Login successful!\n");
        if (user_id == 0) {
            printf("Admin login detected.\n");
            run_admin();  // Call admin function
        } else {
            printf("Welcome, User %d!\n", user_id);
            run_user();  // Call user function
        }
    } else {
        printf("Invalid username or password. Please try again.\n");
    }
}

// Function to run the user program (user.c)
void run_user() {
    // Execute the user.c program (using execvp or system call)
    char *args[] = {"./user", NULL};
    execvp(args[0], args);
    perror("Failed to launch user interface");
}

// Function to run the admin program (admin.c)
void run_admin() {
    // Execute the admin.c program (using execvp or system call)
    char *args[] = {"./admin", NULL};
    execvp(args[0], args);
    perror("Failed to launch admin interface");
}

// Main menu function
void caller() {
    int choice;

    while (1) {
        printf("\nMenu:\n1. Register\n2. Login\n3. Exit\n");
        printf("\nEnter your choice: ");
        scanf("%d", &choice);
        printf("\n");

        switch (choice) {
            case 1:
                user_registration();
                break;
            case 2:
                login();
                break;
            case 3:
                printf("Exiting...\n");
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

int main() {
    caller();  // Call the main menu
    return 0;
}