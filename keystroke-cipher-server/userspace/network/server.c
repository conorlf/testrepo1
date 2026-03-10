#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>       // threads for accept loop
#include "server.h"
#include <sys/socket.h>    // socket, bind, listen, accept
#include <netinet/in.h>    // sockaddr_in, htons
#include <openssl/ssl.h>   // SSL_CTX, SSL_new, SSL_accept etc
#include <openssl/err.h>   // ERR_print_errors_fp for debugging
#include <unistd.h>        // close()
#include <arpa/inet.h>       //inet()

static int listen_fd;
static int running = 1;
static SSL_CTX *ssl_ctx;
static pthread_t accept_thread;


/*
 * server_init - set up SSL context and start listening
 * - SSL_CTX_new(TLS_server_method())
 * - SSL_CTX_use_certificate_file, SSL_CTX_use_PrivateKey_file
 * - socket, setsockopt, bind, listen
 * - pthread_create for server_accept_loop
 */
int server_init(int port)
{
    struct sockaddr_in addr;

    //SSL setup
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    int cert_file = SSL_CTX_use_certificate_file(ssl_ctx, "cert.pem", SSL_FILETYPE_PEM);
    if (cert_file <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    int priv_key_file = SSL_CTX_use_PrivateKey_file(ssl_ctx, "key.pem", SSL_FILETYPE_PEM);
    if (priv_key_file <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    //Socket Setup

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("Socket failure\n");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port
    );

    int binder = bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (binder < 0) {
        perror("Bind Failed");
        return -1;
    }

    int listener = listen(listen_fd, 10);
    if (listener < 0) {
        perror("Listen Failed");
        return -1;
    }

    //Make accept threads

    pthread_create(&accept_thread, NULL, server_accept_loop, NULL);
    pthread_detach(accept_thread);

    printf("Server listening on port %d\n", port);
    return 0;
}

/*
 * server_accept_loop - continuously accept incoming peer connections
 * - while(running): SSL_accept on new connections
 * - for each connection: pthread_create(server_handle_connection)
 * - handle thread cleanup to avoid zombie threads
 */
void *server_accept_loop(void *arg)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (running) {
        printf("Waiting for connection...\n");
        fflush(stdout); 
        client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_t thread; //Handle's one peer http request
        pthread_create(&thread, NULL, server_handle_connection, client_fd_ptr);
        pthread_detach(thread);

        printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));
    }

    return NULL;
}

/*
 * server_handle_connection - process one peer HTTP request
 * - parse HTTP headers to get Content-Length, X-Sender-IP, X-Is-Chatroom
 * - read body (the encrypted message)
 * - check target FIFO (incoming or chatroom) for space
 * - if FIFO full: SSL_write HTTP 429 response and close
 * - if space: write message to FIFO, SSL_write HTTP 200 response
 * this is where backpressure is enforced at the receiving end
 */
void *server_handle_connection(void *arg)
{
    int client_fd = *(int*)arg;
    free(arg); // was malloc'd before pthread_create
    char buf[1024];

    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, client_fd);

    int ssl_accept = SSL_accept(ssl);
    if (ssl_accept <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_fd);
        return NULL;
    }

    int bytes = SSL_read(ssl, buf, sizeof(buf) - 1);
    if (bytes > 0) {
        buf[bytes] = '\0';
        printf("Received: %s\n", buf);

        const char *response = "HTTP/1.1 200 OK\r\n\r\n"; // check FIFO space here, then respond
        SSL_write(ssl, response, strlen(response));
    }

    //Clean up
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
    return NULL;

}

/*
 * server_stop - shut down server cleanly
 * - set running = 0
 * - close listen_fd
 * - SSL_CTX_free(ssl_ctx)
 */
void server_stop(void)
{
    running = 0;
    close(listen_fd);
    SSL_CTX_free(ssl_ctx);
}


int main() {
    server_init(8080);
    while(1) sleep(1);
    return 0;
}