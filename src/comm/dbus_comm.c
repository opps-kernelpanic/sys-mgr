/**
 * @file dbus_comm.c
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
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <inttypes.h>
#include <dbus/dbus.h>

#include "comm/dbus_comm.h"
#include "comm/f_comm.h"
#include "comm/cmd_payload.h"
#include "sched/workqueue.h"
#include "main.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_EVENTS 2

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
// Encode remote_cmd_t into an existing DBusMessage
static int32_t encode_data_frame(DBusMessage *msg, const remote_cmd_t *cmd)
{
    DBusMessageIter iter, array_iter, struct_iter, variant_iter;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &cmd->component_id);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->umid);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->opcode);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->prio);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->duration);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(siiv)", &array_iter);

    for (int32_t i = 0; i < cmd->entry_count; ++i) {
        const payload_t *entry = &cmd->entries[i];

        dbus_message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);

        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &entry->key);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &entry->data_type);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &entry->data_length);

        const char *sig = NULL;
        switch (entry->data_type) {
            case DBUS_TYPE_STRING: sig = "s"; break;
            case DBUS_TYPE_INT32:  sig = "i"; break;
            case DBUS_TYPE_UINT32: sig = "u"; break;
            case DBUS_TYPE_DOUBLE: sig = "d"; break;
            default:
                LOG_ERROR("Unsupported data_type %d for key '%s'", entry->data_type, entry->key);
                return -EINVAL;
        }

        dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_VARIANT, sig, &variant_iter);

        switch (entry->data_type) {
            case DBUS_TYPE_STRING:
                dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &entry->value.str);
                break;
            case DBUS_TYPE_INT32:
                dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &entry->value.i32);
                break;
            case DBUS_TYPE_UINT32:
                dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32, &entry->value.u32);
                break;
            case DBUS_TYPE_DOUBLE:
                dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_DOUBLE, &entry->value.dbl);
                break;
        }

        dbus_message_iter_close_container(&struct_iter, &variant_iter);
        dbus_message_iter_close_container(&array_iter, &struct_iter);
    }

    dbus_message_iter_close_container(&iter, &array_iter);
    return 0;
}

// Decode DBusMessage into remote_cmd_t
static int32_t decode_data_frame(DBusMessage *msg, remote_cmd_t *out)
{
    DBusMessageIter iter, array_iter, struct_iter, variant_iter;

    if (!dbus_message_iter_init(msg, &iter)) {
        LOG_ERROR("Failed to init DBus iterator");
        return -EINVAL;
    }

    dbus_message_iter_get_basic(&iter, &out->component_id);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->umid);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->opcode);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->prio);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->duration);
    dbus_message_iter_next(&iter);

    dbus_message_iter_recurse(&iter, &array_iter);

    int32_t i = 0;
    while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT && \
           i < MAX_ENTRIES) {
        payload_t *entry = &out->entries[i];
        dbus_message_iter_recurse(&array_iter, &struct_iter);

        dbus_message_iter_get_basic(&struct_iter, &entry->key);
        dbus_message_iter_next(&struct_iter);

        dbus_message_iter_get_basic(&struct_iter, &entry->data_type);
        dbus_message_iter_next(&struct_iter);

        dbus_message_iter_get_basic(&struct_iter, &entry->data_length);
        dbus_message_iter_next(&struct_iter);

        dbus_message_iter_recurse(&struct_iter, &variant_iter);

        switch (entry->data_type) {
            case DBUS_TYPE_STRING:
                dbus_message_iter_get_basic(&variant_iter, &entry->value.str);
                break;
            case DBUS_TYPE_INT32:
                dbus_message_iter_get_basic(&variant_iter, &entry->value.i32);
                break;
            case DBUS_TYPE_UINT32:
                dbus_message_iter_get_basic(&variant_iter, &entry->value.u32);
                break;
            case DBUS_TYPE_DOUBLE:
                dbus_message_iter_get_basic(&variant_iter, &entry->value.dbl);
                break;
            default:
                LOG_WARN("Unsupported type %d for entry %d", entry->data_type, i);
                break;
        }

        dbus_message_iter_next(&array_iter);
        i++;
    }

    out->entry_count = i;
    return 0;
}

static int32_t dispatch_cmd_from_message(DBusMessage *msg)
{
    remote_cmd_t *cmd;
    work_t *work;
    int32_t i;

    cmd = create_remote_cmd();
    if (!cmd) {
        LOG_ERROR("Failed to allocate memory for cmd_data");
        return -ENOMEM;
    }

    if (decode_data_frame(msg, cmd)) {
        LOG_ERROR("Failed to decode DBus message");
        delete_remote_cmd(cmd);
        return -EINVAL;
    }

    LOG_DEBUG("Received frame from component: %s", cmd->component_id);
    LOG_DEBUG("Message ID: %d, Opcode: %d, Prio %d, Duration %d", cmd->umid, \
              cmd->opcode, cmd->prio, cmd->duration);

    for (i = 0; i < cmd->entry_count; ++i) {
        payload_t *entry = &cmd->entries[i];
        LOG_TRACE("Entry (%d): Key [%s] - type [%c] - length [%d]", i, \
                 entry->key, entry->data_type, entry->data_length);
        switch (entry->data_type) {
        case DBUS_TYPE_STRING:
            LOG_TRACE("Entry (%d): Value (string): %s", i, entry->value.str);
            break;
        case DBUS_TYPE_INT32:
            LOG_TRACE("Entry (%d): Value (int): %d", i, entry->value.i32);
            break;
        default:
            LOG_TRACE("Entry (%d): Unsupported type: %c", i, entry->data_type);
            break;
        }
    }

    work = create_work(WORK_TYPE_REMOTE, cmd->prio, cmd->duration, \
                       cmd->opcode, (void *)cmd);
    if (!work) {
        LOG_ERROR("Failed to create work from cmd");
        delete_remote_cmd(cmd);
        return -ENOMEM;
    }

    push_work(get_wq(SYSTEM_WQ), work);
    return 0;
}

static int32_t handle_method_call(DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply = NULL;
    DBusMessageIter args;
    const char *reply_str = "Method reply OK";
    int32_t ret;

    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);

    if (!iface || !member)
        return -EINVAL;

    if (strcmp(iface, SER_IFACE) || strcmp(member, SER_METH))
        return 0; /* not our method */

    ret = dispatch_cmd_from_message(msg);
    if (ret < 0) {
        LOG_ERROR("Dispatch failed: iface=%s, meth=%s", iface, member);
        reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED,
                                       "Dispatch failed");
    } else {
        reply = dbus_message_new_method_return(msg);
        dbus_message_iter_init_append(reply, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &reply_str);
    }

    if (reply) {
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }

    return ret;
}

