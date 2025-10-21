/**
 * @file cmd_payload.h
 *
 */

#ifndef G_CMD_PAYLOAD_H
#define G_CMD_PAYLOAD_H
/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/
#define COMP_NAME                       "SYSTEM-MANAGER"
#define COMP_ID                         02
#define MAX_ENTRIES                     32

/**********************
 *      TYPEDEFS
 **********************/
/* Go001 DBus message frame
    STRUCT {
        STRING   component_id;
        INT32    umid;
        INT32    opcode;

        ARRAY of STRUCT {
            STRING key;
            INT32  data_type;
            INT32  data_length;
            VARIANT data;
        }
    }
 */

typedef enum {
    OP_NONE = 0,
    /* Common API */
    OP_PING,
    /* DBUS API*/
    OP_DBUS_SENT_CMD,

    /* HARDWARE */
    OP_START_HW_MON,
    /* Brightness API */
    OP_ALS_ON,
    OP_ALS_OFF,
    OP_BACKLIGHT_ON,
    OP_BACKLIGHT_OFF,
    OP_GET_BRIGHTNESS,
    OP_SET_BRIGHTNESS,
    /* Vibrator API */
    OP_LEFT_VIBRATOR,
    OP_RIGHT_VIBRATOR,
    /* IMU API */
    OP_ENABLE_IMU,
    OP_DISABLE_IMU,
    OP_READ_IMU,
    OP_READ_ANGLE,
    /* AUDIO API */
    OP_AUDIO_INIT,
    OP_AUDIO_RELEASE,
    OP_SOUND_PLAY,

    /* Network API */
    OP_WIFI_RESCAN,
    OP_WIFI_GET_AP_LIST,
    OP_WIFI_GET_AP_INFO,
    OP_WIFI_CONN_AP,

    OP_ID_END,
} opcode_t;


// Union for holding the actual variant value
typedef union {
    const char *str;     // String value
    int32_t i32;         // Signed 32-bit integer
    uint32_t u32;        // Unsigned 32-bit integer
    double dbl;          // Double-precision float
} variant_val_t;

// One entry in the payload array
typedef struct {
    const char *key;         // Entry key name
    uint32_t data_type;       // DBus data type code (e.g., DBUS_TYPE_STRING)
    uint32_t data_length;     // Data length (used if type is array/string)
    variant_val_t value;      // Actual value
} payload_t;

// Top-level data frame structure
typedef struct {
    const char *component_id;    // Identifier of the sender
    uint32_t umid;                // Message ID
    uint32_t opcode;              // Operation code
    uint8_t prio;
    uint8_t duration;
    uint32_t entry_count;         // Number of entries in the payload
    payload_t entries[MAX_ENTRIES]; // Payload entries
} remote_cmd_t;




typedef struct {
    uint32_t opcode;
    void *payload;
} local_cmd_t;

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
remote_cmd_t *create_remote_cmd();
void delete_remote_cmd(remote_cmd_t *cmd);

local_cmd_t *create_local_cmd();
void delete_local_cmd(local_cmd_t *cmd);

void remote_cmd_init(remote_cmd_t *cmd, const char *component_id, int32_t umid, \
                     int32_t opcode, uint8_t priority, uint8_t duration);
int32_t remote_cmd_add_string(remote_cmd_t *cmd, const char *key, \
              const char *value);
int32_t remote_cmd_add_int(remote_cmd_t *cmd, const char *key, int32_t value);

/* Task helpper */
int32_t create_local_simple_task(uint8_t priority, uint8_t duration, uint32_t opcode);
int32_t create_remote_task(uint8_t priority, void *data);
remote_cmd_t *create_remote_task_data(uint8_t priority, uint8_t duration, \
                                      uint32_t opcode);
int32_t create_remote_simple_task(uint8_t priority, uint8_t duration, uint32_t opcode);
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

#endif /* G_CMD_PAYLOAD_H*/
