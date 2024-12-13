//user.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#define MAX_MOVIES 100
#define ROWS 5
#define COLS 6
#define SEAT_FILE "seat_matrix.txt"
#define PORT 8080

sem_t seat_sem;  // Semaphore for seat booking
int sockfd;
pthread_t timeout_thread;
volatile int timeout_flag = 0;


typedef struct {
    char name[50];
    char genre[20];
    char rating[10];
    int audience_score;
    char timing[10];
    char seat_matrix[ROWS][COLS];
    int booked_seats[30];  // Array to hold seat numbers of booked seats
    int seat_count;        // Number of seats booked in this session
} Movie;

Movie movies[MAX_MOVIES];

// Function prototypes
int filters();
void book_movie_seats();
void display_bookings();
void load_seat_matrix(Movie *movie);
void display_seat_matrix(Movie* movie);
void update_seat_matrix(Movie *movie, int row, int col);
void generate_ticket(Movie *movie, int amount);
void dynamic_pricing(int booked_seats, int total_seats, float *price);
void connect_to_server();
void send_server_message(char *message);
void* timeout_handler(void* arg);

void caller() {
    int choice;
    while (1) {
        printf("\nMenu:\n1. Book Movie Tickets\n2. Display My Bookings\n3. Exit\n\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        printf("\n");
        
        switch (choice) {
            case 1:
                book_movie_seats();
                break;
            case 2:
                display_bookings();
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

// Timeout thread function
void* timeout_handler(void* arg) {
    sleep(120);  // Wait for 2 minutes (120 seconds)
    timeout_flag = 1;  // Set timeout flag if the user hasn't entered their ID
    pthread_exit(NULL); // Exit the thread
}

int filters() {
    char genre[20], rating[10];
    int score;
    
    printf("Enter preferred genre: ");
    scanf("%s", genre);
    printf("Enter preferred rating (PG-13, R, etc.): ");
    scanf("%s", rating);
    printf("Enter minimum audience score (0-100): ");
    scanf("%d", &score);
    
    // Fetching movie list based on filters
    FILE *file = fopen("movies.txt", "r");
    if (file == NULL) {
        printf("Error opening movies file\n");
        return -1;
    }
    
    int movie_count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        Movie movie;
        sscanf(line, "%s %s %s %d %s", movie.name, movie.rating, movie.genre, &movie.audience_score, movie.timing);
        
        if (strcmp(movie.genre, genre) == 0 && strcmp(movie.rating, rating) == 0 && movie.audience_score >= score) {
            movies[movie_count++] = movie;  
        }
    }
    
    fclose(file);
    
    // Display available movies
    if (movie_count == 0) {
        printf("\nNo movies found based on your filters!\n");
        return 0;
    }
    
    printf("\nAvailable movies:\n");
    for (int i = 0; i < movie_count; i++) {
        printf("%d. %s (%s, %d%% audience score) - %s\n", i + 1, movies[i].name, movies[i].rating, movies[i].audience_score, movies[i].timing);
    }
    printf("\n");
    
    return movie_count;
}

//Function to load movie seat matrix from file
void load_seat_matrix(Movie *movie) {
    FILE *file = fopen(SEAT_FILE, "r");
    if (file == NULL) {
        printf("Error opening seat matrix file.\n");
        return;
    }

    char line[256], name[50];
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s", name);
        if (strcmp(name, movie->name) == 0) {
            found = 1;
            for (int i = 0; i < ROWS; i++) {
                fgets(line, sizeof(line), file);
                sscanf(line, "%c %c %c %c %c %c",&movie->seat_matrix[i][0], &movie->seat_matrix[i][1], 
                &movie->seat_matrix[i][2], 
                &movie->seat_matrix[i][3], &movie->seat_matrix[i][4], &movie->seat_matrix[i][5]);
            }
            break;
        }
    }
    fclose(file);

    if (!found) {
        printf("No seat matrix found for %s.\n", movie->name);
    }
}

//Function to display seat matrix
void display_seat_matrix(Movie* movie) {
    printf("\nSeat Matrix for %s:\n\n", movie->name);

    const char* aisle = "AISLE";  // Vertical text to display between sections
    int aisle_index = 0;          // Tracks the letter in "AISLE"

    int count = 1;                // Seat numbering starts at 1
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (j == COLS / 2) {  // Central aisle before middle column
                if (i <= 1)
                    printf("      | %c |   ", aisle[aisle_index]);
                else
                    printf("   | %c |   ", aisle[aisle_index]);
                aisle_index++;
            }

            printf("%d.%c ", count, movie->seat_matrix[i][j]);
            count++;
        }
        printf("\n");
    }
}

