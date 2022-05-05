/*
 * "PBX" server module.
 * Manages interaction with a client telephone unit (TU).
 */
#include <stdlib.h>

#include "debug.h"
#include "pbx.h"
#include "server.h"
#include "csapp.h"

/*
 * Thread function for the thread that handles interaction with a client TU.
 * This is called after a network connection has been made via the main server
 * thread and a new thread has been created to handle the connection.
 */
void *pbx_client_service(void *arg) {
    // TO BE IMPLEMENTED
    // NOTE: We are allowed to use code snippets from the textbook and/or slides.
    // Retrieve the connection file descriptor (to communicate with the client).
    int connfd = *((int *)arg);
    // Free the argument pointer.
    Free(arg);
    // The thread is detached, so that it does not have to be explicitly reaped.
    Pthread_detach(pthread_self());
    // Initialize a new TU with the connection file descriptor.
    TU *tu = tu_init(connfd);
    // Register the TU with the PBX server under a particular extension number (i.e., connfd).
    pbx_register(pbx, tu, connfd);
    // The thread should enter a service loop in which it repeatedly
    // receives a message sent by the client, parses the message, and carries
    // out the specified command.
    char *msg;
    size_t read_buf_size, msg_size, alloc_size;
    while(1) {
        alloc_size = 1000;
        msg = Malloc(alloc_size);
        msg_size = 0;
        char read_buf[1000];
        while((read_buf_size = Read(connfd, read_buf, 1000)) > 0) {
            char *end_of_line = strstr(read_buf, EOL);
            if(end_of_line != NULL && end_of_line != read_buf)
                read_buf_size = end_of_line - read_buf;
            if(alloc_size - msg_size < read_buf_size) {
                msg = Realloc(msg, msg_size + read_buf_size);
                alloc_size = msg_size + read_buf_size;
            }
            strncpy(msg + msg_size, read_buf, read_buf_size);
            msg_size += read_buf_size;
            if(end_of_line != NULL) break;
        }
        if(read_buf_size == 0) break;
        char *first_token = strtok(msg, " \t");
        if(strcmp(msg, tu_command_names[TU_PICKUP_CMD]) == 0) {
            tu_pickup(tu);
        }
        else if(strcmp(msg, tu_command_names[TU_HANGUP_CMD]) == 0) {
            tu_hangup(tu);
        }
        else if(strcmp(first_token, tu_command_names[TU_DIAL_CMD]) == 0) {
            pbx_dial(pbx, tu, atoi(strtok(NULL, " \t")));
        }
        else if(strcmp(first_token, tu_command_names[TU_CHAT_CMD]) == 0) {
            tu_chat(tu, strtok(NULL, " \t"));
        }
        Free(msg);
    }
    pbx_unregister(pbx, tu);
    Close(connfd);
    return NULL;
}
