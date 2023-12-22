#include <pthread.h>

#include "utils/log.h"
#include "utils/utextend.h"

#include "trans_data_manager.h"

typedef struct data_elem {
    uint64_t ptr;

    neu_msg_t *msg;
    int64_t    timestamp;

    UT_hash_handle hh;
} data_elem_t;

struct neu_td_mgr {
    pthread_mutex_t mutex;

    data_elem_t *datas;
};

neu_td_mgr_t *neu_td_mgr_create()
{
    struct neu_td_mgr *manager = calloc(1, sizeof(struct neu_td_mgr));

    pthread_mutex_init(&manager->mutex, NULL);

    return NULL;
}

void neu_td_mgr_destroy(neu_td_mgr_t *manager)
{
    pthread_mutex_lock(&manager->mutex);
    data_elem_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, manager->datas, el, tmp)
    {
        HASH_DEL(manager->datas, el);
        // free msg
        free(el);
    }

    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);

    free(manager)
}

void neu_td_mgr_reg(neu_td_mgr_t *manager, neu_msg_t *msg)
{
    return NULL;
}

void neu_td_mgr_free(neu_td_mgr_t *manager, neu_msg_t *msg)
{
    return;
}

bool neu_td_mgr_exist(neu_td_mgr_t *manager, neu_msg_t *msg)
{
    return true;
}

void neu_td_mgr_recycle(neu_td_mgr_t *manager)
{
    return;
}
