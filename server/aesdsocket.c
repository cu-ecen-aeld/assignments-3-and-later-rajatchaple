/** **************************************************************************************************
* aeesdsocket.c : This program 
*                    opens socket bound to port 9000. 
*                    Listens and accepts connection
*                    Logs messages to syslog "accepted connection from xx" (xx : client IP)
*                    Receives data over the connection and appends to /var/tmp/aesdsocketdata
*                    returns content of var/tmp/aesdsocketdata to client
*                    returns full content of var/tmp/aesdsocketdata to client on completed reception
*                    Logs messages to syslog "Closed connection from xx" (xx : client IP)
*                    Restarts accepting connections from new clients forever
*                    Gracefully exits on SIGINT or SIGTERM
*                    Logs message to the syslog  "Caught signal, exiting0 on Signals"
*
*************************************************************************************************** **/

/***************Includes***************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <linux/fs.h>
#include <stdlib.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

/***************Includes***************/
#define BUF_SIZE 256
#define PORT 9000
#define FILE "/var/tmp/aesdsocketdata"

#define NO_DEBUG

#ifdef SYSLOG
#define LOG_DBG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define LOG_ERROR(...) syslog(LOG_ERR, __VA_ARGS__)
#elif defined(NO_DEBUG)
#define LOG_DBG(...)
#define LOG_ERROR(...)
#else
#define LOG_DBG(...) printf(__VA_ARGS__)
#define LOG_ERROR(...) printf(__VA_ARGS__)
#endif

/***************Variables***************/
enum socket_states
{
    STATE_OPENING_SOCKET,
    STATE_BINDING,
    STATE_START_DAEMON,
    STATE_LISTENING,
    STATE_ACCEPTING,
    STATE_OPENING_FILE,
    STATE_RECEIVE_FROM_CLIENT,
    STATE_WRITING_FILE,
    STATE_READING_FILE,
    STATE_SEND_TO_CLIENT,
    STATE_EXIT
} server_socket_state = STATE_OPENING_SOCKET;

int sockfd_server;
int sockfd_client;
int fd;
int status;
bool signal_exit_request = false;

/******************************************************************************
 * Signal handler
 * ****************************************************************************/
void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        printf("\nCaught Signal, exiting\n");
        syslog(LOG_DEBUG, "Caught Signal, exiting\n");
    }

    if (shutdown(sockfd_server, SHUT_RDWR))
    {
        perror("Failed on shutdown()");
        syslog(LOG_ERR, "Could not close socket file descriptor in signal handler : %s", strerror(errno));
    }

    signal_exit_request = true;
    syslog(LOG_INFO, "Caught Signal, exiting");
}

/******************************************************************************
 * Application entry point function 
 * ****************************************************************************/
