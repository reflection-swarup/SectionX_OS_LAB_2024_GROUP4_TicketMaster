//server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

// Function prototypes
void *handle_client(void *client_socket);

// Server main function
int main() {
    int server_fd, client_socket, addr_len;
    struct sockaddr_in address;
    pthread_t thread_id;

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    addr_len = sizeof(address);
    while (1) {
        // Accept a new client connection
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addr_len)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle each client
        if (pthread_create(&thread_id, NULL, handle_client, (void*)&client_socket) != 0) {
            perror("Thread creation failed");
            close(client_socket);
        }
    }

    close(server_fd);
    return 0;
}

// Function to handle each client
void *handle_client(void *client_socket) {
    int sock = *(int*)client_socket;
    char buffer[1024] = {0};
    int read_size;

    // Receive messages from the client
    while ((read_size = recv(sock, buffer, 1024, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received from client: %s\n", buffer);  // Print the message received from the client
        memset(buffer, 0, sizeof(buffer));  // Clear the buffer
    }

    if (read_size == 0) {
        printf("Client disconnected.\n");
    } else if (read_size == -1) {
        perror("Recv failed");
    }

    close(sock);
    return NULL;
}
