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
#include <netdb.h>

struct proc_args
{
    int port;
    size_t buff_size;
    int queue_size;
    char *des_name;
    int des_port;
};

struct handle_conn_info
{
    struct sockaddr_in src_addr_info;
    int src_conn_fd;
    struct in_addr *des_addr;
    int des_port;
    size_t buff_size;
};

struct forward_info
{
    int fm_fd;
    int to_fd;
    char *fm_addr;
    char *to_addr;
    int fm_port;
    int to_port;
    size_t buff_size;
};

char *get_addr_str(struct in_addr *addr);
void log(char *format, ...);
void log_err_and_term(char *format, ...);
void usage(char *proc_name);
void read_opts(int argc, char **argv, struct proc_args *p_proc_args);
int create_socket_and_listen(int port, int queue_size);
int accept_conn(int sock_fd, struct sockaddr_in *p_client_info, socklen_t *p_info_len);
void *handle_conn(struct handle_conn_info *p_handle_conn_info);
void *forward(struct forward_info *p_forward_info);

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
    // Read arguments
    struct proc_args proc_args;
    read_opts(argc, argv, &proc_args);
    log(
        "The application is started with the below configuration:\nListen Port: %d\nBuffer Size: %lu\nConnection Queue Size: %d\nForward Address: %s:%d",
        proc_args.port, proc_args.buff_size, proc_args.queue_size, proc_args.des_name, proc_args.des_port);

    // Find the ip address of the forward destination
    struct hostent *he = gethostbyname(proc_args.des_name);
    if (he == NULL)
        log_err_and_term("Cannot find ip of %s", proc_args.des_name);
    struct in_addr *addr = ((struct in_addr **)he->h_addr_list)[0];
    log("Forward IP: %s", inet_ntoa(*addr));

    // Ignore write error. Write error will be handled within the process
    signal(SIGPIPE, SIG_IGN);

    // Create tcp socket and listen on it
    int l_sock_fd = create_socket_and_listen(proc_args.port, proc_args.queue_size);

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
        p_handle_conn_info->des_addr = addr;
        p_handle_conn_info->des_port = proc_args.des_port;
        p_handle_conn_info->buff_size = proc_args.buff_size;
        pthread_create(&handle_thread, NULL, &handle_conn, p_handle_conn_info);
    }

    return 0;
}

void log(char *format, ...)
{
    // Calculate and create buffer to add a prefix and a subfix to the message
    char *suffix = "\n";
    size_t suffix_len = strlen(suffix);
    size_t fmt_len = strlen(format);
    size_t buff_len = (fmt_len + suffix_len + 1) * sizeof(char); // +1 for null terminator
    char *buff = malloc(buff_len);
    memset(buff, 0, buff_len);
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
    memset(buff, 0, buff_len);
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
        log("Connection accepted (fd: %d, address: %s, port: %u)", conn_fd, inet_ntoa(p_client_info->sin_addr), ntohs(p_client_info->sin_port));

    // Return a pointer to the client addr
    return conn_fd;
}

void *forward(struct forward_info *p_forward_info)
{
    int fm_fd = p_forward_info->fm_fd;
    int to_fd = p_forward_info->to_fd;
    char *fm_addr = p_forward_info->fm_addr;
    char *to_addr = p_forward_info->to_addr;
    int fm_port = p_forward_info->fm_port;
    int to_port = p_forward_info->to_port;
    size_t buff_size = p_forward_info->buff_size;
    uint8_t *buff = malloc(buff_size);
    while (1)
    {
        // Read from src
        int read_ret = read(fm_fd, buff, buff_size);
        if (read_ret < 0)
        {
            log("(%d)Failed to read from %s:%d", read_ret, fm_addr, fm_port);
            break;
        }
        if (read_ret == 0)
        {
            log("No more content can be read from %s:%d", fm_addr, fm_port);
            break;
        }
        log("Read %d bytes from %s:%d", read_ret, fm_addr, fm_port);

        // Write to des
        int write_ret = write(to_fd, buff, read_ret);
        if (write_ret < 0)
        {
            log("(%d)Failed to write to %s:%d", write_ret, to_addr, to_port);
            break;
        }
        if (write_ret == 0)
        {
            log("No more content can be write to %s:%d", to_addr, to_port);
            break;
        }
        log("Wrote %d bytes to %s:%d", write_ret, to_addr, to_port);

        // Clear buffer
        log("Cleaning buffer...");
        memset(buff, 0, buff_size);
    }

    // Free buffer
    free(buff);

    // Close connections
    shutdown(fm_fd, SHUT_RDWR);
    shutdown(to_fd, SHUT_RDWR);

    // Exit the thread
    pthread_exit(NULL);
}

