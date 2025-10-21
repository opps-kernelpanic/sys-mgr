/**
 * @file main.h
 *
 */

#ifndef G_MAIN_H
#define G_MAIN_H
/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <dbus/dbus.h>

#include "sched/workqueue.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct op_handler {
    struct list_head handler_lst;       /* List of registered opcode handlers */
} op_t;

typedef struct comm_handler {
    DBusConnection *dbus_conn;
    int32_t event;
} comm_t;

typedef struct conf_data {
    int8_t brightness;
    bool als_en;
    bool imu_en;
} conf_t;

typedef struct ctx {
    sig_atomic_t run;
    wq_ctx_t *wqs;
    op_t op;
    comm_t comm;
    conf_t cfg;
} ctx_t;

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
ctx_t *get_ctx(void);
int32_t process_opcode(uint32_t opcode, void *data);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* G_MAIN_H */
