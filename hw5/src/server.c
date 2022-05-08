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
    char *msg = NULL;
    size_t read_buf_size, msg_size, alloc_size;
    while(1) {
        alloc_size = 1;
        msg = Malloc(alloc_size);
        msg_size = 0;
        char read_buf;
        while((read_buf_size = Read(connfd, &read_buf, 1)) > 0) {
            if(read_buf == '\n') break;
            if(read_buf != '\r' && read_buf != '\n') {
                if(alloc_size - msg_size < read_buf_size) {
                    msg = Realloc(msg, 2 * msg_size + read_buf_size);
                    alloc_size = 2 * msg_size + read_buf_size;
                }
                strncpy(msg + msg_size, &read_buf, read_buf_size);
                msg_size += read_buf_size;
            }
        }
        if(read_buf_size == 0) break;
        msg = Realloc(msg, msg_size + 1);
        msg[msg_size] = '\0';
        char *first_token = strtok(msg, " \t");
        if(first_token == NULL) continue;
        if(strcmp(first_token, tu_command_names[TU_PICKUP_CMD]) == 0) {
            tu_pickup(tu);
        }
        else if(strcmp(first_token, tu_command_names[TU_HANGUP_CMD]) == 0) {
            tu_hangup(tu);
        }
        else if(strcmp(first_token, tu_command_names[TU_DIAL_CMD]) == 0) {
            char *ext = strtok(NULL, " \t");
            if(ext != NULL) pbx_dial(pbx, tu, atoi(ext));
        }
        else if(strcmp(first_token, tu_command_names[TU_CHAT_CMD]) == 0) {
            char *chat_msg = strtok(NULL, "");
            if(chat_msg == NULL) tu_chat(tu, "");
            else {
                int i = 0;
                char all_whitespace = 1;
                while(i < strlen(chat_msg)) {
                    if(!isspace(chat_msg[i])) {
                        all_whitespace = 0;
                        break;
                    }
                    ++i;
                }
                if(all_whitespace) tu_chat(tu, "");
                else tu_chat(tu, chat_msg + i);
            }
        }
        debug("%s\n", msg);
        Free(msg);
        msg = NULL;
    }
    debug("Unregistering client service thread (ext: %d)...\n", connfd);
    if(msg != NULL) {
        Free(msg);
        msg = NULL;
    }
    pbx_unregister(pbx, tu);
    tu_unref(tu, "pbx_client_service");
    Close(connfd);
    return NULL;
}