static int32_t handle_signal(DBusMessage *msg)
{
    int32_t ret;
    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);

    if (!iface || !member)
        return -EINVAL;

    if (strcmp(iface, LISTEN_IFACE) || strcmp(member, LISTEN_SIG))
        return 0; /* not our signal */

    ret = dispatch_cmd_from_message(msg);
    if (ret < 0)
        LOG_ERROR("Dispatch signal failed: %s.%s", iface, member);

    return ret;
}

static void handle_message(DBusConnection *conn, DBusMessage *msg)
{
    switch (dbus_message_get_type(msg)) {
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
        handle_method_call(conn, msg);
        break;
    case DBUS_MESSAGE_TYPE_SIGNAL:
        handle_signal(msg);
        break;
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        LOG_TRACE("Dbus method return detected");
        break;
    default:
        break;
    }
}

static int32_t dbus_connection_event_handler(DBusConnection *conn)
{
    DBusMessage *msg;

    if (!dbus_connection_read_write_dispatch(conn, 0))
        return 0;

    while ((msg = dbus_connection_pop_message(conn)) != NULL) {
        handle_message(conn, msg);
        dbus_message_unref(msg);
    }

    return 0;
}

static int32_t set_dbus_signal_match_rule(DBusConnection *conn)
{
    int32_t ret = 0;
    char *match_rule;

    match_rule = calloc(256, sizeof(char));
    if (!match_rule)
        return -ENOMEM;

    snprintf(match_rule, 256,
             "type='signal',interface='%s',member='%s',path='%s'",
             LISTEN_IFACE, LISTEN_SIG, LISTEN_OBJ_PATH);

    ret = add_dbus_match_rule(conn, match_rule);
    if (ret)
        LOG_ERROR("Failed to add DBus match rule: %d", ret);

    free(match_rule);
    return ret;
}

