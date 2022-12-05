#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8081
#define QUEUE 5
#define BUFF_SIZE 512
#define DEST_IP "127.0.0.1"
#define DEST_PORT 8082

void log(char *format, ...)
{
    // Calculate and create buffer to add a prefix and a subfix to the message
    char *suffix = "\n";
    size_t suffix_len = strlen(suffix);
    size_t fmt_len = strlen(format);
    size_t buff_len = (fmt_len + suffix_len) * sizeof(char) + 1; // +1 for null terminator
    char *buff = malloc(buff_len);
    strncpy(buff, format, fmt_len);
    strncat(buff, suffix, ++suffix_len); // +1 for null terminator

    // Initalize the args list and print the message
    va_list args;
    va_start(args, format);
    vfprintf(stderr, buff, args);

    // End the args list and free the buffer and exit
    va_end(args);
    free(buff);
}

void log_err_and_term(char *format, ...)
{
    // Calculate and create buffer to add a prefix and a subfix to the message
    char *prefix = "Error: ";
    char *suffix = "\n";
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t pad_len = prefix_len + suffix_len;
    size_t fmt_len = strlen(format);
    size_t buff_len = (pad_len + fmt_len) * sizeof(char) + 1; // +1 for null terminator
    char *buff = malloc(buff_len);
    strncpy(buff, prefix, prefix_len);
    strncat(buff, format, fmt_len);
    strncat(buff, suffix, ++suffix_len); // +1 for null terminator

    // Initalize the args list and print the message
    va_list args;
    va_start(args, format);
    vfprintf(stderr, buff, args);

    // End the args list and free the buffer and exit
    va_end(args);
    free(buff);
    exit(EXIT_FAILURE);
}

int create_socket_and_listen(int port, int queue_size)
{
    // Create file descriptor for socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        log_err_and_term("(%d)Failed to create socket file descriptor", socket_fd);
    else
        log("Socket file descriptor (%d) created", socket_fd);

    // Socket Address Info
    struct sockaddr_in sockaddr_in;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);
    sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to address
    int bind_ret = bind(socket_fd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
    if (bind_ret != 0)
        log_err_and_term("(%d)Failed to bind scoket to PORT %d", bind_ret, port);
    else
        log("Socket binded to PORT %d", port);

    // Listen on socket
    int listen_ret = listen(socket_fd, queue_size);
    if (listen_ret != 0)
        log_err_and_term("(%d) Failed to listen on socket", listen_ret);
    else
        log("Socket listening...");

    // Return file descriptor of the socket
    return socket_fd;
}

int accept_conn(int sock_fd, struct sockaddr_in *p_client_info, socklen_t *p_info_len)
{
    // Set the info_len to be the size of socklen_t
    *p_info_len = sizeof(struct sockaddr_in);

    // Accept the connection
    int conn_fd = accept(sock_fd, (struct sockaddr *)p_client_info, p_info_len);
    if (conn_fd < 0)
        log_err_and_term("(%d)Failed to accept connection", conn_fd);
    else
        log("Connection accepted (fd: %d, address: %s, port: %u)", conn_fd, inet_ntoa(p_client_info->sin_addr), p_client_info->sin_port);

    // Return a pointer to the client addr
    return conn_fd;
}

struct forward_info
{
    int fm_fd;
    int to_fd;
    size_t buff_size;
};

