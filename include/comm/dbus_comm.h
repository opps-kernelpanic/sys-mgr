/**
 * @file dbus_comm.h
 *
 */

#ifndef G_DBUS_COMM_H
#define G_DBUS_COMM_H
/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <dbus/dbus.h>

#include "comm/cmd_payload.h"

/*********************
 *      DEFINES
 *********************/
#define SYS_MGR_DBUS_SER                "com.SystemManager.Service"
#define SYS_MGR_DBUS_OBJ_PATH           "/com/SystemManager/Obj/SysCmd"
#define SYS_MGR_DBUS_IFACE              "com.SystemManager.Interface"
#define SYS_MGR_DBUS_METH               "SysMeth"
#define SYS_MGR_DBUS_SIG                "SysSig"

#define UI_DBUS_SER                     "com.TerminalUI.Service"
#define UI_DBUS_OBJ_PATH                "/com/TerminalUI/Obj/UsrCmd"
#define UI_DBUS_IFACE                   "com.TerminalUI.Interface"
#define UI_DBUS_METH                    "UIMeth"
#define UI_DBUS_SIG                     "UISig"


#define SER_BUS_TYPE                    DBUS_BUS_SYSTEM
#define SER_NAME                        SYS_MGR_DBUS_SER
#define SER_IFACE                       SYS_MGR_DBUS_IFACE
#define SER_METH                        SYS_MGR_DBUS_METH
#define SER_SIG                         SYS_MGR_DBUS_SIG
#define SER_OBJ_PATH                    SYS_MGR_DBUS_OBJ_PATH

#define LISTEN_IFACE                    UI_DBUS_IFACE
#define LISTEN_SIG                      UI_DBUS_SIG
#define LISTEN_OBJ_PATH                 UI_DBUS_OBJ_PATH

#define REMOTE_SER_NAME                 UI_DBUS_SER
#define REMOTE_SER_OBJ_PATH             UI_DBUS_OBJ_PATH
#define REMOTE_SER_IFACE                UI_DBUS_IFACE
#define REMOTE_SER_METH                 UI_DBUS_METH

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
 *  GLOBAL PROTOTYPES
 **********************/
int32_t add_dbus_match_rule(DBusConnection *conn, const char *rule);
void *dbus_fn_thread_handler();

int32_t dbus_method_call(const char *destination, const char *path, \
                         const char *iface, const char *method, \
                         remote_cmd_t *cmd);
int32_t dbus_method_call_with_data(remote_cmd_t *cmd);

int32_t dbus_emit_signal(const char *path, const char *iface, \
                         const char *sig, remote_cmd_t *cmd);
int32_t dbus_emit_signal_with_data(remote_cmd_t *cmd);
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

#endif /* G_DBUS_COMM_H */
