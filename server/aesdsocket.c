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
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "queue.h"

/***************Defines***************/
#define USE_AESD_CHAR_DEVICE 1
#define BUF_SIZE 256
#define PORT 9000

#ifdef USE_AESD_CHAR_DEVICE
#define FILE "/dev/aesdchar"
#else
#define FILE "/var/tmp/aesdsocketdata"
#endif

#define MUTEX_LOCK pthread_mutex_lock(&mutex_socket_communication);
#define MUTEX_UNLOCK pthread_mutex_unlock(&mutex_socket_communication);
// #define NO_DEBUG

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
    STATE_INIT,
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
    STATE_CLOSING,
    STATE_EXIT
} server_socket_state = STATE_INIT;

int sockfd_server;

int fd;
int status;
bool signal_exit_request = false;

typedef struct
{
    int threadIdx;
    int fd;
    bool socket_thread_completion_flag;
    int return_status;
    int sockfd_client;
    char *str;
} threadParams_t;

typedef struct slist_data_s socket_slist_data_t;

struct slist_data_s
{
    pthread_t socket_thread;
    threadParams_t socket_thread_params;
    SLIST_ENTRY(slist_data_s)
    entries;
};

int return_status = 0;
pthread_mutex_t mutex_socket_communication;
long total_number_of_bytes_written = 0;


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
 * timer add
 * ****************************************************************************/
static inline void timespec_add( struct timespec *result,
                        const struct timespec *ts_1, const struct timespec *ts_2)
{
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if( result->tv_nsec > 1000000000L ) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}


/******************************************************************************
 * timer handler
 * ****************************************************************************/
static void timer_thread(union sigval sigval)
{
    int *fd = (int*) sigval.sival_ptr;
    struct tm *time_info;
    char time_format[100];
    time_t time_stamp;
    //int nwrite;
    size_t time_size;

    time(&time_stamp);
    time_info = localtime(&time_stamp);
    memcpy(time_format, "",100);
    time_size = strftime(time_format,100,"timestamp:%a, %d %b %Y %T %z\n", time_info);

    MUTEX_LOCK
    //nwrite = write(*fd, time_format, time_size);
    LOG_DBG("\t%d, %s, %ld\n", *fd, time_format, time_size);
    // if (nwrite < 0)
    // {
    //     perror("write failed");
    // }
    MUTEX_UNLOCK
}


/***********************************************************************************************
 * timer initialization
 * resource : https://github.com/cu-ecen-aeld/aesd-lectures/blob/master/lecture9/timer_thread.c
 * *********************************************************************************************/
static void timer_init(int* fd, timer_t *timerid)
{
    struct sigevent sev;
    struct itimerspec itimerspec;
    int clock_id = CLOCK_MONOTONIC;
    struct timespec start_time = {0};
    int status;



    //memset(&td,0,sizeof(struct thread_data));
    memset(&sev,0,sizeof(struct sigevent));

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = fd;
    sev.sigev_notify_function = timer_thread;

    if(timer_create(clock_id, &sev, timerid) != 0)
    {
        LOG_ERROR("Error creating timer\n");
        return;
    }
    
    status = clock_gettime(clock_id, &start_time);
    if(status != 0)
    {
        perror("Error getting clock\n");
        return;
    }

    itimerspec.it_interval.tv_sec = 10;
    itimerspec.it_interval.tv_nsec = 1000000;//itimerspec.it_value.tv_nsec;

     
    timespec_add(&itimerspec.it_value,&start_time,&itimerspec.it_interval);
    
    status = timer_settime(*timerid, TIMER_ABSTIME, &itimerspec, NULL );
    if(status != 0)
    {
        LOG_ERROR("Error getting clock\n");
        return;
    }
    
}


/******************************************************************************
 * socket communication on every accepted connection
 * ****************************************************************************/