static DBusConnection *setup_dbus_connection()
{
    DBusConnection *conn = NULL;
    DBusError err;
    int32_t ret;

    dbus_error_init(&err);

    conn = dbus_bus_get(SER_BUS_TYPE, &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("DBus connection Error: %s", err.message);
        dbus_error_free(&err);
    }
    if (!conn)
        return NULL;

    ret = dbus_bus_request_name(conn, \
                                SER_NAME, \
                                DBUS_NAME_FLAG_REPLACE_EXISTING, \
                                &err);
    if (dbus_error_is_set(&err)) {
        LOG_FATAL("DBus request name error: %s", err.message);
        dbus_error_free(&err);
        dbus_connection_unref(conn);
        return NULL;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        dbus_connection_unref(conn);
        return NULL;
    }

    return conn;
}

static int32_t dbus_listener_loop(DBusConnection *conn)
{
    int32_t dbus_fd;
    int32_t epoll_fd;
    struct epoll_event ev;
    struct epoll_event events_detected[MAX_EVENTS];
    int32_t n_ready;
    int32_t ready_fd;

    if (!conn) {
        LOG_ERROR("Invalid DBus connection");
        return -EINVAL; /* Invalid argument */
    }

    // Get DBus file desc
    dbus_connection_get_unix_fd(conn, &dbus_fd);
    if (dbus_fd < 0) {
        LOG_ERROR("Failed to get dbus fd");
        return -EBADF; /* Bad file descriptor */
    }

    // Create epoll file desc
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        LOG_ERROR("Failed to create epoll fd: %s", strerror(errno));
        return -errno;
    }

    ev.events = EPOLLIN;

    // Add DBus file desc
    ev.data.fd = dbus_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dbus_fd, &ev) == -1) {
        LOG_ERROR("Failed to add DBus fd to epoll: %s", strerror(errno));
        close(epoll_fd);
        return -errno;
    }

    // Add Event file desc
    ev.data.fd = get_ctx()->comm.event;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, get_ctx()->comm.event, &ev) == -1) {
        LOG_ERROR("Failed to add event fd to epoll: %s", strerror(errno));
        close(epoll_fd);
        return -errno;
    }

    LOG_INFO("System manager DBus communication is running...");
    while (get_ctx()->run) {
        LOG_TRACE("[DBus]--> Waiting for next DBus message...");
        n_ready = epoll_wait(epoll_fd, events_detected, MAX_EVENTS, -1);
        if (n_ready == -1) {
            if (errno == EINTR) {
                LOG_WARN("epoll_wait interrupted, continuing...");
                continue;
            }
            LOG_ERROR("epoll_wait failed: %s", strerror(errno));
            close(epoll_fd);
            return -errno;
        }

        for (int32_t cnt = 0; cnt < n_ready; cnt++) {
            ready_fd = events_detected[cnt].data.fd;
            if (ready_fd == dbus_fd) {
                dbus_connection_event_handler(conn);
            } else if (ready_fd == get_ctx()->comm.event) {
                uint64_t event_id = 0;
                if (!event_get(get_ctx()->comm.event, &event_id)) {
                    LOG_INFO("Received event ID [%" PRIu64 "], stopping DBus listener...", \
                             event_id);
                }
            }
        }
    }

    close(epoll_fd);
    LOG_INFO("The DBus handler thread exited successfully");

    return 0;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
DBusConnection *get_dbus_connection()
{
    ctx_t *ctx = get_ctx();

    if (!ctx || !ctx->comm.dbus_conn) {
        return NULL;
    }

    return ctx->comm.dbus_conn;
}

int32_t set_dbus_connection(DBusConnection *conn)
{
    ctx_t *ctx = get_ctx();

    if (!conn) {
        return -1;
    }

    ctx->comm.dbus_conn = conn;
    return 0;
}

int32_t add_dbus_match_rule(DBusConnection *conn, const char *rule)
{
    DBusError err;
    if (NULL == conn) {
        return -EINVAL;
    }

    LOG_INFO("Dbus + match rule: [%s]", rule);
    dbus_error_init(&err);
    dbus_bus_add_match(conn, rule, &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("Dbus + match rule, error: %s", err.message);
        dbus_error_free(&err);
        return -EINVAL;
    }

    dbus_connection_flush(conn);
    return 0;
}

