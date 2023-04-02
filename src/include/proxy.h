//
// Created by oyamo on 3/7/23.
//

#ifndef SECURE_PROXY_PROXY_H
#define SECURE_PROXY_PROXY_H
#define DEFAULT_PORT 8080

 struct request {
    char *method;
    char *http_version;
    char *host;
    char *headers;
    char *port;
    char *uri;
    char *content;
};


/**
 * Handle incoming requests
 * @param arg Socket file descriptor
 * @return NULL
 */
void* handle_request(void* arg);

/**
 * Parse request_string
 * @param request_string
 * @param req
 */
int parse_request(char *request_string, struct request *req);

/**
 * Secure request
 * @param req
 */
int secure_request(struct request *req);



/**
 * Write forbidden response to socket
 * @param fd
 * @param response
 */
void write_forbidden(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path);

/**
 * Write bad request response
 * @param fd
 * @param response
 */
void write_bad_request(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path);


/**
 * Write server down response
 * @param fd
 * @param client_ip
 */
int write_server_down(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path);

/**
 * Write non implemented response
 * @param fd
 * @param client_ip
 */
int write_non_implemented(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path);

/**
 * Get the response code
 * @param response
 * @return
 */
int response_code(char *response);

#endif //SECURE_PROXY_PROXY_H
