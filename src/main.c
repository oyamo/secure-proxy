#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "include/proxy.h"
#include "include/net.h"
#include "include/log.h"
#include "include/pool.h"

#define PATH_SIZE 256

#define NUM_THREADS 4



/**
 * Signal handler for SIGINT
 * @param sig Signal number
 */
void signal_handler(int sig);

/**
 * Accept incoming connections
 */
void accept_connections();

int parse_forbidden_sites(char *path, struct forbidden_sites *sites);

// Server socket file descriptor
int server_fd;

// Server address
struct sockaddr_in address;
// List of forbidden sites
struct forbidden_sites *forbiddenSites;
// Server port
int port;
// Paths to forbidden sites and log files
char forbidden_sites_path[PATH_SIZE], log_path[PATH_SIZE];

tpool_t *tm = NULL;

int main(int argc, char *argv[])
{


    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <forbidden_sites_path> <log_path>", argv[0]);
        exit(EXIT_FAILURE);
    }


    // parse the arguments
    port = atoi(argv[1]);
    strcpy(forbidden_sites_path, argv[2]);
    strcpy(log_path, argv[3]);

    forbiddenSites = calloc(1, sizeof(struct forbidden_sites));
    int parse_status = parse_forbidden_sites(forbidden_sites_path, forbiddenSites);
    if (parse_status != 0) {
        fprintf(stderr, "Error parsing forbidden forbidden Sites file\n");
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "Parsed %d forbidden forbiddenSites\n", forbiddenSites->size);
    }

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr,"Server is running on port: %d\n", DEFAULT_PORT);

    //print pid
    fprintf(stderr,"PID: %d\n", getpid());

    // kill -SIGTERM pid
    fprintf(stderr,"To kill: kill -SIGTERM %d\n", getpid());

    // Set up signal handler for SIGINT
    signal(SIGINT, signal_handler);

    // Set up signal handler for SIGQUIT
    signal(SIGTERM, signal_handler);

    //initialize thread pool
    tm = tpool_create(NUM_THREADS);

    // Accept incoming connections
    accept_connections();


    return 0;
}

void accept_connections()
{
    int new_socket;
    int addrlen = sizeof(address);
    // Accept incoming connections and spawn threads to handle them
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        struct config *config = malloc(sizeof(struct config));
        config->client_fd = new_socket;
        config->forbiddenSites = forbiddenSites;
        config->log_path = log_path;

        fprintf(stderr,"New connection accepted on socket: %d from %s:%d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        //pthread_t thread_id;
        int check;
        if ((check = tpool_add_work(tm,handle_request,config) == 1)/*pthread_create(&thread_id, NULL, handle_request, config) != 0*/)
        {
            perror("Failed to add work");
            exit(EXIT_FAILURE);
        }
    }
}

void signal_handler(int sig)
{
    // if ctrld is pressed exit the program
    if (sig == SIGTERM)
    {
        fprintf(stderr,"Sad to see you leave; BYE :(\n");
        bzero(&address, sizeof(address));
        close(server_fd);
        if(tm != NULL) tpool_destroy(tm);
        exit(0);
    } else if (sig == SIGINT)
    {
        // reload the forbidden forbiddenSites
        fprintf(stderr,"Reloading forbiddenSites\n");
        struct forbidden_sites *new_sites = calloc(1, sizeof(struct forbidden_sites));
        int parse_status = parse_forbidden_sites(forbidden_sites_path, new_sites);
        if (parse_status != 0) {
            fprintf(stderr, "Error parsing forbiddenSites file");
            exit(EXIT_FAILURE);
        }

        forbiddenSites = new_sites;
        fprintf(stderr, "Parsed %d forbidden Sites\n", forbiddenSites->size);
    }
}

    int parse_forbidden_sites(char *path, struct forbidden_sites *sites)
    {
        FILE *fp;
        char line[PATH_SIZE];
        int i = 0;

        if ((fp = fopen(path, "r")) == NULL)
        {
            perror("fopen failed");
            return -1;
        }

        // create memory for each line
        sites->sites = malloc(sizeof(char *));

        while (fgets(line, PATH_SIZE, fp) != NULL)
        {
            sites->sites[i] = malloc(PATH_SIZE);
            // remove newline
            line[strcspn(line, "\n")] = '\0';
            strcpy(sites->sites[i], line);
            sites->sites = realloc(sites->sites, sizeof(char *) * (i + 2));
            i++;
        }

        sites->size = i;

        fclose(fp);
        return 0;
    }