/*
 * DBus thread function, called after the DBus listener task is initialized.
 * It keeps the listener running for the entire lifetime of this service
 * to handle DBus communication, including method calls from other services
 * and signal events registered for this service.
 */
void *dbus_fn_thread_handler()
{
    DBusConnection *conn;
    int32_t ret;

    conn = setup_dbus_connection();
    if (!conn) {
        LOG_FATAL("Unable to establish connection with DBus");
        goto exit_error;
    }

    ret = set_dbus_signal_match_rule(conn);
    if (ret) {
        LOG_ERROR("DBus add signal match rule Error: %d", ret);
        goto exit_error;
    }

    ret = set_dbus_connection(conn);
    if (ret) {
        LOG_FATAL("Unable to save connection with DBus: %d", ret);
        goto exit_error;
    }

    // This loop processes DBus messages
    ret = dbus_listener_loop(conn);
    if (ret) {
        LOG_FATAL("Failed to create DBus listener");
        goto exit_error;
    }

    dbus_connection_unref(conn);

exit_error:
    return NULL;
}

/*
 * The DBus command will be sent by the task handler after the corresponding
 * work item is created and pushed into the workqueue. This work item will hold
 * an operation ID indicating that it is responsible for sending a DBus message.
 * The actual operation ID will be stored inside the associated data structure,
 * which will be encoded and decoded by the DBus communication framework during
 * message transmission and reception.
 */
static int32_t dbus_send_message_async(DBusMessage *msg, remote_cmd_t *cmd)
{
    DBusConnection *conn;

    if (!msg || !cmd)
        return -EINVAL;

    conn = get_dbus_connection();
    if (!conn) {
        LOG_ERROR("Failed to get dbus connection");
        return -EIO;
    }

    if (encode_data_frame(msg, cmd)) {
        LOG_ERROR("Failed to encode data frame");
        dbus_message_unref(msg);
        return -EIO;
    }

    /*
     * Sends a method call or signal without waiting for a reply, because the
     * listener thread will handle the reply message.
     */
    if (!dbus_connection_send(conn, msg, NULL)) {
        LOG_ERROR("Out of memory while sending message");
        dbus_message_unref(msg);
        return -ENOMEM;
    }

    dbus_connection_flush(conn);
    dbus_message_unref(msg);

    return 0;
}

/*
 * This function sends a D-Bus method call to the dbus client.
 * It operates without a specific callback for the reply message
 * because all responses are processed centrally in the D-Bus
 * listener thread rather than assigning per-call callbacks.
 */
int32_t dbus_method_call(const char *destination, const char *path, \
                         const char *iface, const char *method, \
                         remote_cmd_t *cmd)
{
    DBusMessage *msg;

    if (!destination || !path || !iface || !method || !cmd) {
        LOG_ERROR("Invalid argument");
        return -EINVAL;
    }

    msg = dbus_message_new_method_call(destination, path, iface, method);
    if (!msg) {
        LOG_ERROR("Failed to create method call message");
        return -ENOMEM;
    }

    return dbus_send_message_async(msg, cmd);
}

int32_t dbus_method_call_with_data(remote_cmd_t *cmd)
{
    int32_t ret;
    ret = dbus_method_call(REMOTE_SER_NAME, REMOTE_SER_OBJ_PATH,
                           REMOTE_SER_IFACE, REMOTE_SER_METH, cmd);
    if (ret) {
        LOG_ERROR("Failed to send method call message, ret %d", ret);
    }

    return ret;
}

/*
 * This function sends a D-Bus signal to the dbus.
 */
int32_t dbus_emit_signal(const char *path, const char *iface, \
                         const char *sig, remote_cmd_t *cmd)
{
    DBusMessage *msg;

    if (!path || !iface || !sig || !cmd) {
        LOG_ERROR("Invalid argument");
        return -EINVAL;
    }

    msg = dbus_message_new_signal(path, iface, sig);
    if (!msg) {
        LOG_ERROR("Failed to create signal message");
        return -ENOMEM;
    }

    return dbus_send_message_async(msg, cmd);
}

int32_t dbus_emit_signal_with_data(remote_cmd_t *cmd)
{
    int32_t ret;
    ret = dbus_emit_signal(SER_OBJ_PATH, SER_IFACE, SER_SIG, cmd);
    if (ret) {
        LOG_ERROR("Failed to emit signal message, ret %d", ret);
    }

    return ret;
}