int main(int argc, char *argv[])
{
    openlog("aesdsocket", 0, LOG_USER); //setting up explicitly LOG_USER facility (usually its default set to user)
    pid_t pid;
    long nread, nwrite, nsent, nreceived;
    struct sockaddr_in server_addr, new_addr;
    struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&new_addr;
    struct in_addr client_addr = pV4Addr->sin_addr;
    char buf[BUF_SIZE];
    long total_buffer_size_for_write_into_file = BUF_SIZE;
    long total_buffer_size_for_read_from_file = BUF_SIZE;
    long filled_up_buffer_size = 0;
    socklen_t addr_size;
    bool daemon_mode = false;
    char str[INET_ADDRSTRLEN];
    char *newline_char_ptr;
    long newline_char_index = 0;
    long prev_newline_char_index = 0;
    long end_of_file_index = 0;

    char *heap_buffer_for_write = NULL;
    char *heap_buffer_for_read = NULL;
    int return_status = 0;

    //check command line arguments
    if (argc > 2)
    {
        perror("Invalid arguments");
        return -1;
    }
    else if (argc == 1)
    {
        daemon_mode = false;
    }

    else if (argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0)
            daemon_mode = true;
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    while (1)
    {
        switch (server_socket_state)
        {
        case STATE_OPENING_SOCKET:
            LOG_DBG("STATE_OPENING_SOCKET\n");
            sockfd_server = socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd_server < 0)
            {
                perror("Error opening a socket");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }

            //set socket options
            if (setsockopt(sockfd_server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
            {
                perror("error in setsockopt");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(PORT);

            server_socket_state = STATE_BINDING;
            break;

        case STATE_BINDING:
            //bind socket server
            LOG_DBG("STATE_BINDING\n");
            status = bind(sockfd_server, (struct sockaddr *)&server_addr, sizeof(server_addr));
            if (status < 0)
            {
                perror("Error binding");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }
            if (daemon_mode == true)
                server_socket_state = STATE_START_DAEMON;
            else
                server_socket_state = STATE_LISTENING;
            break;

        case STATE_START_DAEMON:
            LOG_DBG("DAEMON\n");
            signal(SIGCHLD, SIG_IGN);
            signal(SIGHUP, SIG_IGN);
            server_socket_state = STATE_LISTENING;

            pid = fork();
            if (pid == -1)
                return -1;
            else if (pid != 0)
                exit(EXIT_SUCCESS);

            umask(0);

            if (setsid() == -1)
            {
                perror("Error starting daemon");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }

            if (chdir("/") == -1)
            {
                perror("Error starting daemon");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            //redirect files to /dev/null
            open("/dev/null", O_RDWR);
            dup(0);
            dup(0);

            break;

        case STATE_LISTENING:
            LOG_DBG("STATE_LISTENING\n");
            //listen for connections
            status = listen(sockfd_server, 5);
            if (status < 0)
            {
                perror("Listening failed");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }
            server_socket_state = STATE_OPENING_FILE;
            break;

        case STATE_OPENING_FILE:
            LOG_DBG("STATE_OPENING_FILE\n");
            fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_RDWR | O_TRUNC, 0644);
            if (fd == -1)
            {
                perror("error creating file");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }
            server_socket_state = STATE_ACCEPTING;
            break;

        case STATE_ACCEPTING:
            LOG_DBG("STATE_ACCEPTING\n");
            addr_size = sizeof(new_addr);

            //accept socket connections
            sockfd_client = accept(sockfd_server, (struct sockaddr *)&new_addr, &addr_size);
            if (sockfd_client < 0)
            {
                LOG_ERROR("error accepting connection\n");
                server_socket_state = STATE_EXIT;
                break;
            }

            inet_ntop(AF_INET, &client_addr, str, INET_ADDRSTRLEN);
            syslog(LOG_INFO, "Accepted connection from %s", str);

            nwrite = 0, nreceived = 0, nsent = 0, nread = 0;
            heap_buffer_for_write = NULL;
            heap_buffer_for_read = NULL;
            filled_up_buffer_size = 0;
            total_buffer_size_for_write_into_file = BUF_SIZE;

            heap_buffer_for_write = (char *)malloc(sizeof(char) * BUF_SIZE);
            if (heap_buffer_for_write == NULL)
            {
                LOG_ERROR("could not malloc");
                server_socket_state = STATE_EXIT;
                break;
            }
            server_socket_state = STATE_RECEIVE_FROM_CLIENT;
            break;

        case STATE_RECEIVE_FROM_CLIENT:
            LOG_DBG("STATE_RECEIVE_FROM_CLIENT");
            do
            {
                nreceived = recv(sockfd_client, buf, sizeof(buf), 0);
                LOG_DBG("\tReceived %ld\n", nreceived);
                if (!nreceived || (strchr(buf, '\n') != NULL))
                    server_socket_state = STATE_WRITING_FILE;

                if ((total_buffer_size_for_write_into_file - filled_up_buffer_size) < nreceived)
                {
                    total_buffer_size_for_write_into_file += nreceived;
                    heap_buffer_for_write = (char *)realloc(heap_buffer_for_write, sizeof(char) * total_buffer_size_for_write_into_file);
                }
                memcpy(&heap_buffer_for_write[filled_up_buffer_size], buf, nreceived);
                filled_up_buffer_size += nreceived;
            } while (server_socket_state == STATE_RECEIVE_FROM_CLIENT);

            break;

        case STATE_WRITING_FILE:
            LOG_DBG("STATE_WRITING_FILE");
            nwrite = write(fd, heap_buffer_for_write, filled_up_buffer_size);
            LOG_DBG("\tWritten %ld\n", nwrite);
            if (nwrite < 0)
            {
                perror("write failed");
                server_socket_state = STATE_EXIT;
                break;
            }
            lseek(fd, 0, SEEK_SET);
            server_socket_state = STATE_READING_FILE;
            break;

        case STATE_READING_FILE:
            LOG_DBG("STATE_READING_FILE");
            heap_buffer_for_read = (char *)malloc(sizeof(char) * BUF_SIZE);
            if (heap_buffer_for_read == NULL)
            {
                perror("error malloc");
                server_socket_state = STATE_EXIT;
                break;
            }
            LOG_DBG("------heap_buffer_for_read=%p\n", heap_buffer_for_read);
            total_buffer_size_for_read_from_file = BUF_SIZE;
            lseek(fd, 0, SEEK_SET);
            do
            {
                nread = read(fd, heap_buffer_for_read, total_buffer_size_for_read_from_file);
                LOG_DBG("\tno_of_bytes read expected: %ld, actual:  %ld\t", total_buffer_size_for_read_from_file, nread);
                if (nread == 0)
                {
                    LOG_DBG("No of chars to be sent over socket : %ld\n", (newline_char_index - prev_newline_char_index));
                    // LOG_DBG("-----------newline_ptr = %p\theap_buffer_for_read = %p\tprev_heap_buffer_for_read = %p\n", newline_char_ptr, heap_buffer_for_read, prev_heap_buffer_for_read);
                    server_socket_state = STATE_EXIT;
                }

                newline_char_ptr = memchr((heap_buffer_for_read + newline_char_index), '\n', (total_buffer_size_for_read_from_file - newline_char_index));

                if ((newline_char_ptr != NULL))
                {
                    prev_newline_char_index = newline_char_index;
                    newline_char_index = newline_char_ptr - heap_buffer_for_read + 1;
                    server_socket_state = STATE_SEND_TO_CLIENT;
                    LOG_DBG("-----------Found newline character = %ld\n", newline_char_index);
                }
                else
                {
                    total_buffer_size_for_read_from_file += BUF_SIZE;

                    heap_buffer_for_read = (char *)realloc(heap_buffer_for_read, sizeof(char) * total_buffer_size_for_read_from_file);
                    if (heap_buffer_for_read == NULL)
                    {
                        LOG_DBG("realloc failed");
                        server_socket_state = STATE_EXIT;
                        break;
                    }

                    LOG_DBG("-Reallocating size for %ld\ns", total_buffer_size_for_read_from_file);
                    nread = total_buffer_size_for_read_from_file;
                    lseek(fd, 0, SEEK_SET);
                }

            } while (server_socket_state == STATE_READING_FILE);

            break;

        case STATE_SEND_TO_CLIENT:
            LOG_DBG("STATE_SEND_TO_CLIENT\n");
            nsent = send(sockfd_client, heap_buffer_for_read + prev_newline_char_index, (newline_char_index - prev_newline_char_index), 0);
            if (nsent != (newline_char_index - prev_newline_char_index))
            {
                perror("error sending");
                server_socket_state = STATE_EXIT;
                break;
            }
            LOG_DBG("\tindex of file: %ld\t", newline_char_index);
            end_of_file_index = lseek(fd, 0, SEEK_END);
            LOG_DBG("\tend of file: %ld\t", end_of_file_index);

            if (end_of_file_index == newline_char_index)
            {
                close(sockfd_client);
                syslog(LOG_INFO, "Closed connection from %s", str);
                server_socket_state = STATE_ACCEPTING;
                newline_char_index = 0;
                LOG_DBG("Reached end of read_file\n");
                free(heap_buffer_for_write);
            }
            else
            {
                server_socket_state = STATE_READING_FILE;
                nread = 0;
            }

            free(heap_buffer_for_read);

            if (signal_exit_request == true)
                server_socket_state = STATE_EXIT;

            break;

        case STATE_EXIT:
            LOG_DBG("STATE_EXIT\n");
            close(fd);
            close(sockfd_server);
            syslog(LOG_INFO, "Closed connection from %s", str);
            close(sockfd_client);
            closelog();
            if (remove(FILE) == -1)
            {
                perror("Could not delete file");
            }
            return return_status;
            break;

        default:
            break;
        }
    }
}