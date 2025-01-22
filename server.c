// messaging_server.c
// Enhanced server for the messaging app with identities and direct messaging
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Client structure
typedef struct
{
    int socket;
    char username[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex;

// Broadcast message to all clients
void broadcast_message(const char *message, int sender_socket)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket != sender_socket)
        {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send a direct message to a specific client
void send_direct_message(const char *message, const char *recipient_username)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].username, recipient_username) == 0)
        {
            send(clients[i].socket, message, strlen(message), 0);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle client messages
void *client_handler(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char username[50];

    // Receive username
    if (recv(client_socket, username, sizeof(username), 0) <= 0)
    {
        close(client_socket);
        return NULL;
    }
    username[strcspn(username, "\n")] = 0; // Remove newline character

    // Add client to the list
    pthread_mutex_lock(&clients_mutex);
    clients[client_count].socket = client_socket;
    strncpy(clients[client_count].username, username, sizeof(clients[client_count].username));
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    printf("%s connected.\n", username);
    sprintf(buffer, "%s has joined the chat.\n", username);
    broadcast_message(buffer, client_socket);

    while (1)
    {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        // Check if the message is a direct message
        if (buffer[0] == '@')
        {
            char recipient[50];
            char message[BUFFER_SIZE];

            // Parse the recipient username and message
            sscanf(buffer, "@%s %[^\n]", recipient, message);

            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, sizeof(formatted_message), "[DM from %s]: %s\n", username, message);
            send_direct_message(formatted_message, recipient);
        }
        else
        {
            // Broadcast the message to all clients
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, sizeof(formatted_message), "[%s]: %s\n", username, buffer);
            broadcast_message(formatted_message, client_socket);
        }
    }

    // Remove client from the list
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket == client_socket)
        {
            for (int j = i; j < client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    sprintf(buffer, "%s has left the chat.\n", username);
    broadcast_message(buffer, client_socket);
    printf("%s disconnected.\n", username);

    close(client_socket);
    return NULL;
}

int main()
{
    int server_socket, client_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    pthread_mutex_init(&clients_mutex, NULL);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)))
    {
        printf("New connection accepted\n");

        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, (void *)new_sock) < 0)
        {
            perror("Thread creation failed");
            free(new_sock);
        }

        pthread_detach(client_thread);
    }

    if (client_socket < 0)
    {
        perror("Accept failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_destroy(&clients_mutex);
    close(server_socket);

    return 0;
}