void *handle_conn(struct handle_conn_info *p_handle_conn_info)
{
    // Variable mapping
    struct sockaddr_in src_addr_info = p_handle_conn_info->src_addr_info;
    int src_conn_fd = p_handle_conn_info->src_conn_fd;
    int src_port = (int)ntohs(src_addr_info.sin_port);
    struct in_addr *des_addr = p_handle_conn_info->des_addr;
    int des_port = p_handle_conn_info->des_port;

    // Set the forwarding info
    struct sockaddr_in des_addr_info;
    des_addr_info.sin_family = AF_INET;
    des_addr_info.sin_addr = *des_addr;
    des_addr_info.sin_port = htons(des_port);

    // Parse the address string
    pthread_mutex_lock(&mutex_lock); // Lock thread to prevent race condition when copying address
    char *src_addr_str = get_addr_str(&(src_addr_info.sin_addr));
    char *des_addr_str = get_addr_str(&(des_addr_info.sin_addr));
    pthread_mutex_unlock(&mutex_lock);

    // Create socket for forwarding
    int f_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (f_sock_fd >= 0)
    {
        log("Socket file descriptor (%d) created", f_sock_fd);

        // Connect to socket
        int connect_ret = connect(f_sock_fd, (struct sockaddr *)&des_addr_info, sizeof(des_addr_info));
        if (connect_ret == 0)
        {
            log("Connected to %s at port %d", inet_ntoa(des_addr_info.sin_addr), ntohs(des_addr_info.sin_port));

            // Create 2 threads to handle two way trafficing
            struct forward_info s2d_info = {src_conn_fd, f_sock_fd, src_addr_str, des_addr_str, src_port, des_port, p_handle_conn_info->buff_size};
            struct forward_info d2s_info = {f_sock_fd, src_conn_fd, des_addr_str, src_addr_str, des_port, src_port, p_handle_conn_info->buff_size};
            pthread_t s2d_thread, d2s_thread;
            pthread_create(&s2d_thread, NULL, &forward, &s2d_info);
            pthread_create(&d2s_thread, NULL, &forward, &d2s_info);

            // Wait for the threads to exit before exiting this thread
            pthread_join(s2d_thread, NULL);
            pthread_join(d2s_thread, NULL);
        }
        else
        {
            log("(%d)Failed to connect to %s at port %d", connect_ret, inet_ntoa(des_addr_info.sin_addr), ntohs(des_addr_info.sin_port));
        }
    }
    else
    {
        log("(%d)Failed to create socket file descriptor", f_sock_fd);
    }

    // Close connections
    close(src_conn_fd);
    if (f_sock_fd >= 0)
        close(f_sock_fd);

    // Free used resources
    free(p_handle_conn_info);
    free(src_addr_str);
    free(des_addr_str);

    // Exit thread
    pthread_exit(NULL);
}

void usage(char *proc_name)
{
    printf(
        "Usage: %s -t fwd_addr -p fwd_port [-l lstn_port] [-b buff_size] [-q queue_size]\n\
        fwd_addr:\tThe address to be forwarded to\n\
        fwd_port:\tThe port number to be forwarded to\n\
        lstn_port:\tThe port to listen on\n\
        buff_size:\tThe size of the read/write buffer in KB (default to 1 KB).\n\
        \t\tEach ongoing connection will consume [2 * buff_size] amount of memory.\n\
        queue_size:\tThe maximum number of pending connecting allowed (default to 5)\n",
        proc_name);
}

void read_opts(int argc, char **argv, struct proc_args *p_proc_args)
{
    // Return usage if no argument is provided
    if (argc <= 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Zero the args struct
    memset(p_proc_args, 0, sizeof(struct proc_args));

    // Set default values
    int listen_port = 8081;
    int queue_size = 5;
    int i_buff_size = 1024;
    int des_port = -1;
    char *des_name = NULL;

    // Read args
    int ch;
    while ((ch = getopt(argc, argv, "l:b:q:t:p:h")) != -1)
    {
        switch (ch)
        {
        case 'l':
            listen_port = atoi(optarg);
            break;
        case 'b':
            i_buff_size = atoi(optarg) * 1024;
            break;
        case 'q':
            queue_size = atoi(optarg);
            break;
        case 't':
            des_name = optarg;
            break;
        case 'p':
            des_port = atoi(optarg);
            break;
        case '?':
        case 'h':
        default:
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    // Validate args
    if (listen_port <= 0)
        log_err_and_term("Please provide a valid listen port number");
    if (i_buff_size <= 0)
        log_err_and_term("Please provide a valid buffer size");
    if (queue_size < 0)
        log_err_and_term("Please provide a valid queue size");
    if (des_port <= 0)
        log_err_and_term("Please provide a valid target port number");
    if (des_name == NULL)
        log_err_and_term("Please provide a valid target address");

    // Set process args
    p_proc_args->port = listen_port;
    p_proc_args->buff_size = i_buff_size;
    p_proc_args->queue_size = queue_size;
    p_proc_args->des_name = des_name;
    p_proc_args->des_port = des_port;
}

char *get_addr_str(struct in_addr *addr)
{
    char *tmp_addr_str = inet_ntoa(*addr);
    size_t str_len = strlen(tmp_addr_str + sizeof(char)); // +1 for null terminator
    char *str = malloc(str_len);
    strncpy(str, tmp_addr_str, str_len);
    return str;
}