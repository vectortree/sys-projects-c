#include <stdlib.h>
#include <unistd.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

static void terminate(int status);
static void sighup_handler(int sig);
static void *thread(void *vargp);

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
    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    // NOTE: We are allowed to use code snippets from the textbook and/or slides.
    // This is for setting up the server socket and entering a loop
    // to accept connections on this socket.
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);
    if(listenfd < 0) terminate(EXIT_FAILURE);
    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        if(connfdp == NULL) terminate(EXIT_FAILURE);
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if(*connfdp < 0) terminate(EXIT_FAILURE);
        Pthread_create(&tid, NULL, thread, connfdp);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the PBX server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    debug("Shutting down PBX...\n");
    pbx_shutdown(pbx);
    debug("PBX server terminating\n");
    exit(status);
}

/*
 * This is the SIGHUP handler. Upon receipt of a
 * SIGHUP signal, it terminates the server cleanly.
 */
static void sighup_handler(int sig) {
    terminate(EXIT_SUCCESS);
}

/*
 * For each connection, a client service thread is started.
 */
static void *thread(void *vargp) {
    return pbx_client_service(vargp);
}