//Function to update seat matrix
void update_seat_matrix(Movie *movie, int row, int col) {
    if (row >= ROWS || col >= COLS || row < 0 || col < 0) {
        printf("Invalid or already booked seat!\n");
        return;
    }
    FILE *file = fopen(SEAT_FILE, "r+");
    char line[256];
    long position = -1;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, movie->name) != NULL) {
            position = ftell(file);
            break;
        }
    }

    if (position == -1) {
        printf("Movie not found in seat matrix file.\n");
        fclose(file);
        return;
    }

    fseek(file, position, SEEK_SET);

    for (int i = 0; i < ROWS; i++) {
        fprintf(file, "%c %c %c %c %c %c\n", movie->seat_matrix[i][0], movie->seat_matrix[i][1], movie->seat_matrix[i][2], 
        movie->seat_matrix[i][3], movie->seat_matrix[i][4], movie->seat_matrix[i][5]);
    }
    fclose(file);
}

// Function to display the movie ticket
void generate_ticket(Movie *movie, int amount) {
    printf("\n----------------------------------\n");
    printf("           MOVIE TICKET           \n");
    printf("----------------------------------\n");
    printf("Movie Name : %s\n", movie->name);
    printf("Timing     : %s\n", movie->timing);
    printf("Genre      : %s\n", movie->genre);
    printf("Rating     : %s\n", movie->rating);
    printf("Amount     : %d\n", amount);
    printf("\n");
    printf("Booked Seats: ");
    if (movie->seat_count > 0) {
        for (int i = 0; i < movie->seat_count; i++) {
            printf("%d", movie->booked_seats[i]);
            if (i < movie->seat_count - 1) {
                printf(", ");  // Print comma for all but the last seat
            }
        }
        
    } else {
        printf("None");
    }
    printf("\n----------------------------------\n");
}


