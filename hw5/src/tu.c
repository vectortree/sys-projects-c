/*
 * TU: simulates a "telephone unit", which interfaces a client with the PBX.
 */
#include <stdlib.h>
#include <pthread.h>

#include "pbx.h"
#include "debug.h"
#include "csapp.h"

struct tu {
    int fd;
    TU *peer;
    TU_STATE state;
    sem_t mutex;
    int ref_count;
};

/*
 * Initialize a TU
 *
 * @param fd  The file descriptor of the underlying network connection.
 * @return  The TU, newly initialized and in the TU_ON_HOOK state, if initialization
 * was successful, otherwise NULL.
 */
TU *tu_init(int fd) {
    // TO BE IMPLEMENTED
    TU *tu = Malloc(sizeof(TU));
    if(tu == NULL) return NULL;
    debug("Reached tu_init\n");
    tu->fd = fd;
    tu->peer = NULL;
    tu->state = TU_ON_HOOK;
    tu->ref_count = 1;
    Sem_init(&tu->mutex, 0, 1);
    char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
    strcpy(tu_buf, tu_state_names[tu->state]);
    char num[12];
    sprintf(num, " %d", tu->fd);
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
    strcat(tu_buf, num);
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
    strcat(tu_buf, EOL);
    Write(tu->fd, tu_buf, strlen(tu_buf));
    Free(tu_buf);
    return tu;
}

/*
 * Increment the reference count on a TU.
 *
 * @param tu  The TU whose reference count is to be incremented
 * @param reason  A string describing the reason why the count is being incremented
 * (for debugging purposes).
 */
void tu_ref(TU *tu, char *reason) {
    // TO BE IMPLEMENTED
    if(tu == NULL) return;
    P(&tu->mutex);
    ++tu->ref_count;
    if(reason != NULL) debug("tu_ref: %s\n", reason);
    V(&tu->mutex);
}

/*
 * Decrement the reference count on a TU, freeing it if the count becomes 0.
 *
 * @param tu  The TU whose reference count is to be decremented
 * @param reason  A string describing the reason why the count is being decremented
 * (for debugging purposes).
 */
void tu_unref(TU *tu, char *reason) {
    // TO BE IMPLEMENTED
    if(tu == NULL) return;
    P(&tu->mutex);
    if(tu->ref_count < 0) {
        V(&tu->mutex);
        return;
    }
    --tu->ref_count;
    int ref_count = tu->ref_count;
    debug("tu_unref: Reference count is %d\n", ref_count);
    V(&tu->mutex);
    if(ref_count == 0) {
        sem_destroy(&tu->mutex);
        Free(tu);
    }
    if(reason != NULL) debug("tu_unref: %s\n", reason);
}

/*
 * Get the file descriptor for the network connection underlying a TU.
 * This file descriptor should only be used by a server to read input from
 * the connection.  Output to the connection must only be performed within
 * the PBX functions.
 *
 * @param tu
 * @return the underlying file descriptor, if any, otherwise -1.
 */
int tu_fileno(TU *tu) {
    // TO BE IMPLEMENTED
    if(tu == NULL) return -1;
    P(&tu->mutex);
    int fileno = tu->fd;
    V(&tu->mutex);
    return fileno;
}

/*
 * Get the extension number for a TU.
 * This extension number is assigned by the PBX when a TU is registered
 * and it is used to identify a particular TU in calls to tu_dial().
 * The value returned might be the same as the value returned by tu_fileno(),
 * but is not necessarily so.
 *
 * @param tu
 * @return the extension number, if any, otherwise -1.
 */
int tu_extension(TU *tu) {
    // TO BE IMPLEMENTED
    return tu_fileno(tu);
}

/*
 * Set the extension number for a TU.
 * A notification is set to the client of the TU.
 * This function should be called at most once one any particular TU.
 *
 * @param tu  The TU whose extension is being set.
 */
int tu_set_extension(TU *tu, int ext) {
    // TO BE IMPLEMENTED
    if(tu == NULL || ext < 0) return -1;
    P(&tu->mutex);
    tu->fd = ext;
    V(&tu->mutex);
    return 0;
}

