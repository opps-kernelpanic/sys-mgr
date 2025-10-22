/**
 * @file sys_utils.c
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

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "comm/dbus_comm.h"

// Encode remote_cmd_t into an existing DBusMessage
bool encode_data_frame(DBusMessage *msg, const remote_cmd_t *cmd)
{
    DBusMessageIter iter, array_iter, struct_iter, variant_iter;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &cmd->component_id);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->umid);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &cmd->opcode);

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
                return false;
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
    return true;
}

// Decode DBusMessage into remote_cmd_t
bool decode_data_frame(DBusMessage *msg, remote_cmd_t *out)
{
    DBusMessageIter iter, array_iter, struct_iter, variant_iter;

    if (!dbus_message_iter_init(msg, &iter)) {
        LOG_ERROR("Failed to init DBus iterator");
        return false;
    }

    dbus_message_iter_get_basic(&iter, &out->component_id);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->umid);
    dbus_message_iter_next(&iter);

    dbus_message_iter_get_basic(&iter, &out->opcode);
    dbus_message_iter_next(&iter);

    dbus_message_iter_recurse(&iter, &array_iter);

    int32_t i = 0;
    while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT && i < MAX_ENTRIES) {
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
    return true;
}

// Create a method cmd to send via method call
void create_method_frame(remote_cmd_t *cmd)
{
    cmd->component_id = "terminal-ui";
    cmd->umid = 1001;
    cmd->opcode = OP_ADJUST_BRIGHTNESS;
    cmd->entry_count = 2;

    cmd->entries[0].key = "backlight";
    cmd->entries[0].data_type = DBUS_TYPE_STRING;
    cmd->entries[0].data_length = 0;
    cmd->entries[0].value.str = "max";

    cmd->entries[1].key = "brightness";
    cmd->entries[1].data_type = DBUS_TYPE_INT32;
    cmd->entries[1].data_length = 1;
    cmd->entries[1].value.i32 = 100;
}

// Create a sample cmd to send as a signal
void create_signal_frame(remote_cmd_t *cmd)
{
    cmd->component_id = "terminal-ui";
    cmd->umid = 1001;
    cmd->opcode = OP_ADJUST_BRIGHTNESS;
    cmd->entry_count = 2;

    cmd->entries[0].key = "backlight";
    cmd->entries[0].data_type = DBUS_TYPE_STRING;
    cmd->entries[0].data_length = 0;
    cmd->entries[0].value.str = "min";

    cmd->entries[1].key = "brightness";
    cmd->entries[1].data_type = DBUS_TYPE_INT32;
    cmd->entries[1].data_length = 1;
    cmd->entries[1].value.i32 = 1;
}

// Send a DBus method call with encoded remote_cmd_t
int32_t send_method_call(DBusConnection *conn)
{
    DBusMessage *msg;
    DBusPendingCall *pending;
    DBusMessage *reply;
    DBusMessageIter reply_args;
    remote_cmd_t cmd;
    int32_t ret = EXIT_SUCCESS;

    msg = dbus_message_new_method_call(SYS_MGR_DBUS_SER,
                                       SYS_MGR_DBUS_OBJ_PATH,
                                       SYS_MGR_DBUS_IFACE,
                                       SYS_MGR_DBUS_METH);
    if (!msg) {
        LOG_FATAL("Failed to create method call message");
        return EXIT_FAILURE;
    }

    create_method_frame(&cmd);

    if (!encode_data_frame(msg, &cmd)) {
        LOG_ERROR("Failed to encode data frame");
        dbus_message_unref(msg);
        return EXIT_FAILURE;
    }

    if (!dbus_connection_send_with_reply(conn, msg, &pending, -1)) {
        LOG_ERROR("Out of memory while sending method call");
        dbus_message_unref(msg);
        return EXIT_FAILURE;
    }

    dbus_connection_flush(conn);
    dbus_message_unref(msg);

    dbus_pending_call_block(pending);
    reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!reply) {
        LOG_ERROR("No reply received for method call");
        return EXIT_FAILURE;
    }

    if (dbus_message_iter_init(reply, &reply_args)) {
        char *reply_str = NULL;
        dbus_message_iter_get_basic(&reply_args, &reply_str);
        if (reply_str) {
            LOG_INFO("Method call reply: %s", reply_str);
        } else {
            LOG_WARN("Method call reply received but empty");
        }
    } else {
        LOG_WARN("Reply does not contain arguments");
    }

    dbus_message_unref(reply);
    return ret;
}

// Send a DBus signal with encoded remote_cmd_t
int32_t send_signal(DBusConnection *conn)
{
    DBusMessage *msg;
    remote_cmd_t cmd;

    msg = dbus_message_new_signal(UI_DBUS_OBJ_PATH,
                                  UI_DBUS_IFACE,
                                  UI_DBUS_SIG);
    if (!msg) {
        LOG_FATAL("Failed to create signal message");
        return EXIT_FAILURE;
    }

    create_signal_frame(&cmd);

    if (!encode_data_frame(msg, &cmd)) {
        LOG_ERROR("Failed to encode data frame");
        dbus_message_unref(msg);
        return EXIT_FAILURE;
    }

    if (!dbus_connection_send(conn, msg, NULL)) {
        LOG_ERROR("Out of memory while sending signal");
        dbus_message_unref(msg);
        return EXIT_FAILURE;
    }

    dbus_connection_flush(conn);
    dbus_message_unref(msg);

    LOG_INFO("Signal sent successfully");
    return EXIT_SUCCESS;
}

// Main entry point32_t of the CLI test application
int32_t main(int32_t argc, char **argv)
{
    DBusError err;
    DBusConnection *conn;
    int32_t ret;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("Connection error: %s", err.message);
        dbus_error_free(&err);
    }

    if (!conn) {
        LOG_FATAL("Failed to connect to DBus system bus");
        return EXIT_FAILURE;
    }

    ret = dbus_bus_request_name(conn,
                                UI_DBUS_SER,
                                DBUS_NAME_FLAG_REPLACE_EXISTING,
                                &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("DBus name request error: %s", err.message);
        dbus_error_free(&err);
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        LOG_FATAL("Not primary owner of the requested DBus name");
        return EXIT_FAILURE;
    }

    if (argc > 1 && strcmp(argv[1], "signal") == 0) {
        return send_signal(conn);
    } else {
        return send_method_call(conn);
    }

    return EXIT_SUCCESS;
}
