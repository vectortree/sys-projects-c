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
    //sem_t sem;
    pthread_mutex_t mutex;
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
    pthread_mutex_init(&pbx->mutex, NULL);
    //Sem_init(&pbx->sem, 0, 1);
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
    pthread_mutex_lock(&pbx->mutex);
    struct pbx_client *node = pbx->registry->next;
    while(node != pbx->registry) {
        shutdown(node->ext, SHUT_RDWR);
        node = node->next;
    }
    //while(pbx->size > 0);
    node = pbx->registry->next;
    while(node != pbx->registry) {
        struct pbx_client *temp = node;
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node = node->next;
        Free(temp);
    }
    Free(pbx->registry);
    Free(pbx);
    pthread_mutex_unlock(&pbx->mutex);
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
    if(pbx != NULL && pbx->size >= PBX_MAX_EXTENSIONS) return -1;
    pthread_mutex_lock(&pbx->mutex);
    struct pbx_client *pbx_client = Malloc(sizeof(struct pbx_client));
    if(pbx_client == NULL) return -1;
    tu_ref(tu, "pbx_register");
    if(tu_set_extension(tu, ext) < 0) return -1;
    pbx_client->tu = tu;
    pbx_client->ext = ext;
    pbx_client->next = pbx->registry;
    pbx_client->prev = pbx->registry->prev;
    pbx->registry->prev->next = pbx_client;
    pbx->registry->prev = pbx_client;
    //P(&pbx->sem);
    ++pbx->size;
    //V(&pbx->sem);
    pthread_mutex_unlock(&pbx->mutex);
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
    if(pbx != NULL && pbx->size <= 0) return -1;
    pthread_mutex_lock(&pbx->mutex);
    struct pbx_client *node = pbx->registry->next;
    char flag = 0;
    while(node != pbx->registry) {
        if(node->tu == tu) {
            flag = 1;
            node->prev->next = node->next;
            node->next->prev = node->prev;
            break;
        }
        node = node->next;
    }
    if(!flag) {
        pthread_mutex_unlock(&pbx->mutex);
        return -1;
    }
    if(tu_hangup(tu) < 0) return -1;
    tu_unref(tu, "pbx_unregister");
    //P(&pbx->sem);
    --pbx->size;
    //V(&pbx->sem);
    pthread_mutex_unlock(&pbx->mutex);
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
    pthread_mutex_lock(&pbx->mutex);
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
        if(tu_dial(tu, NULL) < 0) {
            pthread_mutex_unlock(&pbx->mutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&pbx->mutex);
    return 0;
}