/*
 * Initiate a call from a specified originating TU to a specified target TU.
 *   If the originating TU is not in the TU_DIAL_TONE state, then there is no effect.
 *   If the target TU is the same as the originating TU, then the TU transitions
 *     to the TU_BUSY_SIGNAL state.
 *   If the target TU already has a peer, or the target TU is not in the TU_ON_HOOK
 *     state, then the originating TU transitions to the TU_BUSY_SIGNAL state.
 *   Otherwise, the originating TU and the target TU are recorded as peers of each other
 *     (this causes the reference count of each of them to be incremented),
 *     the target TU transitions to the TU_RINGING state, and the originating TU
 *     transitions to the TU_RING_BACK state.
 *
 * In all cases, a notification of the resulting state of the originating TU is sent to
 * to the associated network client.  If the target TU has changed state, then its client
 * is also notified of its new state.
 *
 * If the caller of this function was unable to determine a target TU to be called,
 * it will pass NULL as the target TU.  In this case, the originating TU will transition
 * to the TU_ERROR state if it was in the TU_DIAL_TONE state, and there will be no
 * effect otherwise.  This situation is handled here, rather than in the caller,
 * because here we have knowledge of the current TU state and we do not want to introduce
 * the possibility of transitions to a TU_ERROR state from arbitrary other states,
 * especially in states where there could be a peer TU that would have to be dealt with.
 *
 * @param tu  The originating TU.
 * @param target  The target TU, or NULL if the caller of this function was unable to
 * identify a TU to be dialed.
 * @return 0 if successful, -1 if any error occurs that results in the originating
 * TU transitioning to the TU_ERROR state. 
 */
int tu_dial(TU *tu, TU *target) {
    // TO BE IMPLEMENTED
    if(tu == NULL) {
        debug("tu_dial");
        return -1;
    }
    char target_is_null = 0, target_is_equal = 0;
    if(target == NULL) target_is_null = 1;
    if(tu == target) target_is_equal = 1;
    char lock_target_first = 0;
    if(!target_is_null && target < tu) lock_target_first = 1;
    if(lock_target_first) {
        if(!target_is_null && !target_is_equal) P(&target->mutex);
        P(&tu->mutex);
    }
    else {
        P(&tu->mutex);
        if(!target_is_null && !target_is_equal) P(&target->mutex);
    }
    char target_state_changed = 0;
    if(target_is_null && tu->state == TU_DIAL_TONE) {
        tu->state = TU_ERROR;
    }
    else if(tu->state == TU_DIAL_TONE) {
        if(target_is_equal || target->peer != NULL || target->state != TU_ON_HOOK) {
            if(tu == target) debug("tu == target");
            if(target->peer != NULL) debug("target->peer != NULL");
            if(target->state != TU_ON_HOOK) debug("target->state != TU_ON_HOOK");
            tu->state = TU_BUSY_SIGNAL;
        }
        else {
            target_state_changed = 1;
            if(lock_target_first) {
                V(&tu->mutex);
                V(&target->mutex);
            }
            else {
                V(&target->mutex);
                V(&tu->mutex);
            }
            tu_ref(tu, "tu_dial");
            tu_ref(target, "tu_dial");
            if(lock_target_first) {
                P(&target->mutex);
                P(&tu->mutex);
            }
            else {
                P(&tu->mutex);
                P(&target->mutex);
            }
            target->state = TU_RINGING;
            tu->state = TU_RING_BACK;
            tu->peer = target;
            target->peer = tu;
            debug("tu_dial: Connecting ext %d to %d\n", tu->fd, target->fd);
        }
    }
    char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
    strcpy(tu_buf, tu_state_names[tu->state]);
    if(tu->state == TU_ON_HOOK || tu->state == TU_CONNECTED) {
        char num[12];
        if(tu->state == TU_ON_HOOK) sprintf(num, " %d", tu->fd);
        else sprintf(num, " %d", tu->peer->fd);
        tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
        strcat(tu_buf, num);
    }
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
    strcat(tu_buf, EOL);
    Write(tu->fd, tu_buf, strlen(tu_buf));
    Free(tu_buf);

    if(target_state_changed) {
        debug("tu_dial: Notifying target (ext %d)...\n", target->fd);
        char *target_buf = Malloc(strlen(tu_state_names[target->state]) + 1);
        strcpy(target_buf, tu_state_names[target->state]);
        if(target->state == TU_ON_HOOK || target->state == TU_CONNECTED) {
            char num[12];
            if(target->state == TU_ON_HOOK) sprintf(num, " %d", target->fd);
            else sprintf(num, " %d", target->peer->fd);
            target_buf = Realloc(target_buf, strlen(target_buf) + strlen(num) + 1);
            strcat(target_buf, num);
        }
        target_buf = Realloc(target_buf, strlen(target_buf) + 3);
        strcat(target_buf, EOL);
        Write(target->fd, target_buf, strlen(target_buf));
        Free(target_buf);
    }
    if(tu->state == TU_ERROR) {
        if(lock_target_first) {
            V(&tu->mutex);
            if(!target_is_null && !target_is_equal) V(&target->mutex);
        }
        else {
            if(!target_is_null && !target_is_equal) V(&target->mutex);
            V(&tu->mutex);
        }
        debug("tu_dial");
        return -1;
    }
    if(lock_target_first) {
        V(&tu->mutex);
        if(!target_is_null && !target_is_equal) V(&target->mutex);
    }
    else {
        if(!target_is_null && !target_is_equal) V(&target->mutex);
        V(&tu->mutex);
    }
    return 0;
}

