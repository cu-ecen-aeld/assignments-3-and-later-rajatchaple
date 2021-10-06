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

/***************Includes***************/
#define BUF_SIZE 256
#define PORT 9000
#define FILE "/var/tmp/aesdsocketdata"

#define SYSLOG   //uncomment this to print data using printf
#ifdef SYSLOG
#define LOG_DBG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define LOG_ERROR(...) syslog(LOG_ERR, __VA_ARGS__)
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
    EXIT
} server_socket_state = STATE_OPENING_SOCKET;

int sockfd_server;
int sockfd_client;
int fd;  
int status;

/******************************************************************************
 * Signal handler
 * ****************************************************************************/
void signal_handler() {

	close(fd);
	close(sockfd_server);
	closelog();

    syslog(LOG_INFO, "Caught Signal, exiting");

    status = remove(FILE);
	if(status < 0) {
		perror("Could not delete file");
		exit(-1);
	}
	
	exit(0);
}



/******************************************************************************
 * Application entry point function 
 * ****************************************************************************/
int main(int argc, char *argv[])
{
    openlog("aesdsocket", 0, LOG_USER);  //setting up explicitly LOG_USER facility (usually its default set to user)
    pid_t pid;  
    int nread, nwrite, nsent, nreceived;
    struct sockaddr_in server_addr, new_addr;
    struct sockaddr_in * pV4Addr = (struct sockaddr_in*)&new_addr;
    struct in_addr client_addr = pV4Addr->sin_addr;
    char buf[BUF_SIZE];
    long total_buffer_size = BUF_SIZE;
    long filled_up_buffer_size = 0;
    socklen_t addr_size;
    bool daemon_mode = false;
    int read_buff_size = 0;
    char str[INET_ADDRSTRLEN];

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
            // LOG_DBG("STATE_OPENING_SOCKET\n");
            sockfd_server = socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd_server < 0)
            {
                perror("Error opening a socket");
                return_status = -1;
                server_socket_state = EXIT;
                break;
            }

            //set socket options
            if (setsockopt(sockfd_server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
            {
                perror("error in setsockopt");
                return_status = -1;
                server_socket_state = EXIT;
                break;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(PORT);

            server_socket_state = STATE_BINDING;
            break;

        case STATE_BINDING:
            //bind socket server
            //LOG_DBG("STATE_BINDING\n");
            status = bind(sockfd_server, (struct sockaddr *)&server_addr, sizeof(server_addr));
            if (status < 0)
            {
                perror("Error binding");
                return_status = -1;
                server_socket_state = EXIT;
                break;
            }
            if (daemon_mode == true)
                server_socket_state = STATE_START_DAEMON;
            else
                server_socket_state = STATE_LISTENING;
            break;

        case STATE_START_DAEMON:
            // LOG_DBG("DAEMON\n");
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
                server_socket_state = EXIT;
                break;
            }

            if (chdir("/") == -1)
            {
                perror("Error starting daemon");
                return_status = -1;
                server_socket_state = EXIT;
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
            // LOG_DBG("STATE_LISTENING\n");
            //listen for connections
            status = listen(sockfd_server, 5);
            if (status < 0)
            {
                perror("Listening failed");
                return_status = -1;
                server_socket_state = EXIT;
                break;
            }
            server_socket_state = STATE_OPENING_FILE;
            break;

        case STATE_OPENING_FILE:
        //    LOG_DBG("STATE_OPENING_FILE\n");
           fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_RDWR | O_TRUNC, 0644);
            if (fd == -1)
            {
                perror("error creating file");
                return_status = -1;
                server_socket_state = EXIT;
                break;
            }
            server_socket_state = STATE_ACCEPTING;
            break;

        case STATE_ACCEPTING:
            // LOG_DBG("STATE_ACCEPTING\n");
            addr_size = sizeof(new_addr);
		
            //accept socket connections
            sockfd_client = accept(sockfd_server, (struct sockaddr*)&new_addr, &addr_size);
            if(sockfd_client < 0) 
            {
                LOG_ERROR("error accepting connection");
                server_socket_state = EXIT;
                break;
            }

            inet_ntop(AF_INET, &client_addr, str, INET_ADDRSTRLEN);
            
		    syslog(LOG_INFO, "Accepted connection from %s", str);


            nwrite=0, nreceived=0, nsent=0, nread=0;
            heap_buffer_for_write = NULL;
            heap_buffer_for_read = NULL;
            filled_up_buffer_size = 0;
            total_buffer_size = BUF_SIZE;

            heap_buffer_for_write = (char *)malloc(sizeof(char)*BUF_SIZE);
            if(heap_buffer_for_write == NULL) 
            {
                LOG_ERROR("could not malloc");
                server_socket_state = EXIT;
                break;
            }
            server_socket_state = STATE_RECEIVE_FROM_CLIENT;
            break;

        case STATE_RECEIVE_FROM_CLIENT:
            // LOG_DBG("STATE_RECEIVE_FROM_CLIENT\n");
            do {
			nreceived = recv(sockfd_client, buf, sizeof(buf), 0);
			//LOG_DBG("Received %d\n", nreceived);
			if(!nreceived || (strchr(buf, '\n')!=NULL))
				server_socket_state = STATE_WRITING_FILE;

			if((total_buffer_size - filled_up_buffer_size) < nreceived) {

				total_buffer_size += nreceived;
				heap_buffer_for_write = (char *)realloc(heap_buffer_for_write, sizeof(char)*total_buffer_size);
			}
			
			memcpy(&heap_buffer_for_write[filled_up_buffer_size], buf, nreceived);
			filled_up_buffer_size += nreceived;
			
		    } while(server_socket_state == STATE_RECEIVE_FROM_CLIENT);

            break;

        case STATE_WRITING_FILE:
            // LOG_DBG("STATE_WRITING_FILE\n");
            nwrite = write(fd, heap_buffer_for_write, filled_up_buffer_size);
            //LOG_DBG("Written %d\n", nwrite);
		    if(nwrite < 0) 
            {
                perror("write failed");
                server_socket_state = EXIT;
                break;
            }
            lseek(fd, 0, SEEK_SET);
            server_socket_state = STATE_READING_FILE;
            break;

        case STATE_READING_FILE:
            //  LOG_DBG("STATE_READING_FILE\n");
            read_buff_size += filled_up_buffer_size;
            heap_buffer_for_read = (char *)malloc(sizeof(char)*read_buff_size);
            
            if(heap_buffer_for_read == NULL) 
            {
                perror("error malloc");
                server_socket_state = EXIT;
                break;
            }
                
            //store contents os output file in heap_buffer_for_read
            nread = read(fd, heap_buffer_for_read, read_buff_size);
            //LOG_DBG("Read %d\n", nread);
            if(nread != read_buff_size) 
            {
                perror("error reading file");
                server_socket_state = EXIT;
                break;
            }
            server_socket_state = STATE_SEND_TO_CLIENT;
            break;

        case STATE_SEND_TO_CLIENT:
            // LOG_DBG("STATE_SEND_TO_CLIENT\n");
            nsent = send(sockfd_client, heap_buffer_for_read, read_buff_size, 0);
            if(nsent != read_buff_size) 
            {
                perror("error sending");
                server_socket_state = EXIT;
                break;
		    }
            free(heap_buffer_for_read);
	        free(heap_buffer_for_write);
            close(sockfd_client);
            syslog(LOG_INFO, "Closed connection from %s", str);
            server_socket_state = STATE_ACCEPTING;
            break;

        case EXIT:
            // LOG_DBG("EXIT\n");
            close(fd);
            close(sockfd_server);
            status = remove(FILE);
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