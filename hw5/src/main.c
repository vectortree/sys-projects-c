#include <stdlib.h>
#include <unistd.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

static void terminate(int status);
static void sighup_handler(int sig);
static void *thread(void *vargp);

static volatile sig_atomic_t sighup_flag = 0;
static int *connfdp;

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    char opt, flag = 0;
    char *port;
    while((opt = getopt(argc, argv, "p:")) != -1) {
        switch(opt) {
            case 'p':
                if(atoi(optarg) > 0) {
                    port = optarg;
                    flag = 1;
                }
        }
    }
    if(!flag) {
        fprintf(stderr, "Usage: %s %s\n", argv[0], "-p <port>");
        exit(EXIT_SUCCESS);
    }
    // Perform required initialization of the PBX module.
    debug("Initializing PBX...\n");
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    // Install SIGHUP signal handler.
    // Ignore SIGPIPE.
    struct sigaction sa_sighup = {0}, sa_sigpipe = {0};
    sa_sighup.sa_handler = sighup_handler;
    sa_sigpipe.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &sa_sighup, NULL);
    sigaction(SIGPIPE, &sa_sigpipe, NULL);

    // NOTE: We are allowed to use code snippets from the textbook and/or slides.
    // This is for setting up the server socket and entering a loop
    // to accept connections on this socket.
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);
    if(listenfd < 0) terminate(EXIT_FAILURE);
    while(!sighup_flag) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        if(connfdp == NULL) terminate(EXIT_FAILURE);
        *connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if(*connfdp < 0 && errno != EINTR) terminate(EXIT_FAILURE);
        if(*connfdp < 0) break;
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    Close(listenfd);
    terminate(EXIT_SUCCESS);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    debug("Shutting down PBX...\n");
    Free(connfdp);
    pbx_shutdown(pbx);
    debug("PBX server terminating\n");
    pthread_exit(NULL);
}

/*
 * This is the SIGHUP handler. Upon receipt of a
 * SIGHUP signal, it terminates the server cleanly.
 */
static void sighup_handler(int sig) {
    sighup_flag = 1;
}

/*
 * For each connection, a client service thread is started.
 */
static void *thread(void *vargp) {
    return pbx_client_service(vargp);
}
