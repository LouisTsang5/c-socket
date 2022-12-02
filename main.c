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
    socklen_t sock_len;
    struct sockaddr sock_addr;
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
    // Create the buffers to hold the address info
    struct conn_info *p_conn_info = malloc(sizeof(struct conn_info));

    // bzero(&client, clientLen);
    p_conn_info->conn_fd = accept(sock_fd, &(p_conn_info->sock_addr), &(p_conn_info->sock_len));
    if (p_conn_info->conn_fd < 0)
    {
        perror("Failed to accept connection\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted (fd: %d)\n", p_conn_info->conn_fd);
    }

    // Return a pointer to the client addr
    return p_conn_info;
}

void handle_conn(int conn_fd, size_t buffer_size)
{
    // Read from buffer
    char *p_buffer = malloc(buffer_size);
    for (;;)
    {
        // Read from soket into buffer
        read(conn_fd, p_buffer, buffer_size);

        // Check if exit command has been received
        if (strncmp("exit", p_buffer, 4) == 0)
        {
            printf("Exit command received. Exiting...\n");
            close(conn_fd);
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

    // Accept the connection
    struct conn_info *p_conn_info = accept_conn(sock_fd);

    // Handle the connection
    handle_conn(p_conn_info->conn_fd, BUFFER_SIZE);

    // Close the socket and free used memory
    close(sock_fd);
    free(p_conn_info);

    exit(EXIT_SUCCESS);
}