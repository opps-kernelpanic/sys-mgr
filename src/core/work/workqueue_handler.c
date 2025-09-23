/**
 * @file workqueue_handler.c
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

#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>

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

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void *workqueue_handler(void* arg)
{
    work_t *w = NULL;
    int32_t ret = 0;
    pthread_t tid = pthread_self();
    workqueue_t *wq = NULL;

    wq = (workqueue_t *)arg;
    if (wq == NULL) {
        LOG_FATAL("Workqueue handler unable to get workqueue");
        return NULL;
    }

    LOG_INFO("Workqueue handler started - thread ID: %lu", (unsigned long)tid);

    // LOG_INFO("Task handler is running...");
    while (get_ctx()->run) {
        // remove sleep to handle parallel tasks faster after request
        // usleep(200000);
        LOG_TRACE("Workqueue handler ID [%lu] --> waiting for new task...", \
                  (unsigned long)tid);
        w = pop_work_wait_safe(wq);
        /*
         * After a work item is popped from the workqueue, it is no longer
         * linked to the work list. This means:
         *
         *   For serial (single-threaded) work: the work structure can also be
         *   freed immediately after the task is processed, as no other entity
         *   can access it.
         *
         * NOTE: Do not free the work item if it is expected to be re-queued or
         *       if any other component may still hold a reference to it.
         */
        if (w == NULL) {
            LOG_INFO("Workqueue handler ID [%lu] is exiting...", \
                     (unsigned long)tid);
            break;
        }

        LOG_TRACE("Handler ID [%lu] type: [%d] - priority [%d] - opcode [%d]", \
                  (unsigned long)tid, w->type, w->prio , w->opcode);

        if (w->opcode <= OP_NONE || w->opcode >= OP_ID_END) {
            LOG_WARN("Invalid task specification:\n" \
                     "\tOpcode [%d]\n" \
                     "\tPriority [%d]\n" \
                     "\tDuration [%d] -> DELETE", \
                     w->opcode, w->prio , w->duration);

            workqueue_complete_work(wq, w);
        } else {
            /*
             * Run blocking task; return after it completes other tasks in
             * queue wait until it's done. or will be handled by another handler
             */

            ret = process_opcode(w->opcode, w->data);
            if (ret) {
                LOG_ERROR("Handler ID [%lu] task failed with ret=%d\n" \
                          "\tType: [%d] - priority [%d] - opcode [%d]", \
                          (unsigned long)tid, ret, w->type, w->prio, w->opcode);
            }
            // The working data structures for any tasks need to be freed
            workqueue_complete_work(wq, w);
        }
    };

    LOG_INFO("Workqueue handler ID [%lu] exited...", (unsigned long)tid);

    return NULL;
}