void *socket_communication(void *threadp)
{

    long nread, nwrite, nsent, nreceived;
    char *heap_buffer_for_write = NULL;
    char *heap_buffer_for_read = NULL;
    long filled_up_buffer_size = 0;
    char buf[BUF_SIZE];
    long total_buffer_size_for_write_into_file = BUF_SIZE;
    long total_buffer_size_for_read_from_file = BUF_SIZE;
    char *newline_char_ptr;
    long newline_char_index = 0;
    long prev_newline_char_index = 0;
    long end_of_file_index = 0;
    //char str[INET_ADDRSTRLEN];

    threadParams_t *thread_params = (threadParams_t *)threadp;

    
    heap_buffer_for_write = (char *)malloc(sizeof(char) * BUF_SIZE);
    if (heap_buffer_for_write == NULL)
    {
        LOG_ERROR("could not malloc");
        server_socket_state = STATE_EXIT;
    }
    server_socket_state = STATE_RECEIVE_FROM_CLIENT;

    
    while (1)
    {
        switch (server_socket_state)
        {
        case STATE_RECEIVE_FROM_CLIENT:
            LOG_DBG("STATE_RECEIVE_FROM_CLIENT\n");
            do
            {
                nreceived = recv(thread_params->sockfd_client, buf, sizeof(buf), 0);
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
            LOG_DBG("STATE_WRITING_FILE\n");
            
            MUTEX_LOCK
            nwrite = write(thread_params->fd, heap_buffer_for_write, filled_up_buffer_size);
            total_number_of_bytes_written += nwrite;
            if (nwrite < 0)
            {
                perror("write failed");
                server_socket_state = STATE_EXIT;
                break;
            }
            //lseek(thread_params->fd, 0, SEEK_SET);

            MUTEX_UNLOCK
            
            server_socket_state = STATE_READING_FILE;
            break;

        case STATE_READING_FILE:
            LOG_DBG("STATE_READING_FILE\n");
            heap_buffer_for_read = (char *)malloc(sizeof(char) * (BUF_SIZE + newline_char_index));
            if (heap_buffer_for_read == NULL)
            {
                perror("error malloc");
                server_socket_state = STATE_EXIT;
                break;
            }
            total_buffer_size_for_read_from_file = BUF_SIZE + newline_char_index;
            
            MUTEX_LOCK
            lseek(thread_params->fd, 0, SEEK_SET);
            MUTEX_UNLOCK
            do
            {
                MUTEX_LOCK
                nread = read(thread_params->fd, heap_buffer_for_read, total_buffer_size_for_read_from_file);
                MUTEX_UNLOCK
                //LOG_DBG("\tno_of_bytes read expected: %ld, actual:  %ld\t", total_buffer_size_for_read_from_file, nread);
                if (nread == 0)
                {
                    // LOG_DBG("No of chars to be sent over socket : %ld\n", (newline_char_index - prev_newline_char_index));
                    server_socket_state = STATE_EXIT;
                }
                 
                //LOG_DBG("heap_buffer_for_read: %s\n", heap_buffer_for_read);
                newline_char_ptr = memchr((heap_buffer_for_read + newline_char_index), '\n', (total_buffer_size_for_read_from_file - newline_char_index));
                
                if ((newline_char_ptr != NULL))
                {
                    prev_newline_char_index = newline_char_index;
                    newline_char_index = newline_char_ptr - heap_buffer_for_read + 1;
                    server_socket_state = STATE_SEND_TO_CLIENT;
                    // LOG_DBG("-----------Found newline character = %ld\n", newline_char_index);
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

                    // LOG_DBG("-Reallocating size for %ld\ns", total_buffer_size_for_read_from_file);
                    nread = total_buffer_size_for_read_from_file;
                    MUTEX_LOCK
                    lseek(thread_params->fd, 0, SEEK_SET);
                    MUTEX_UNLOCK
                }
               

            } while (server_socket_state == STATE_READING_FILE);
            

            break;

        case STATE_SEND_TO_CLIENT:
            LOG_DBG("STATE_SEND_TO_CLIENT\n");
            //LOG_DBG("\n\nSending : %s\n\n", heap_buffer_for_read + prev_newline_char_index);
            nsent = send(thread_params->sockfd_client, heap_buffer_for_read + prev_newline_char_index, (newline_char_index - prev_newline_char_index), 0);
            if (nsent != (newline_char_index - prev_newline_char_index))
            {
                perror("error sending");
                server_socket_state = STATE_EXIT;
                break;
            }
            // LOG_DBG("\tindex of file: %ld\t", newline_char_index);

            MUTEX_LOCK
            end_of_file_index = total_number_of_bytes_written;//lseek(thread_params->fd, 0, SEEK_END);
            MUTEX_UNLOCK
            
            // LOG_DBG("\tend of file: %ld\t", end_of_file_index);

            if (end_of_file_index == newline_char_index)
            {
                server_socket_state = STATE_EXIT;
                close(thread_params->sockfd_client);
                syslog(LOG_INFO, "Closed connection from %s", thread_params->str);
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
            //pthread_mutex_unlock(&mutex_socket_communication);
            thread_params->socket_thread_completion_flag = true;
            LOG_DBG("STATE_EXIT\n");
            LOG_DBG("Exiting thread\n");
            return NULL;
            //pthread_exit(NULL);
            break;

        default:
            break;
        }
    }
    return 0;
}

/******************************************************************************
 * Application entry point function 
 * ****************************************************************************/
int main(int argc, char *argv[])
{
    openlog("aesdsocket", 0, LOG_USER); //setting up explicitly LOG_USER facility (usually its default set to user)
    pid_t pid;
    struct sockaddr_in server_addr;
    //struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&new_addr;
    //struct in_addr client_addr = pV4Addr->sin_addr;
    struct sockaddr_in client_addr;// = pV4Addr;
    timer_t timerid;
    int sockfd_client;
    socklen_t addr_size;
    bool daemon_mode = false;
    int file_descriptor;
    bool timer_started = false;
    int i = 0;

    LOG_DBG("Starting socket server...\n");
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

    pthread_mutex_init(&mutex_socket_communication, NULL);
    server_socket_state = STATE_INIT;

    

    while (1)
    {
        switch (server_socket_state)
        {

        case STATE_INIT:
            #ifndef USE_AESD_CHAR_DEVICE
            if (remove(FILE) == -1)
            {
                perror("Could not delete file");
            }
            #endif
            server_socket_state = STATE_OPENING_SOCKET;
            break;

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
            //daemon_mode = false;
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
            file_descriptor = open(FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
            if (file_descriptor == -1)
            {
                perror("error creating file");
                return_status = -1;
                server_socket_state = STATE_EXIT;
                break;
            }
            
            LOG_DBG("Creating a head for socket threads linked list\n");
            SLIST_HEAD(slisthead, slist_data_s) socket_head;
            SLIST_INIT(&socket_head);
            server_socket_state = STATE_ACCEPTING;
            break;

        case STATE_ACCEPTING:
            LOG_DBG("***************************************************\n");
            LOG_DBG("STATE_ACCEPTING\n");
            addr_size = sizeof(client_addr);

            if((daemon_mode == false) || (pid == 0))
            {
                if(timer_started == false)
                {
                    timer_init(&file_descriptor, &timerid);
                    timer_started = true;                
                }
            }

            //accept socket connections
            sockfd_client = accept(sockfd_server, (struct sockaddr *)&client_addr, &addr_size);
            if (sockfd_client < 0)
            {
                LOG_ERROR("error accepting connection\n");
                server_socket_state = STATE_EXIT;
                break;
            }

            LOG_DBG("CREATING THREAD\n");
            //malloc for thread params
            socket_slist_data_t *socket_server_data_ptr = NULL;
            socket_server_data_ptr = (socket_slist_data_t *)malloc(sizeof(socket_slist_data_t));
            socket_server_data_ptr->socket_thread_params.threadIdx = i++;
            socket_server_data_ptr->socket_thread_params.socket_thread_completion_flag = false;
            socket_server_data_ptr->socket_thread_params.fd = file_descriptor;
            socket_server_data_ptr->socket_thread_params.sockfd_client = sockfd_client;
            
            pthread_create(&socket_server_data_ptr->socket_thread,                           // pointer to thread descriptor
                           (void *)0,                                                        // use default attributes
                           socket_communication,                                             // thread function entry point
                           (void *)&(socket_server_data_ptr->socket_thread_params) // parameters to pass in
            );

            SLIST_INSERT_HEAD(&socket_head, socket_server_data_ptr, entries);

            //inet_ntop(AF_INET, &client_addr, socket_server_data_ptr->socket_thread_params.str, INET_ADDRSTRLEN);
            socket_server_data_ptr->socket_thread_params.str = inet_ntoa(client_addr.sin_addr);
            
            syslog(LOG_INFO, "Accepted connection from %s\n", socket_server_data_ptr->socket_thread_params.str);
            LOG_DBG("Accepted connection from %s\n", socket_server_data_ptr->socket_thread_params.str);

            pthread_join(socket_server_data_ptr->socket_thread, NULL);
            server_socket_state = STATE_CLOSING;
            break;

        case STATE_CLOSING:
            LOG_DBG("STATE_CLOSING\n");
            SLIST_FOREACH(socket_server_data_ptr, &socket_head, entries)
            {
                //LOG_DBG("Total %d threads were created\n", i);
                if (socket_server_data_ptr->socket_thread_params.socket_thread_completion_flag == true)
                {
                    status = close(socket_server_data_ptr->socket_thread_params.sockfd_client);
                    LOG_DBG("Thread data for thread %d removed\n", socket_server_data_ptr->socket_thread_params.threadIdx);
                    SLIST_REMOVE(&socket_head, socket_server_data_ptr, slist_data_s, entries);
                    free(socket_server_data_ptr);
                    
                }
                    

                //remove head
            }
            server_socket_state = STATE_ACCEPTING;

            if (signal_exit_request == true)
                {
                    status = timer_delete(timerid);
                    if(status != 0)
                    {
                        perror("timer could not be deleted");
                    }
                    server_socket_state = STATE_EXIT;
                }

            break;

        case STATE_EXIT:
            LOG_DBG("STATE_EXIT\n");
            close(fd);
            
            
            #ifndef USE_AESD_CHAR_DEVICE
            if (remove(FILE) == -1)
            {
                perror("Could not delete file");
            }
            #endif
            if(pthread_mutex_destroy(&mutex_socket_communication) != 0)
                perror("mutex destroy");
            close(sockfd_server);
            SLIST_FOREACH(socket_server_data_ptr, &socket_head, entries)
            {
                close(socket_server_data_ptr->socket_thread_params.sockfd_client);
                syslog(LOG_INFO, "Closed connection from %s", socket_server_data_ptr->socket_thread_params.str);
                free(socket_server_data_ptr);
            }
            

            closelog();
            return -1;
            //return return_status;
            break;

        default:
            break;
        }
    }
}
