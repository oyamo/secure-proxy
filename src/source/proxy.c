#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/net.h"
#include "../include/proxy.h"
#include "../include/log.h"

/**
 * Check if the host is forbidden
 * @param pSites
 * @param host
 * @return
 */
bool is_forbidden(struct forbidden_sites *pSites, char *host);

char* get_remote_ip(int client_fd);

void* handle_request(void* arg)
{

    struct config *config = (struct config*)arg;
    int new_socket = config->client_fd;
    char *ip = get_remote_ip(new_socket);
    char buffer[1024] = {0};

    size_t bytes_read;
    bytes_read = read(new_socket, buffer, 1024);
    if (bytes_read == 0) {
        // client closed connection
        close(new_socket);
        return NULL;
    }

    struct request req;
    if (parse_request(buffer, &req) != 0) {
        // invalid request
        fprintf(stderr, "Invalid request: %s", buffer);
        write_bad_request(new_socket, ip, req.host , req.uri, req.method, config->log_path);
        close(new_socket);
        return NULL;
    }

    // check if host is forbidden
    if (is_forbidden(config->forbiddenSites, req.host)) {
        // forbidden site
        fprintf(stderr,"Forbidden site: %s\n", req.host);
        write_forbidden(new_socket, ip, req.host, req.uri, req.method, config->log_path);
        close(new_socket);
        return NULL;
    }

    int s_status = secure_request(&req);
    if (s_status != 0) {
        // secure request failed
        fprintf(stderr,"Secure request failed: %s", buffer);
        write_non_implemented(new_socket, ip, req.host, req.uri, req.method, config->log_path);
        close(new_socket);
        return NULL;
    }

    struct response resp;
    int resp_status = send_secure_request(&req, buffer, strlen(buffer), &resp);
    if (resp_status != 0) {
        write_server_down(new_socket, ip, req.host, req.uri, req.method, config->log_path);
        close(new_socket);
        return NULL;
    }

    write(new_socket, resp.data, resp.size);
    // client ip request first line http status code response size in byte
    // 127.70.7.1 "GET http://ucsc.edu/ HTTP/1.1" 200 2326

    int reponse_code = response_code(resp.data);
    log_f("%s \"%s %s HTTP/1.1\" %d %d",ip , req.method, req.uri,reponse_code , resp.size);
    flog_f(config->log_path,"%s \"%s %s HTTP/1.1\" %d %d",ip , req.method, req.uri,reponse_code , resp.size);
    close(new_socket);

    return NULL;
}



int parse_request(char *request_string, struct request *req) {
    // find end of method
    char *end_method = strchr(request_string, ' ');
    if (end_method == NULL) {
        // invalid request string
        return 1;
    }
    // calculate length of method
    size_t method_len = end_method - request_string;

    // copy method to request structure
    req->method = strndup(request_string, method_len);

    // find start of URI
    char *start_uri = end_method + 1;

    // find end of URI
    char *end_uri = strchr(start_uri, ' ');
    if (end_uri == NULL) {
        // invalid request string
        return 1;
    }
    // calculate length of URI
    size_t uri_len = end_uri - start_uri;

    // copy URI to request structure
    req->uri = strndup(start_uri, uri_len);

    // find start of HTTP version
    char *start_version = end_uri + 1;

    // find end of HTTP version
    char *end_version = strstr(start_version, "\r\n");
    if (end_version == NULL) {
        // invalid request string
        return 1;
    }
    // calculate length of HTTP version
    size_t version_len = end_version - start_version;

    // copy HTTP version to request structure
    req->http_version = strndup(start_version, version_len);

    // find start of Host header
    char *start_host = strstr(request_string, "Host: ");
    if (start_host == NULL) {
        // Host header not found
        return 1;
    }
    start_host += 6; // skip "Host: "

    // find end of Host header
    char *end_host = strstr(start_host, "\r\n");
    if (end_host == NULL) {
        // invalid request string
        return 1;
    }
    // calculate length of Host header value
    size_t host_len = end_host - start_host;

    // copy Host header value to request structure
    req->host = strndup(start_host, host_len);

    // parse port from Host header value
    char *colon = strchr(req->host, ':');
    if (colon == NULL) {
        // use default port
        req->port = strdup("80");
    } else {
        // copy port to request structure
        req->port = strndup(colon + 1, end_host - colon - 1);
    }

    // find start of headers
    char *start_headers = end_version + 2;

    // find end of headers
    char *end_headers = strstr(start_headers, "\r\n\r\n");
    if (end_headers == NULL) {
        // no end of headers found
        return 1;
    }

    // calculate length of headers
    size_t headers_len = end_headers - start_headers;

    // copy headers to request structure
    req->headers = strndup(start_headers, headers_len);

    // find start of content
    char *start_content = end_headers + 4;

    // calculate length of content
    size_t content_len = strlen(request_string) - (start_content - request_string);

    // copy content to request structure
    req->content = strndup(start_content, content_len);

    // return 0 if no errors
    return 0;
}

