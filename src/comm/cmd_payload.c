/**
 * @file cmd_payload.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "comm/dbus_comm.h"
#include "comm/cmd_payload.h"
#include "sched/workqueue.h"

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
remote_cmd_t *create_remote_cmd()
{
    remote_cmd_t *cmd;

    cmd = calloc(1, sizeof(*cmd));
    if (!cmd) {
        return NULL;
    }

    return cmd;
}

void delete_remote_cmd(remote_cmd_t *cmd)
{
    if (!cmd) {
        LOG_WARN("Unable to delete cmd: null pointer");
        return;
    }

    free(cmd);
}

local_cmd_t *create_local_cmd()
{
    local_cmd_t *cmd = NULL;

    cmd = calloc(1, sizeof(*cmd));
    if (!cmd) {
        return NULL;
    }

    return cmd;
}

void delete_local_cmd(local_cmd_t *cmd)
{
    if (!cmd) {
        LOG_WARN("Unable to delete cmd: null pointer");
        return;
    }

    free(cmd);
}

void remote_cmd_init(remote_cmd_t *cmd, const char *component_id, int32_t umid, \
                     int32_t opcode, uint8_t priority, uint8_t duration)
{
    int32_t i;

    cmd->component_id = component_id;
    cmd->umid = umid;
    cmd->opcode = opcode;
    cmd->prio = priority;
    cmd->duration = duration;
    cmd->entry_count = 0;

    for (i = 0; i < MAX_ENTRIES; i++) {
        cmd->entries[i].key = NULL;
        cmd->entries[i].data_type = 0;
        cmd->entries[i].data_length = 0;
    }
}

int32_t remote_cmd_add_string(remote_cmd_t *cmd, const char *key, const char *value)
{
    payload_t *entry;
    if (cmd->entry_count >= MAX_ENTRIES)
        return -1;

    entry = &cmd->entries[cmd->entry_count++];
    entry->key = key;
    entry->data_type = DBUS_TYPE_STRING;
    entry->data_length = strlen(value) + 1;;
    entry->value.str = value;

    return 0;
}

int32_t remote_cmd_add_int(remote_cmd_t *cmd, const char *key, int32_t value)
{
    payload_t *entry;

    if (cmd->entry_count >= MAX_ENTRIES)
        return -1;

    entry = &cmd->entries[cmd->entry_count++];
    entry->key = key;
    entry->data_type = DBUS_TYPE_INT32;
    entry->data_length = sizeof(int32_t);
    entry->value.i32 = value;

    return 0;
}

/*
 * Local task runs only in the task handler. Its behavior depends on the
 * current state of the handler: it may run immediately after being pushed
 * into the workqueue, or it may wait until the handler is free if a
 * previous task is still blocking.
 */
int32_t create_local_simple_task(uint8_t priority, uint8_t duration, \
                                 uint32_t opcode)
{
    work_t *work = create_work(WORK_TYPE_LOCAL, \
                               priority, duration, opcode, NULL);
    if (!work) {
        LOG_ERROR("Failed to create work from cmd");
        return -EINVAL;
    }

    push_work(get_wq(SYSTEM_WQ), work);

    return 0;
}

/*
 * Remote task is created locally and pushed into the workqueue like a
 * local task. The difference is that it is processed on the target
 * service (e.g. System Manager). It is not blocked in this service,
 * because all required actions are sent as commands to another service
 * via DBus.
 */
int32_t create_remote_task(uint8_t priority, void *data)
{
    work_t *work;

    work = create_work(WORK_TYPE_REMOTE, priority, WORK_DURATION_SHORT, \
                       OP_DBUS_SENT_CMD, data);
    if (!work) {
        LOG_ERROR("Failed to create work from cmd");
        return -EINVAL;
    }

    push_work(get_wq(SYSTEM_WQ), work);

    return 0;
}

remote_cmd_t *create_remote_task_data(uint8_t priority, uint8_t duration, \
                                      uint32_t opcode)
{
    remote_cmd_t *cmd;

    cmd = create_remote_cmd();
    if (!cmd) {
        return NULL;
    }

    remote_cmd_init(cmd, COMP_NAME, COMP_ID, opcode, priority, duration);

    return cmd;
}

/*
 * After a remote command is sent to another service, it is used to create
 * a local task on that service. The command must contain specific data so
 * the service knows what to do.
 *
 * Each remote command requires at least two steps:
 * 1. Create the remote command data (defines the expected operation on target)
 * 2. Create the local task containing that data
 */
int32_t create_remote_simple_task(uint8_t priority, uint8_t duration, uint32_t opcode)
{
    remote_cmd_t *cmd;

    cmd = create_remote_task_data(priority, duration, opcode);
    if (!cmd) {
        LOG_ERROR("Failed to create remote command payload");
        return -EINVAL;
    }

    return create_remote_task(WORK_PRIO_HIGH, (void *)cmd);
}
