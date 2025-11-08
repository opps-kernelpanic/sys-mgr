/**
 * @file workqueue.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
// #define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>
#include <errno.h>

#include "comm/dbus_comm.h"
#include "sched/workqueue.h"
#include "main.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/
static workqueue_t *workqueue_create(void)
{
    workqueue_t *wq;

    wq = calloc(1, sizeof(*wq));
    if (!wq)
        return NULL;

    INIT_LIST_HEAD(&wq->list);
    pthread_mutex_init(&wq->mutex, NULL);
    pthread_cond_init(&wq->cond, NULL);
    atomic_store(&wq->active_cnt, 0);

    return wq;
}

static void workqueue_destroy(workqueue_t *wq)
{
    work_t *pos, *n;

    if (wq == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return;
    } 

    pthread_mutex_lock(&wq->mutex);
    list_for_each_entry_safe(pos, n, &wq->list, node) {
        list_del(&pos->node);
        free(pos->data);
        free(pos);
    }
    pthread_mutex_unlock(&wq->mutex);

    pthread_mutex_destroy(&wq->mutex);
    pthread_cond_destroy(&wq->cond);
    free(wq);
}

static int32_t create_workers(wq_ctx_t *ctx)
{
    int32_t i, ret;

    ctx->nr_workers = 0;

    for (i = 0; i < WORKERS_PER_QUEUE; i++) {
        ret = pthread_create(&ctx->workers[i], NULL,
                             workqueue_handler, ctx->wq);
        if (ret) {
            LOG_FATAL("Failed to create worker thread: %s", strerror(ret));
            return -ENOMEM;
        }

        ctx->nr_workers++;
        LOG_TRACE("Worker %d created", i);
    }

    return 0;
}

static void rollback_workqueues(int32_t upto)
{
    int32_t i, w;
    wq_ctx_t *wq_ctxs = get_ctx()->wqs;

    for (i = 0; i < upto; i++) {
        for (w = 0; w < wq_ctxs[i].nr_workers; w++)
            pthread_join(wq_ctxs[i].workers[w], NULL);

        workqueue_destroy(wq_ctxs[i].wq);
    }

    free(wq_ctxs);
    wq_ctxs = NULL;
}
/**********************
 *   GLOBAL FUNCTIONS
 **********************/
work_t *create_work(uint8_t type, uint8_t priority, uint8_t duration, \
                    uint32_t opcode, void *data)
{
    work_t *w;

    w = calloc(1, sizeof(*w));
    if (!w)
        return NULL;

    w->type = type;
    w->prio = priority;
    w->duration = duration;
    w->opcode = opcode;
    w->data = data;
    INIT_LIST_HEAD(&w->node);

    LOG_TRACE("Created work for opcode: %d", w->opcode);
    return w;
}

void push_work(workqueue_t *wq, work_t *w)
{
    if (wq == NULL || w == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return;
    } 

    pthread_mutex_lock(&wq->mutex);
    list_add_tail(&w->node, &wq->list);
    atomic_fetch_add(&wq->active_cnt, 1);
    pthread_cond_signal(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);
}

work_t *pop_work_wait_safe(workqueue_t *wq)
{
    work_t *w = NULL;

    if (wq == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return NULL;
    } 

    pthread_mutex_lock(&wq->mutex);
    while (list_empty(&wq->list) && get_ctx()->run)
        pthread_cond_wait(&wq->cond, &wq->mutex);

    if (list_empty(&wq->list)) {
        pthread_mutex_unlock(&wq->mutex);
        return NULL;
    }

    w = list_first_entry(&wq->list, work_t, node);
    list_del(&w->node);

    pthread_mutex_unlock(&wq->mutex);
    return w;
}

void workqueue_complete_work(workqueue_t *wq, work_t *w)
{
    if (wq == NULL || w == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return;
    } 

    if (w->data)
        free(w->data);
    free(w);

    atomic_fetch_sub(&wq->active_cnt, 1);
    if (atomic_load(&wq->active_cnt) == 0) {
        pthread_mutex_lock(&wq->mutex);
        pthread_cond_broadcast(&wq->cond);
        pthread_mutex_unlock(&wq->mutex);
    }
}

void workqueue_handler_wakeup(workqueue_t *wq)
{
    if (wq == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return;
    } 

    pthread_mutex_lock(&wq->mutex);
    pthread_cond_broadcast(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);
}

int32_t workqueue_handler_wakeup_all(void)
{
    int32_t i;

    for (i = 0; i < NR_WORKQUEUE; i++) {
        workqueue_handler_wakeup(get_wq(i));
        LOG_TRACE("WQ %d wakeup all handler", i);
    }

    return 0;
}

int32_t workqueue_active_count(workqueue_t *wq)
{
    if (wq == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return -EINVAL;
    } 
    return atomic_load(&wq->active_cnt);
}

workqueue_t *get_wq(int32_t index)
{
    wq_ctx_t *wq_ctxs = get_ctx()->wqs;

    if (index > NR_WORKQUEUE) {
        LOG_ERROR("Workqueue data is invalid");
        return NULL;
    }
        
    return wq_ctxs[index].wq;
}

static inline void set_wq(workqueue_t *wq, int32_t index)
{
    wq_ctx_t *wq_ctxs = get_ctx()->wqs;

    if (wq == NULL) {
        LOG_ERROR("Workqueue data is invalid");
        return;
    } 

    wq_ctxs[index].wq = wq;
}

int32_t workqueue_init(void)
{
    int32_t i, ret;
    workqueue_t *wq = NULL;
    wq_ctx_t *ctx = NULL;

    ctx = calloc(NR_WORKQUEUE, sizeof(*ctx));
    if (!ctx) {
        LOG_FATAL("Unable to allocate %d workqueue contexts", NR_WORKQUEUE);
        return -ENOMEM;
    }

    LOG_INFO("Init %d workers per workqueue, total %d workqueues",
             WORKERS_PER_QUEUE, NR_WORKQUEUE);

    get_ctx()->wqs = ctx;

    for (i = 0; i < NR_WORKQUEUE; i++) {
        wq = workqueue_create();
        if (!wq) {
            LOG_FATAL("Unable to create workqueue, index %d", i);
            rollback_workqueues(i);
            return -ENOMEM;
        }

        ctx[i].wq = wq;
        LOG_TRACE("Workqueue %d created", i);

        ret = create_workers(&ctx[i]);
        if (ret) {
            if (NR_WORKQUEUE > 1)
                LOG_ERROR("Failed to create %d workers for queue %d",
                          WORKERS_PER_QUEUE, i);
            else
                LOG_FATAL("Failed to create %d workers for queue %d",
                          WORKERS_PER_QUEUE, i);

            rollback_workqueues(i + 1);
            return -ENOMEM;
        }

        LOG_TRACE("Created %d workers for queue %d",
                  WORKERS_PER_QUEUE, i);
    }

    return 0;
}

void workqueue_deinit(void)
{
    int32_t i, w;
    wq_ctx_t *wq_ctxs = get_ctx()->wqs;

    workqueue_handler_wakeup_all();

    for (i = 0; i < NR_WORKQUEUE; i++) {
        for (w = 0; w < wq_ctxs[i].nr_workers; w++) {
            pthread_join(wq_ctxs[i].workers[w], NULL);
            LOG_TRACE("WQ %d - Worker %d: Exited", i, w);
        }

        workqueue_destroy(get_wq(i));
        LOG_TRACE("WQ %d destroyed", i);
    }

    free(wq_ctxs);
    get_ctx()->wqs = NULL;
}
