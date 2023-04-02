
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <sys/select.h>
#include "../include/net.h"
#include "../include/log.h"

#define MAX_BUF_SIZE 8112

static int open_connection(const char *hostname, const char *port) {
    int sockfd;
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        struct timeval tv;
        tv.tv_sec =  2;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
    }

    freeaddrinfo(res);

    if (p == NULL) {
        return -1;
    }

    return sockfd;
}

int send_secure_request(struct request *request, char * content, size_t len, struct response *response) {
    SSL_CTX *ctx;
    SSL *ssl;
    int sockfd;
    char buf[MAX_BUF_SIZE];
    fd_set readfds;

    SSL_library_init();
    ctx = SSL_CTX_new(TLSv1_2_client_method());

    if (ctx == NULL) {
        fprintf(stderr, "Error creating SSL context.\n");
        return -1;
    }
    fprintf(stderr,"Verifying certificate\n");
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    sockfd = open_connection(request->host, request->port);
    if (sockfd < 0) {
        fprintf(stderr, "Error connecting to server.\n");
        SSL_CTX_free(ctx);
        return -1;
    }

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    fprintf(stderr,"Establishing SSL connection with %s\n", request->host);

    if (SSL_connect(ssl) == -1) {
        fprintf(stderr, "Error establishing SSL connection.\n");
        SSL_CTX_free(ctx);
        return -1;
    }

    fprintf(stderr,"Sending encrypted request to %s\n", request->host);
    SSL_write(ssl, content, (int)len);

    response->data = malloc(1);
    response->size = 0;
    int received = 0;
    int total_received = 0;
    fprintf(stderr,"READING STREAM #");
    while (1) {
        fprintf(stderr,"#");
        received = SSL_read(ssl, buf, sizeof(buf));
        total_received += received;

        if (received > 0) {
            response->data = realloc(response->data, response->size + received);
            memcpy(response->data + response->size, buf, received);
            response->size += received;
        } else {
            int err = SSL_get_error(ssl, received);
            switch (err) {
                case SSL_ERROR_NONE: {
                    fprintf(stderr, "\nSSL_ERROR_NONE");
                    continue;
                }
                case SSL_ERROR_ZERO_RETURN: {
                    fprintf(stderr, "\nSSL_ERROR_ZERO_RETURN\n");
                    break;
                }
                case SSL_ERROR_WANT_READ: {
                    fprintf(stderr, "\nSSL_ERROR_WANT_READ: WAITING FOR MORE DATA\n");
                    int sock = SSL_get_rfd(ssl);
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);

                    struct timeval timeout;
                    timeout.tv_sec = 2;
                    timeout.tv_usec = 0;

                    int ret = select(sock + 1, &readfds, NULL, NULL, &timeout);
                    if (ret == 0) {
                        fprintf(stderr, "TIMEOUT\n");
                        break;
                    } else if (ret > 0) {
                        fprintf(stderr, "WANT READ\n");
                        continue;
                    }
                    break;
                }
                case SSL_ERROR_WANT_WRITE: {
                    fprintf(stderr, "\nSSL_ERROR_WANT_WRITE\n");
                    int sock = SSL_get_wfd(ssl);
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);

                    struct timeval timeout;
                    timeout.tv_sec = 2;
                    timeout.tv_usec = 0;

                    int ret = select(sock + 1, NULL, &readfds, NULL, &timeout);
                    if (ret == 0) {
                        fprintf(stderr, "TIMEOUT\n");
                        break;
                    } else if (ret > 0) {
                        fprintf(stderr, "WANT WRITE\n");
                        continue;
                    }

                    break;
                }
                default: {
                    fprintf(stderr, "\nSSL_ERROR: %d\n", err);
                    break;
                }
            }
            break;
        }
    }
    fprintf(stderr,"\nSSL Request done\n");
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sockfd);

    return 0;
}