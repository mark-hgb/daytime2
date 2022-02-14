#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <assert.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

#define TCP         1
#define UDP         2

#define PROTO       TCP
#define PORT        13
#define BACKLOG     256

#define MSGLENGTH   27

#define XSTR(s)		STR(s)
#define STR(s)		#s

#define ERROR(str)  (fprintf(stderr, "Error in function %s() line %d: ", __FUNCTION__, __LINE__),\
                    perror(str), kill(0, SIGTERM), exit(EXIT_FAILURE))

#define LOG(...)	(printf(__VA_ARGS__), fflush(stdout))

volatile sig_atomic_t   active = 1;

/*
 * struct srvcfg is used to manage all necessary settings for a daytime service
 */
struct srvcfg {
    int                 proto;
    unsigned short      port;
    struct in_addr      server_ip;
    struct sockaddr_in  server_addr;
    int                 sockfd; 
};

/*
 * Declaration of functions
 */
void exiting(int sig);
void set_signal_handler(void);
void print_usage(void);
void parse_arguments(int argc, char *argv[], struct srvcfg *cfg);
void start_server(struct srvcfg *cfg);
int create_socket(int type, struct sockaddr_in *addr);
int sending_datetime(int fd);
void destroy_server(struct srvcfg *cfg);

int main(int argc, char *argv[]) {

    struct srvcfg *cfg;
    struct sockaddr_in client;
    int client_len = sizeof(struct sockaddr_in);
    int fd;

    if ((cfg = malloc(sizeof(struct srvcfg))) == NULL)
        ERROR("Error allocating memory");

    set_signal_handler();
    parse_arguments(argc, argv, cfg);
    start_server(cfg);

    while (active) {
        
        // Sockets are set to non-blocking, therefor errno needs to be checked 
        // for EWOULDBLOCK and/or EAGAIN
        if (((fd = accept(cfg->sockfd, (struct sockaddr *)&client, &client_len)) == -1) 
              && (errno != EWOULDBLOCK) && (errno != EAGAIN))
            ERROR("Error incoming connection");

        // Only if we get a socket descriptor from the accept() call, 
        // we continue sending the daytime message
        if (fd != -1) {

            LOG("Successful incoming connection from %s:%d.\n", \
                inet_ntoa(client.sin_addr),ntohs(client.sin_port));

            if (sending_datetime(fd) == -1)
                ERROR("Error sending datetime");

            LOG("Successfully sent datetime to %s:%d.\n", \
                inet_ntoa(client.sin_addr),ntohs(client.sin_port));

            if (close(fd) == -1)
                ERROR("Error on closing socket");

            LOG("Successfully closing connection from %s:%d.\n", \
                inet_ntoa(client.sin_addr),ntohs(client.sin_port));
        }
    }

    destroy_server(cfg);

    free(cfg);

    exit(EXIT_SUCCESS);
}

/*
 * In case of SIGINT (Strg+C) or SIGTERM (kill) we set active to 0 and 
 * therefor the daytime services is torn down.
 */
void exiting(int sig)
{
    active = 0;
}

/*
 * This sets the certain signal handler for SIGINT and SIGTERM to existing()
 */
void set_signal_handler(void)
{
    struct sigaction sa = {};

    sa.sa_handler = exiting;
    if (sigaction(SIGINT, &sa, NULL) == -1)
        ERROR("Error setting signal handling");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        ERROR("Error setting signal handling");
}

/*
 * Printing usage guidelines, for example possible command line arguments.
 */ 
void print_usage(void) 
{

    puts("Implementation of the RFC 867 Daytime protocol\n"
         "Usage: daytime [OPTIONS] \n"
         "\n"
         "Options:\n"
         "  -i, --ip IP     listening IP (default all)\n"
         "  -t, --tcp       TCP service (default)\n"
         "  -p, --port NUM  port number (default "XSTR(PORT)")\n"
         "  -h, --help      display this help message\n"
    );

    exit(EXIT_SUCCESS);
}

/*
 * Parsing the command line arguments using getopt()
 */ 
void parse_arguments(int argc, char *argv[], struct srvcfg *cfg)
{
    assert(cfg != NULL);

    int ret;

    static const char optstring[] = "i:tup:h";
    static const struct option optslong[] = {
        {"ip", required_argument, NULL, 'i'},
        {"tcp", no_argument, NULL, 't'},
        {"port", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {}
    };

    cfg->port = PORT;
    cfg->proto = TCP;

    while ((ret = getopt_long(argc, argv, optstring, optslong, NULL)) != -1) {

        switch (ret) {

            case 'i':
                if (inet_aton(optarg, &cfg->server_ip) == 0)
                    cfg->server_ip.s_addr = htonl(INADDR_ANY);
                break;
            case 't':
                if (cfg->proto == UDP)
                    print_usage();
                else
                    cfg->proto = TCP;
                break;
            case 'p':
                if (sscanf(optarg, "%hu", &cfg->port) != 1)
                    print_usage();
                break;
            default:
                print_usage();
        }
    }
}

/*
 * Starting the daytime services by using create_socket() until the socket is in 
 * listen state.
 */ 
void start_server(struct srvcfg *cfg) 
{
    assert(cfg != NULL);

    bzero(&cfg->server_addr, sizeof(cfg->server_addr));
    cfg->server_addr.sin_family = AF_INET;
    cfg->server_addr.sin_port = htons(cfg->port);
    cfg->server_addr.sin_addr.s_addr = cfg->server_ip.s_addr;

    if (cfg->proto == TCP) {
        cfg->sockfd = create_socket(SOCK_STREAM, &cfg->server_addr);
        // We set the socket to non-blocking, which means an accept() call won't block 
        // until a client connection request is received
        fcntl(cfg->sockfd, F_SETFL, fcntl(cfg->sockfd, F_GETFL, 0) | O_NONBLOCK);
    }

    LOG("Server started, listening on %s:%d ...\n", inet_ntoa(cfg->server_ip), cfg->port);
}

/*
 * Creating a socket in listen state for the daytime service.
 */ 
int create_socket(int type, struct sockaddr_in *addr)
{
    assert(type == SOCK_STREAM || type == SOCK_DGRAM);
	assert(addr != NULL);

    int sockfd;
    int reuseaddr = 1;

    if ((sockfd = socket(PF_INET, type, 0)) == -1)
        ERROR("Error creating socket");

    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                    &reuseaddr, sizeof(reuseaddr))) == -1)
        ERROR("Error setting socket options");

    if ((bind(sockfd, (struct sockaddr *)addr, sizeof(*addr))) == -1)
        ERROR("Error binding socket");

    if (type == SOCK_STREAM) {
        if ((listen(sockfd, BACKLOG)) == -1)
            ERROR("Error setting stream socket into listening mode");
    }
     
    return sockfd;
}

/*
 * Sending a daytime message to the client.
 */ 
int sending_datetime(int fd)
{
    time_t curr_time;
    char buffer[MSGLENGTH];

    /*
     * A daytime message from this server is 26 bytes long, including a closing \r\n.
     * Example: Thu Nov 26 11:29:54 2020\r\n
     */    
    curr_time = time(NULL);
    snprintf(buffer, sizeof(buffer), "%.24s\r\n", ctime(&curr_time));

    return write(fd, buffer, strlen(buffer));
}

void destroy_server(struct srvcfg *cfg) 
{
    assert(cfg != NULL);

    if ((close(cfg->sockfd)) == -1)
        ERROR("Error closing socket");

}