// Main function to handle seat booking
void book_movie_seats() {
    int movie_count = 0, movie_choice, num_seats, user_id, booked_seats = 0;
    char selected_seats[100];
    int row, col = 0;

    movie_count = filters();  // Display filtered movie options

    // Get user’s movie choice based on displayed list
    printf("Enter the movie number to book: ");
    scanf("%d", &movie_choice);

    // Validate movie choice
    if (movie_choice < 1 || movie_choice > movie_count) {
        printf("Invalid movie choice!\n");
        return;
    }

    Movie movie = movies[movie_choice - 1];  // Select the chosen movie
    load_seat_matrix(&movie);  // Load the current seat matrix
    display_seat_matrix(&movie);
    printf("\n");

    // Count already booked seats in the matrix
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (movie.seat_matrix[i][j] == 'X') booked_seats++;
        }
    }

    // Calculate dynamic price based on booking percentage
    float price = 100.0;
    dynamic_pricing(booked_seats, ROWS * COLS, &price);
    printf("Current price per seat: %.2f\n", price);
    
    printf("Enter number of seats to book: ");
    scanf("%d", &num_seats);
    printf("Enter seat numbers to book (e.g., 1,2,3,...): ");
    scanf("%s", selected_seats);

    // Lock semaphore for booking and launch timeout handler
    sem_wait(&seat_sem);
    pthread_create(&timeout_thread, NULL, timeout_handler, NULL);
    
    movie.seat_count = 0;

    // Book selected seats
    char *seat_token = strtok(selected_seats, ",");
    while (seat_token != NULL) {
        int seat_num = atoi(seat_token) - 1;
        row = seat_num / COLS;
        col = seat_num % COLS;

        // Validate seat availability
        if (row >= ROWS || col >= COLS || movie.seat_matrix[row][col] == 'X' || movie.seat_matrix[row][col] == 'R') {
            printf("Seat is invalid, booked or reserved: %d.\n", seat_num + 1);
            sem_post(&seat_sem);  // Unlock semaphore
            return;
        }
        
        movie.seat_matrix[row][col] = 'R';  // Mark seat as reserved
        
        // Store seat number in booked_seats array and increment seat_count
        movie.booked_seats[movie.seat_count++] = seat_num + 1;

        seat_token = strtok(NULL, ",");
    }

    update_seat_matrix(&movie, row, col);  // Update seat matrix file with reserved seats

    // Prompt user ID for payment
    printf("\nEnter User ID within 2 minutes to confirm payment: ");
    scanf("%d", &user_id);

    // Handle timeout condition
    if (timeout_flag) {
        printf("Booking timeout! You took too long to enter your User ID.\n");
        // Revert reserved seats to empty
        for (int i = 0; i < movie.seat_count; i++) {
            int booked_seat = movie.booked_seats[i] - 1; // Convert to 0-based index
            int revert_row = booked_seat / COLS;
            int revert_col = booked_seat % COLS;
            movie.seat_matrix[revert_row][revert_col] = '_';  // Change 'R' to '_'
        }
        update_seat_matrix(&movie, row, col);  // Update the seat matrix file
        sem_post(&seat_sem);  // Release semaphore
        return;
    }

    pthread_cancel(timeout_thread);
    pthread_join(timeout_thread, NULL);

    // Change 'R' to 'X' for successfully booked seats
    for (int i = 0; i < movie.seat_count; i++) {
        int booked_seat = movie.booked_seats[i] - 1; // Convert to 0-based index
        int confirm_row = booked_seat / COLS;
        int confirm_col = booked_seat % COLS;
        movie.seat_matrix[confirm_row][confirm_col] = 'X';  // Mark seat as booked
    }
    update_seat_matrix(&movie, row, col);  // Update the seat matrix file

    printf("Booking successful!\n\n");
    
    generate_ticket(&movie, num_seats * price);  // Display the ticket

    // Log the booking details
    FILE *bookings_file = fopen("bookings.txt", "a");
    if (bookings_file) {
        char seats[100] = "";  // Initialize an empty string for booked seats
        for (int i = 0; i < movie.seat_count; i++) {
            // Append the booked seat number to the seats string
            char seat_str[4];  // Temporary string to hold the seat number
            sprintf(seat_str, "%d", movie.booked_seats[i]);  // Convert seat number to string
            strcat(seats, seat_str);  // Concatenate the seat number to the seats string

            // Add a comma if it's not the last seat
            if (i != movie.seat_count - 1) {
                strcat(seats, ",");
            }
        }

        // Now log the booking details with the complete seats string
        fprintf(bookings_file, "%d %s %s %s %.2f\n", user_id, movie.name, movie.timing, seats, num_seats * price);
        fclose(bookings_file);
    }
    
    // Notify server
    char server_message[100];
    sprintf(server_message, "User %d booked %d seats for %s at %s", user_id, num_seats, movie.name, movie.timing);
    send_server_message(server_message);

    sem_post(&seat_sem);  // Release semaphore
}



void dynamic_pricing(int booked_seats, int total_seats, float *price) {
    float percentage = (float)booked_seats / total_seats * 100;
    if (percentage >= 75) {
        *price += (*price * 0.30);
    } else if (percentage >= 50) {
        *price += (*price * 0.20);
    } else if (percentage >= 25) {
        *price += (*price * 0.10);
    }
}

void display_bookings() {
    int user_id;
    printf("Enter your User ID: ");
    scanf("%d", &user_id);
    
    FILE *file = fopen("bookings.txt", "r");
    if (file == NULL) {
        printf("Error opening bookings file\n");
        return;
    }
    
    char line[256];
    printf("Your bookings:\n");
    while (fgets(line, sizeof(line), file)) {
        int uid;
        char movie_name[50], timing[10], seats[50];
        float amount_paid;
        
        sscanf(line, "%d %s %s %s %f", &uid, movie_name, timing, seats, &amount_paid);
        if (uid == user_id) {
            printf("Movie: %s, Timing: %s, Seats: %s, Amount Paid: %.2f\n", movie_name, timing, seats, amount_paid);
        }
    }
    
    fclose(file);
}

// Function to handle socket connection with server
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

void send_server_message(char *message) {
    send(sockfd, message, strlen(message), 0);
}

int main() {
    sem_init(&seat_sem, 0, 1);  // Initialize semaphore
    
    connect_to_server();
    caller();  // Display menu and handle user actions
    
    return 0;
}
