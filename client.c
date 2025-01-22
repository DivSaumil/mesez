// messaging_client.c
// Simple client for the messaging app with identities
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char username[50];

    // Get username
    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline character

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sock);
        return EXIT_FAILURE;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return EXIT_FAILURE;
    }

    // Send username to the server
    send(sock, username, strlen(username), 0);

    printf("Connected to the server as %s. Type your messages below:\n", username);

    fd_set readfds;
    int max_fd = sock;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select error");
            break;
        }

        // Check for user input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

            if (strncmp(buffer, "@", 1) == 0) {
                // Direct message format: @username message
                send(sock, buffer, strlen(buffer), 0);
            } else {
                // General chat message
                send(sock, buffer, strlen(buffer), 0);
            }
        }

        // Check for server messages
        if (FD_ISSET(sock, &readfds)) {
            int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[bytes_received] = '\0';
            printf("%s\n", buffer);
        }
    }

    close(sock);
    return 0;
}
