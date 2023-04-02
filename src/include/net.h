//
// Created by oyamo on 3/7/23.
//

#ifndef SECURE_PROXY_NET_H
#define SECURE_PROXY_NET_H
#include "../include/proxy.h"

struct response {
    char *data;
    size_t size;
};

/**
 * Struct to hold the forbidden forbiddenSites
 */
struct forbidden_sites{
    char **sites;
    int size;
};

struct config {
    int client_fd;
    struct forbidden_sites *forbiddenSites;
    char *log_path;
};

int send_secure_request(struct request *request, char * content, size_t len, struct response *response);
#endif //SECURE_PROXY_NET_H