/*
 * Take a TU receiver off-hook (i.e. pick up the handset).
 *   If the TU is in neither the TU_ON_HOOK state nor the TU_RINGING state,
 *     then there is no effect.
 *   If the TU is in the TU_ON_HOOK state, it goes to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RINGING state, it goes to the TU_CONNECTED state,
 *     reflecting an answered call.  In this case, the calling TU simultaneously
 *     also transitions to the TU_CONNECTED state.
 *
 * In all cases, a notification of the resulting state of the specified TU is sent to
 * to the associated network client.  If a peer TU has changed state, then its client
 * is also notified of its new state.
 *
 * @param tu  The TU that is to be picked up.
 * @return 0 if successful, -1 if any error occurs that results in the originating
 * TU transitioning to the TU_ERROR state. 
 */
int tu_pickup(TU *tu) {
    // TO BE IMPLEMENTED
    if(tu == NULL) {
        debug("tu_pickup");
        return -1;
    }
    char peer_is_null = 0;
    P(&tu->mutex);
    if(tu->peer == NULL) peer_is_null = 1;
    if(peer_is_null && tu->state == TU_RINGING) {
        debug("tu_pickup");
        V(&tu->mutex);
        return -1;
    }
    V(&tu->mutex);
    char lock_peer_first = 0;
    if(!peer_is_null && tu->peer < tu) lock_peer_first = 1;
    if(lock_peer_first) {
        if(!peer_is_null) P(&tu->peer->mutex);
        P(&tu->mutex);
    }
    else {
        P(&tu->mutex);
        if(!peer_is_null) P(&tu->peer->mutex);
    }
    char peer_state_changed = 0;
    if(tu->state == TU_ON_HOOK) {
        tu->state = TU_DIAL_TONE;
    }
    else if(tu->state == TU_RINGING) {
        tu->state = TU_CONNECTED;
        peer_state_changed = 1;
        tu->peer->state = TU_CONNECTED;
    }
    char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
    strcpy(tu_buf, tu_state_names[tu->state]);
    if(tu->state == TU_ON_HOOK || tu->state == TU_CONNECTED) {
        char num[12];
        if(tu->state == TU_ON_HOOK) sprintf(num, " %d", tu->fd);
        else sprintf(num, " %d", tu->peer->fd);
        tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
        strcat(tu_buf, num);
    }
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
    strcat(tu_buf, EOL);
    Write(tu->fd, tu_buf, strlen(tu_buf));
    Free(tu_buf);
    if(peer_state_changed) {
        char *peer_buf = Malloc(strlen(tu_state_names[tu->peer->state]) + 1);
        strcpy(peer_buf, tu_state_names[tu->peer->state]);
        if(tu->peer->state == TU_ON_HOOK || tu->peer->state == TU_CONNECTED) {
            char num[12];
            if(tu->state == TU_ON_HOOK) sprintf(num, " %d", tu->peer->fd);
            else sprintf(num, " %d", tu->fd);
            peer_buf = Realloc(peer_buf, strlen(peer_buf) + strlen(num) + 1);
            strcat(peer_buf, num);
        }
        peer_buf = Realloc(peer_buf, strlen(peer_buf) + 3);
        strcat(peer_buf, EOL);
        Write(tu->peer->fd, peer_buf, strlen(peer_buf));
        Free(peer_buf);
    }
    if(tu->state == TU_ERROR) {
        if(lock_peer_first) {
            V(&tu->mutex);
            if(!peer_is_null) V(&tu->peer->mutex);
        }
        else {
            if(!peer_is_null) V(&tu->peer->mutex);
            V(&tu->mutex);
        }
        return -1;
    }
    if(lock_peer_first) {
        V(&tu->mutex);
        if(!peer_is_null) V(&tu->peer->mutex);
    }
    else {
        if(!peer_is_null) V(&tu->peer->mutex);
        V(&tu->mutex);
    }
    return 0;
}

