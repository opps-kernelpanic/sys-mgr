/**
 * @file f_comm.h
 *
 */

#ifndef G_F_COMM_H
#define G_F_COMM_H

/*********************
 *      INCLUDES
 *********************/
#include <stddef.h>
#include <stdint.h>

#include "main.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_PATH_LEN                    256
#define IIO_DEV_SYSFS_PATH              "/sys/bus/iio/devices"
#define IIO_DEV_NAME_FILE               "name"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
// internal comm
int32_t event_set(int32_t evfd, uint64_t code);
int32_t event_get(int32_t evfd, uint64_t *out_val);
int32_t init_event_file(ctx_t *ctx);
int32_t cleanup_event_file(ctx_t *ctx);

// fs_comm
int32_t fs_write_file(const char *path, const char *data, size_t len);
int32_t fs_append_file(const char *path, const char *data, size_t len);
int32_t fs_read_file(const char *path, char *buf, size_t buf_len, \
                     size_t *out_len);
int32_t fs_file_exists(const char *path);

int32_t find_device_path_by_name(const char *basepath, const char *fid, \
                                 const char *id_str, char *result_path, \
                                 size_t result_max_len);

int32_t iio_dev_get_path_by_name(const char *name, char *dev_path, \
                                 size_t path_len);
// proc_comm
int32_t exec_cmd_with_interact(char *cmd, char *input);


#endif /* G_F_COMM_H */
