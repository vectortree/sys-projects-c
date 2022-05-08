/*
 * PBX: simulates a Private Branch Exchange.
 */
#include <stdlib.h>
#include <pthread.h>

#include "pbx.h"
#include "debug.h"
#include "csapp.h"

struct pbx_client {
    TU *tu;
    int ext;
    struct pbx_client *next;
    struct pbx_client *prev;
};

struct pbx {
    sem_t w;
    sem_t mutex;
    int size;
    struct pbx_client *registry;
};

/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init() {
    // TO BE IMPLEMENTED
    PBX *pbx = Malloc(sizeof(PBX));
    if(pbx == NULL) return NULL;
    Sem_init(&pbx->mutex, 0, 1);
    Sem_init(&pbx->w, 0, 1);
    pbx->size = 0;
    pbx->registry = Malloc(sizeof(struct pbx_client));
    if(pbx->registry == NULL) return NULL;
    pbx->registry->next = pbx->registry;
    pbx->registry->prev = pbx->registry;
    return pbx;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed.  The PBX object itself is freed, and should not be used again.
 *
 * @param pbx  The PBX to be shut down.
 */
void pbx_shutdown(PBX *pbx) {
    // TO BE IMPLEMENTED
    if(pbx == NULL || pbx->registry == NULL) return;
    P(&pbx->mutex);
    struct pbx_client *node = pbx->registry->next;
    while(node != pbx->registry) {
        shutdown(node->ext, SHUT_RDWR);
        node = node->next;
    }
    debug("pbx_shutdown: (1) Thread count is %d\n", pbx->size);
    V(&pbx->mutex);
    P(&pbx->w);
    debug("pbx_shutdown: (2) Thread count is %d\n", pbx->size);
    //while(pbx->size > 0);
    V(&pbx->w);
    sem_destroy(&pbx->mutex);
    sem_destroy(&pbx->w);
    Free(pbx->registry);
    Free(pbx);
}

/*
 * Register a telephone unit with a PBX at a specified extension number.
 * This amounts to "plugging a telephone unit into the PBX".
 * The TU is initialized to the TU_ON_HOOK state.
 * The reference count of the TU is increased and the PBX retains this reference
 * for as long as the TU remains registered.
 * A notification of the assigned extension number is sent to the underlying network
 * client.
 *
 * @param pbx  The PBX registry.
 * @param tu  The TU to be registered.
 * @param ext  The extension number on which the TU is to be registered.
 * @return 0 if registration succeeds, otherwise -1.
 */
int pbx_register(PBX *pbx, TU *tu, int ext) {
    // TO BE IMPLEMENTED
    if(pbx == NULL || tu == NULL || ext < 0) return -1;
    P(&pbx->mutex);
    if(pbx->size >= PBX_MAX_EXTENSIONS) {
        V(&pbx->mutex);
        return -1;
    }
    struct pbx_client *pbx_client = Malloc(sizeof(struct pbx_client));
    if(pbx_client == NULL) {
        V(&pbx->mutex);
        return -1;
    }
    tu_ref(tu, "pbx_register");
    if(tu_set_extension(tu, ext) < 0) {
        V(&pbx->mutex);
        return -1;
    }
    pbx_client->tu = tu;
    pbx_client->ext = ext;
    pbx_client->next = pbx->registry;
    pbx_client->prev = pbx->registry->prev;
    pbx->registry->prev->next = pbx_client;
    pbx->registry->prev = pbx_client;
    ++pbx->size;
    if(pbx->size == 1) P(&pbx->w);
    V(&pbx->mutex);
    return 0;
}

/*
 * Unregister a TU from a PBX.
 * This amounts to "unplugging a telephone unit from the PBX".
 * The TU is disassociated from its extension number.
 * Then a hangup operation is performed on the TU to cancel any
 * call that might be in progress.
 * Finally, the reference held by the PBX to the TU is released.
 *
 * @param pbx  The PBX.
 * @param tu  The TU to be unregistered.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu) {
    // TO BE IMPLEMENTED
    if(pbx == NULL || tu == NULL) return -1;
    debug("Called pbx_unregister\n");
    P(&pbx->mutex);
    if(pbx != NULL && pbx->size <= 0) {
        V(&pbx->mutex);
        return -1;
    }
    debug("Calling tu_hangup in pbx_unregister\n");
    if(tu_hangup(tu) < 0) {
        V(&pbx->mutex);
        return -1;
    }
    debug("Called tu_hangup in pbx_unregister\n");
    struct pbx_client *node = pbx->registry->next;
    char flag = 0;
    while(node != pbx->registry) {
        if(node->tu == tu) {
            flag = 1;
            node->prev->next = node->next;
            node->next->prev = node->prev;
            tu_unref(tu, "pbx_unregister");
            Free(node);
            break;
        }
        node = node->next;
    }
    if(!flag) {
        V(&pbx->mutex);
        return -1;
    }
    --pbx->size;
    if(pbx->size == 0) V(&pbx->w);
    V(&pbx->mutex);
    debug("Returning from pbx_unregister\n");
    return 0;
}

/*
 * Use the PBX to initiate a call from a specified TU to a specified extension.
 *
 * @param pbx  The PBX registry.
 * @param tu  The TU that is initiating the call.
 * @param ext  The extension number to be called.
 * @return 0 if dialing succeeds, otherwise -1.
 */
int pbx_dial(PBX *pbx, TU *tu, int ext) {
    // TO BE IMPLEMENTED
    if(pbx == NULL || tu == NULL || ext < 0) return -1;
    P(&pbx->mutex);
    struct pbx_client *node = pbx->registry->next;
    char flag = 0;
    while(node != pbx->registry) {
        if(node->ext == ext) {
            flag = 1;
            tu_dial(tu, node->tu);
            break;
        }
        node = node->next;
    }
    if(!flag) {
        debug("pbx_dial: ext %d not found", ext);
        tu_dial(tu, NULL);
        V(&pbx->mutex);
        return -1;
    }
    V(&pbx->mutex);
    return 0;
}