int secure_request(struct request *req) {
    // if method is not GET or PUT return 503
    if (strcmp(req->method, "GET") != 0 && strcmp(req->method, "HEAD") != 0) {
        return 503;
    }


    // if HTTP version is not 1.0 or 1.1 return 505
    if (strcmp(req->http_version, "HTTP/1.0") != 0 && strcmp(req->http_version, "HTTP/1.1") != 0) {
        return 505;
    }

    // change port to 443 if it is 80
    if (strcmp(req->port, "80") == 0) {
        free(req->port);
        req->port = strdup("443");
    }

    return 0;
}

int write_server_down(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path) {
    char *response = malloc(1024);
    sprintf(response, "HTTP/1.1 503 Bad Gateway\nContent-Type: text/html\n\n"
                      "<html>\n<body>\n<h1>[%s] BAD GATEWAY!</h1>\n</body>\n</html>\n",
            host);
    log_f("%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri, 503 , strlen(response));
    flog_f(log_path,"%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri,503 , strlen(response));
    write(socket, response, strlen(response));
    return 0;
}


int write_non_implemented(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path) {
    struct response resp;
    char *data = malloc(1024);
    sprintf(data, "HTTP/1.1 501 Not Implemented\nContent-Type: text/html\n\n"
                  "<html>\n<body>\n<h1>[%s] NOT IMPLEMENTED!</h1>\n</body>\n</html>\n",
            host);
    resp.size = strlen(data);
    resp.data = malloc(resp.size);

    memcpy(resp.data, data, resp.size);
    // html response that server is down
    log_f("%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri, 501 , strlen(data));
    flog_f(log_path,"%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri,501 , strlen(data));
    size_t ret = write(socket, resp.data, resp.size);
    if (ret == 0) {
        perror("write");
    }
    return 0;
}



void write_forbidden(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path) {
    char *response = malloc(1024);
    sprintf(response, "HTTP/1.1 403 Forbidden\nContent-Type: text/html\n\n"
                      "<html>\n<body>\n<h1>[%s] FORBIDDEN!</h1>\n</body>\n</html>\n",
            host);
    log_f("%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri, 403 , strlen(response));
    flog_f(log_path,"%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri,403 , strlen(response));
    write(socket, response, strlen(response));
}


void write_bad_request(int socket, char *client_ip, char *host, char *uri, char *method, char*log_path) {
    char *response = malloc(1024);
    sprintf(response, "HTTP/1.1 400 Bad Request\nContent-Type: text/html\n\n"
                      "<html>\n<body>\n<h1>[%s] BAD REQUEST!</h1>\n</body>\n</html>\n",
            host);
    log_f("%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri, 400 , strlen(response));
    flog_f(log_path,"%s \"%s %s HTTP/1.1\" %d %d",client_ip , method, uri,400 , strlen(response));
    write(socket, response, strlen(response));
}

bool is_forbidden(struct forbidden_sites *pSites, char *host) {
    for (int i = 0; i < pSites->size; i++) {
        if (strcmp(pSites->sites[i], host) == 0) {
            return true;
        }
    }
    return false;
}

int response_code(char *response) {
    char* status_code_str = strstr(response, "HTTP/1.1");
    int status_code;
    if (status_code_str == NULL) {
        printf("Invalid HTTP response\n");
        return -1;
    }
    status_code_str += 9;
    char *end = strstr(status_code_str, " ");
    if (end == NULL) {
        printf("Invalid HTTP response\n");
        return -1;
    }
    status_code = atoi(status_code_str);

    return status_code;
}

char* get_remote_ip(int client_fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    char* ip_str = malloc(16);
    if (getpeername(client_fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getpeername");
        exit(EXIT_FAILURE);
    }
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, 16) == NULL) {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    return ip_str;
}
