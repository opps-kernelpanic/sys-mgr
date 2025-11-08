/**
 * @file workqueue.h
 *
 */

#ifndef G_WORKQUEUE_H
#define G_WORKQUEUE_H
/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

#include "list.h"

/*********************
 *      DEFINES
 *********************/
/*
 * Depends on application to modify these values
 */
#define NR_WORKQUEUE                    1
#define WORKERS_PER_QUEUE               2


#define SYSTEM_WQ                       0
/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    WORK_TYPE_LOCAL = 0,
    WORK_TYPE_REMOTE,
} work_type_t;

typedef enum {
    WORK_PRIO_LOW = 0,
    WORK_PRIO_NORMAL,
    WORK_PRIO_HIGH,
    WORK_PRIO_URGENT,
} work_priority_t;

typedef enum {
    WORK_DURATION_SHORT = 0,
    WORK_DURATION_LONG,
} work_duration_t;

typedef struct work {
    struct list_head node;
    work_type_t type;
    work_priority_t prio;
    work_duration_t duration;
    uint32_t opcode;
    void *data;
} work_t;

typedef struct workqueue {
    struct list_head list;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    atomic_int active_cnt;
} workqueue_t;

typedef struct wq_ctx {
    workqueue_t *wq;
    pthread_t workers[WORKERS_PER_QUEUE];
    int32_t nr_workers;  /* number of successfully created workers */
} wq_ctx_t;

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/*=====================
 * Setter functions
 *====================*/

/*=====================
 * Getter functions
 *====================*/

/*=====================
 * Other functions
 *====================*/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
work_t *create_work(uint8_t type, uint8_t priority, uint8_t duration, \
                    uint32_t opcode, void *data);
void workqueue_complete_work(workqueue_t *wq, work_t *w);
void push_work(workqueue_t *wq, work_t *w);
work_t *pop_work_wait_safe(workqueue_t *wq);
void workqueue_handler_wakeup(workqueue_t *wq);
int32_t workqueue_handler_wakeup_all(void);
int32_t workqueue_active_count(workqueue_t *wq);

void *workqueue_handler(void* arg);

workqueue_t *get_wq(int32_t index);

int32_t workqueue_init();
void workqueue_deinit();
/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* G_WORKQUEUE_H */