void *forward(struct forward_info *p_forward_info)
{
    int fm_fd = p_forward_info->fm_fd;
    int to_fd = p_forward_info->to_fd;
    size_t buff_size = p_forward_info->buff_size;
    uint8_t *buff = malloc(buff_size);
    while (1)
    {
        // Read from src
        int read_ret = read(fm_fd, buff, buff_size);
        log("Bytes read: %d", read_ret);
        if (read_ret < 0)
        {
            log("(%d)Failed to read from fd %d", read_ret, fm_fd);
            break;
        }
        if (read_ret == 0)
        {
            log("No more content can be read from fd %d", fm_fd);
            break;
        }

        // Write to des
        int write_ret = write(to_fd, buff, read_ret);
        log("Bytes written: %d", write_ret);
        if (write_ret < 0)
        {
            log("(%d)Failed to write to fd %d", write_ret, to_fd);
            break;
        }
        if (write_ret == 0)
        {
            log("No more content can be write to fd %d", to_fd);
            break;
        }

        // Clear buffer
        log("Cleaning buffer...");
        bzero(buff, buff_size);
    }

    // Free buffer
    free(buff);

    // Close connections
    shutdown(fm_fd, SHUT_RD);
    shutdown(to_fd, SHUT_WR);
    close(fm_fd);
    close(to_fd);

    // Exit the thread
    pthread_exit(NULL);
}

// TODO: Update handle conn to be threaded
struct handle_conn_info
{
    struct sockaddr_in src_addr_info;
    int src_conn_fd;
    char *des_name;
    int des_port;
};

void *handle_conn(struct handle_conn_info *p_handle_conn_info)
{
    // Variable mapping
    struct sockaddr_in src_addr_info = p_handle_conn_info->src_addr_info;
    int src_conn_fd = p_handle_conn_info->src_conn_fd;
    char *des_name = p_handle_conn_info->des_name;
    int des_port = p_handle_conn_info->des_port;

    // Set the forwarding info
    struct sockaddr_in des_addr_info;
    des_addr_info.sin_family = AF_INET;
    des_addr_info.sin_addr.s_addr = inet_addr(des_name);
    des_addr_info.sin_port = htons(des_port);

    // Create socket for forwarding
    int f_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (f_sock_fd < 0)
        log_err_and_term("(%d)Failed to create socket file descriptor", f_sock_fd);
    else
        log("Socket file descriptor (%d) created", f_sock_fd);

    // Connect to socket
    int connect_ret = connect(f_sock_fd, (struct sockaddr *)&des_addr_info, sizeof(des_addr_info));
    if (connect_ret != 0)
        log_err_and_term("(%d)Failed to connect to %s at port %d", connect_ret, inet_ntoa(des_addr_info.sin_addr), ntohs(des_addr_info.sin_port));
    else
        log("Connected to %s at port %d", inet_ntoa(des_addr_info.sin_addr), ntohs(des_addr_info.sin_port));

    // Create 2 threads to handle two way trafficing
    struct forward_info s2d_info = {src_conn_fd, f_sock_fd, BUFF_SIZE};
    struct forward_info d2s_info = {f_sock_fd, src_conn_fd, BUFF_SIZE};
    pthread_t s2d_thread, d2s_thread;
    pthread_create(&s2d_thread, NULL, &forward, &s2d_info);
    pthread_create(&d2s_thread, NULL, &forward, &d2s_info);

    // Wait for the threads to exit before exiting this thread
    pthread_join(s2d_thread, NULL);
    pthread_join(d2s_thread, NULL);

    // Free the used heap memory and exit thread
    free(p_handle_conn_info);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    // Ignore write error. Write error will be handled within the process
    signal(SIGPIPE, SIG_IGN);

    // Create tcp socket and listen on it
    int l_sock_fd = create_socket_and_listen(PORT, QUEUE);

    while (1)
    {
        // Accept a connection
        struct sockaddr_in src_info;
        socklen_t src_info_len;
        int src_conn_fd = accept_conn(l_sock_fd, &src_info, &src_info_len);

        // Create a new thread to handle the connection
        pthread_t handle_thread;
        struct handle_conn_info *p_handle_conn_info = malloc(sizeof(struct handle_conn_info));
        p_handle_conn_info->src_conn_fd = src_conn_fd;
        p_handle_conn_info->src_addr_info = src_info;
        p_handle_conn_info->des_name = DEST_IP;
        p_handle_conn_info->des_port = DEST_PORT;
        pthread_create(&handle_thread, NULL, &handle_conn, p_handle_conn_info);
    }

    return 0;
}