/*
 * Hang up a TU (i.e. replace the handset on the switchhook).
 *
 *   If the TU is in the TU_CONNECTED or TU_RINGING state, then it goes to the
 *     TU_ON_HOOK state.  In addition, in this case the peer TU (the one to which
 *     the call is currently connected) simultaneously transitions to the TU_DIAL_TONE
 *     state.
 *   If the TU was in the TU_RING_BACK state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the calling TU (which is in the TU_RINGING state)
 *     simultaneously transitions to the TU_ON_HOOK state.
 *   If the TU was in the TU_DIAL_TONE, TU_BUSY_SIGNAL, or TU_ERROR state,
 *     then it goes to the TU_ON_HOOK state.
 *
 * In all cases, a notification of the resulting state of the specified TU is sent to
 * to the associated network client.  If a peer TU has changed state, then its client
 * is also notified of its new state.
 *
 * @param tu  The tu that is to be hung up.
 * @return 0 if successful, -1 if any error occurs that results in the originating
 * TU transitioning to the TU_ERROR state. 
 */
int tu_hangup(TU *tu) {
    // TO BE IMPLEMENTED
    if(tu == NULL) {
        debug("tu_hangup");
        return -1;
    }
    char peer_is_null = 0;
    P(&tu->mutex);
    if(tu->peer == NULL) peer_is_null = 1;
    if(peer_is_null && (tu->state == TU_CONNECTED || tu->state == TU_RINGING || tu->state == TU_RING_BACK)) {
        debug("tu_hangup");
        V(&tu->mutex);
        return -1;
    }
    V(&tu->mutex);
    char lock_peer_first = 0;
    if(!peer_is_null && tu->peer < tu) lock_peer_first = 1;
    if(lock_peer_first) {
        if(!peer_is_null) P(&tu->peer->mutex);
        P(&tu->mutex);
    }
    else {
        P(&tu->mutex);
        if(!peer_is_null) P(&tu->peer->mutex);
    }
    debug("tu_hangup: Reference count is %d\n", tu->ref_count);
    char peer_state_changed = 0;
    char set_to_null = 0;
    if(tu->state == TU_CONNECTED || tu->state == TU_RINGING) {
        tu->state = TU_ON_HOOK;
        peer_state_changed = 1;
        tu->peer->state = TU_DIAL_TONE;
        set_to_null = 1;
        debug("Set to null\n");
    }
    else if(tu->state == TU_RING_BACK) {
        tu->state = TU_ON_HOOK;
        peer_state_changed = 1;
        tu->peer->state = TU_ON_HOOK;
        set_to_null = 1;
        debug("Set to null\n");
    }
    else if(tu->state == TU_DIAL_TONE || tu->state == TU_BUSY_SIGNAL || tu->state == TU_ERROR) {
        tu->state = TU_ON_HOOK;
    }
    char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
    strcpy(tu_buf, tu_state_names[tu->state]);
    if(tu->state == TU_ON_HOOK || tu->state == TU_CONNECTED) {
        char num[12];
        if(tu->state == TU_ON_HOOK) sprintf(num, " %d", tu->fd);
        else sprintf(num, " %d", tu->peer->fd);
        tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
        strcat(tu_buf, num);
    }
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
    strcat(tu_buf, EOL);
    write(tu->fd, tu_buf, strlen(tu_buf));
    Free(tu_buf);
    if(peer_state_changed) {
        if(peer_is_null) {
            debug("tu_hangup");
            V(&tu->mutex);
            return -1;
        }
        char *peer_buf = Malloc(strlen(tu_state_names[tu->peer->state]) + 1);
        strcpy(peer_buf, tu_state_names[tu->peer->state]);
        if(tu->peer->state == TU_ON_HOOK || tu->peer->state == TU_CONNECTED) {
            char num[12];
            if(tu->peer->state == TU_ON_HOOK) sprintf(num, " %d", tu->peer->fd);
            else sprintf(num, " %d", tu->fd);
            peer_buf = Realloc(peer_buf, strlen(peer_buf) + strlen(num) + 1);
            strcat(peer_buf, num);
        }
        peer_buf = Realloc(peer_buf, strlen(peer_buf) + 3);
        strcat(peer_buf, EOL);
        write(tu->peer->fd, peer_buf, strlen(peer_buf));
        Free(peer_buf);
    }
    if(tu->state == TU_ERROR) {
        if(lock_peer_first) {
            V(&tu->mutex);
            if(!peer_is_null) V(&tu->peer->mutex);
        }
        else {
            if(!peer_is_null) V(&tu->peer->mutex);
            V(&tu->mutex);
        }
        return -1;
    }
    if(set_to_null) {
        if(lock_peer_first) {
            V(&tu->mutex);
            if(!peer_is_null) V(&tu->peer->mutex);
        }
        else {
            if(!peer_is_null) V(&tu->peer->mutex);
            V(&tu->mutex);
        }
        tu_unref(tu->peer, "tu_hangup");
        tu_unref(tu, "tu_hangup");
        if(lock_peer_first) {
            if(!peer_is_null) P(&tu->peer->mutex);
            P(&tu->mutex);
        }
        else {
            P(&tu->mutex);
            if(!peer_is_null) P(&tu->peer->mutex);
        }
        sem_t *mt = &tu->peer->mutex;
        tu->peer->peer = NULL;
        tu->peer = NULL;
        debug("Finished setting to null\n");
        if(lock_peer_first) {
            V(&tu->mutex);
            V(mt);
        }
        else {
            V(mt);
            V(&tu->mutex);
        }
        debug("End of tu_hangup\n");
        return 0;
    }
    if(lock_peer_first) {
        V(&tu->mutex);
        if(!peer_is_null) V(&tu->peer->mutex);
    }
    else {
        if(!peer_is_null) V(&tu->peer->mutex);
        V(&tu->mutex);
    }
    debug("End of tu_hangup\n");
    return 0;
}

