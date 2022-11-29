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

void printBytes(uint8_t *byte_start, size_t byte_len);

int main(int argc, char **argv)
{
    printf("Hello World!\n");

    // Create file descriptor for socket
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0)
    {
        perror("Error: Failed to create socket file descriptor\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket file descriptor (%d) created\n", socketFd);
    }

    // Socket Address Info
    struct sockaddr_in sockaddr_in;           // Create struct
    bzero(&sockaddr_in, sizeof(sockaddr_in)); // Zero out the memery of the struct
    struct in_addr in_addr;
    bzero(&in_addr, sizeof(in_addr));
    in_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(PORT);
    sockaddr_in.sin_addr = in_addr;

    // Bind socket to address
    int bindResult = bind(socketFd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
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
    int listenResult = listen(socketFd, QUEUE);
    if (listenResult != 0)
    {
        perror("Error: Failed to listen on socket\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket listening...\n");
    }

    // Accept connection
    struct sockaddr client;
    unsigned int clientLen = sizeof(client);
    bzero(&client, clientLen);
    int connFd = accept(socketFd, &client, &clientLen);
    if (connFd < 0)
    {
        perror("Failed to accept connection\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted\n");
    }

    // Read from buffer
    char buffer[BUFFER_SIZE];
    bzero(&buffer, BUFFER_SIZE);
    for (;;)
    {
        // Read from soket into buffer
        read(connFd, &buffer, sizeof(buffer));

        // Check if exit command has been received
        if (strncmp("exit", buffer, 4) == 0)
        {
            printf("Exit command received. Exiting...\n");
            close(connFd);
            break;
        }

        // Print the message and clear the buffer
        printf("Message: %s", buffer);
        bzero(&buffer, BUFFER_SIZE);
    }

    // Close the socket
    close(socketFd);

    exit(EXIT_SUCCESS);
}