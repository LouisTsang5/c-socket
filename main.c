#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define QUEUE 5
#define BUFFER_SIZE 512

struct conn_info
{
    int conn_fd;
    socklen_t info_len;
    struct sockaddr_in conn_info;
};

int create_socket_and_listen(int port, int queue_size)
{
    // Create file descriptor for socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        perror("Error: Failed to create socket file descriptor\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket file descriptor (%d) created\n", socket_fd);
    }

    // Socket Address Info
    struct sockaddr_in sockaddr_in;           // Create struct
    bzero(&sockaddr_in, sizeof(sockaddr_in)); // Zero out the memery of the struct
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);
    sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to address
    int bindResult = bind(socket_fd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
    if (bindResult != 0)
    {
        char errMsg[50];
        sprintf(errMsg, "Error: Failed to bind scoket to PORT %d\n", PORT);
        perror(errMsg);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket binded to PORT %d\n", PORT);
    }

    // Listen on socket
    int listenResult = listen(socket_fd, queue_size);
    if (listenResult != 0)
    {
        perror("Error: Failed to listen on socket\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket listening...\n");
    }

    // Return file descriptor of the socket
    return socket_fd;
}

struct conn_info *accept_conn(int sock_fd)
{
    // Create the buffers to hold the connection info
    struct conn_info *p_conn_info = malloc(sizeof(struct conn_info));
    p_conn_info->info_len = sizeof(struct sockaddr_in);
    p_conn_info->conn_fd = accept(sock_fd, (struct sockaddr *)&(p_conn_info->conn_info), &(p_conn_info->info_len));
    if (p_conn_info->conn_fd < 0)
    {
        perror("Failed to accept connection\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted (fd: %d, address: %s, port: %u)\n", p_conn_info->conn_fd, inet_ntoa(p_conn_info->conn_info.sin_addr), p_conn_info->conn_info.sin_port);
    }

    // Return a pointer to the client addr
    return p_conn_info;
}

void handle_conn(struct conn_info *p_conn_info, size_t buffer_size)
{
    // Read from buffer
    char *p_buffer = malloc(buffer_size);
    for (;;)
    {
        // Read from soket into buffer
        read(p_conn_info->conn_fd, p_buffer, buffer_size);

        // Check if exit command has been received
        if (strncmp("exit", p_buffer, 4) == 0)
        {
            printf("Exit command received. Exiting...\n");
            close(p_conn_info->conn_fd);
            break;
        }

        // Print the message and clear the buffer
        printf("Message: %s", p_buffer);
        bzero(p_buffer, buffer_size);
    }
    free(p_buffer);
}

int main(int argc, char **argv)
{
    // Create a socket, bind, and listen
    int sock_fd = create_socket_and_listen(PORT, QUEUE);

    for (;;)
    {
        // Accept the connection
        struct conn_info *p_conn_info = accept_conn(sock_fd);

        // Handle the connection
        handle_conn(p_conn_info, BUFFER_SIZE);

        // Free the memory
        free(p_conn_info);
    }

    // Close the socket and free used memory
    close(sock_fd);

    exit(EXIT_SUCCESS);
}