/*
 * "Chat" over a connection.
 *
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged and a notification containing
 * the current state is sent to the TU sending the chat.
 *
 * @param tu  The tu sending the chat.
 * @param msg  The message to be sent.
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg) {
    // TO BE IMPLEMENTED
    if(tu == NULL) {
        debug("tu_chat");
        return -1;
    }
    char peer_is_null = 0;
    P(&tu->mutex);
    if(tu->peer == NULL) peer_is_null = 1;
    if(peer_is_null && tu->state == TU_CONNECTED) {
        debug("tu_chat error");
        V(&tu->mutex);
        return -1;
    }
    V(&tu->mutex);
    char lock_peer_first = 0;
    if(!peer_is_null && tu->peer < tu) lock_peer_first = 1;
    if(lock_peer_first) {
        if(!peer_is_null) P(&tu->peer->mutex);
        P(&tu->mutex);
    }
    else {
        P(&tu->mutex);
        if(!peer_is_null) P(&tu->peer->mutex);
    }
    if(tu->state != TU_CONNECTED) {
        char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
        strcpy(tu_buf, tu_state_names[tu->state]);
        if(tu->state == TU_ON_HOOK) {
            char num[12];
            sprintf(num, " %d", tu->fd);
            tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
            strcat(tu_buf, num);
        }
        tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
        strcat(tu_buf, EOL);
        Write(tu->fd, tu_buf, strlen(tu_buf));
        Free(tu_buf);
        if(lock_peer_first) {
            V(&tu->mutex);
            if(!peer_is_null) V(&tu->peer->mutex);
        }
        else {
            if(!peer_is_null) V(&tu->peer->mutex);
            V(&tu->mutex);
        }
        return -1;
    }
    char *tu_buf = Malloc(strlen(tu_state_names[tu->state]) + 1);
    strcpy(tu_buf, tu_state_names[tu->state]);
    char num[12];
    sprintf(num, " %d", tu->peer->fd);
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + strlen(num) + 1);
    strcat(tu_buf, num);
    tu_buf = Realloc(tu_buf, strlen(tu_buf) + 3);
    strcat(tu_buf, EOL);
    Write(tu->fd, tu_buf, strlen(tu_buf));
    Free(tu_buf);
    char *msg_buf = Malloc(strlen("CHAT ") + 1);
    strcpy(msg_buf, "CHAT ");
    msg_buf = Realloc(msg_buf, strlen(msg_buf) + strlen(msg) + 1);
    strcat(msg_buf, msg);
    msg_buf = Realloc(msg_buf, strlen(msg_buf) + 3);
    strcat(msg_buf, EOL);
    Write(tu->peer->fd, msg_buf, strlen(msg_buf));
    Free(msg_buf);
    if(lock_peer_first) {
        V(&tu->mutex);
        if(!peer_is_null) V(&tu->peer->mutex);
    }
    else {
        if(!peer_is_null) V(&tu->peer->mutex);
        V(&tu->mutex);
    }
    return 0;
}
