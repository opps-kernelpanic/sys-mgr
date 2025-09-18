/**
 * @file fs_comm.c
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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "comm/f_comm.h"

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

static int32_t __sf_fs_write_internal(const char *path,  const char *data, \
                                  size_t len, int32_t append)
{
    int32_t fd, ret, flags;

    if (!path || !data || len == 0) {
        LOG_ERROR("invalid argument");
        return -EINVAL;
    }

    flags = O_WRONLY | O_CREAT;
    flags |= append ? O_APPEND : O_TRUNC;

    LOG_TRACE("%s to %s, len=%zu", \
         append ? "append" : "write", path, len);
    LOG_TRACE("write data:\n%.*s", (int)len, data);

    fd = open(path, flags, 0644);
    if (fd < 0) {
        LOG_ERROR("open failed: %s", strerror(errno));
        return -EIO;
    }

    ret = write(fd, data, len);
    if (ret < 0) {
        LOG_ERROR("write failed: %s", strerror(errno));
        close(fd);
        return ret;
    }

    close(fd);
    LOG_TRACE("%s success", append ? "append" : "write");
    return 0;
}

int32_t gf_fs_write_file(const char *path, const char *data, size_t len)
{
    return __sf_fs_write_internal(path, data, len, 0);
}

int32_t gf_fs_append_file(const char *path, const char *data, size_t len)
{
    return __sf_fs_write_internal(path, data, len, 1);
}

int gf_fs_read_file(const char *path, char *buf, size_t buf_len, \
                    size_t *out_len)
{
    int fd, ret;
    ssize_t size;

    if (!path || !buf || buf_len == 0 || !out_len)
        return -EINVAL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -errno;

    size = read(fd, buf, buf_len - 1);
    if (size < 0) {
        ret = -errno;
        close(fd);
        return ret;
    }

    buf[size] = '\0';
    *out_len = size;

    close(fd);
    return 0;
}

int32_t gf_fs_file_exists(const char *path)
{
    struct stat st;

    if (!path) {
        LOG_ERROR("invalid argument");
        return -EINVAL;
    }

    if (stat(path, &st) == 0) {
        LOG_DEBUG("file exists: %s", path);
        return 1;
    }

    LOG_DEBUG("file not found: %s", path);
    return -ENOENT;
}

int32_t find_device_path_by_name(const char *basepath, const char *fid, \
                                 const char *id_str, char *result_path, \
                                 size_t result_max_len)
{
    DIR *dir;
    struct dirent *entry;
    char name_path[256];
    char name_buf[128];
    size_t read_len;
    int ret = -ENOENT;

    if (!id_str || !result_path || result_max_len == 0)
        return -EINVAL;

    dir = opendir(basepath);
    if (!dir)
        return -errno;

    while ((entry = readdir(dir)) != NULL) {
        /*
         * Only support IIO dev
         * TODO: container for each device type
         */
        if (strncmp(entry->d_name, "iio:device", 10) != 0)
            continue;

        snprintf(name_path, sizeof(name_path), "%s/%s/%s", \
                 basepath, entry->d_name, fid);

        ret = gf_fs_read_file(name_path, name_buf, sizeof(name_buf), \
                              &read_len);
        if (ret < 0)
            continue;

        name_buf[strcspn(name_buf, "\n")] = '\0';

        if (strcmp(name_buf, id_str) == 0) {
            snprintf(result_path, result_max_len, "%s/%s", \
                     basepath, entry->d_name);
            ret = 0;
            break;
        }
    }

    closedir(dir);
    return ret;
}

int32_t iio_dev_get_path_by_name(const char *name, char *dev_path, \
                                 size_t path_len)
{
    int32_t ret;

    ret = find_device_path_by_name(IIO_DEV_SYSFS_PATH, IIO_DEV_NAME_FILE, \
                                   name, dev_path, path_len);
    if (ret == 0) {
        LOG_INFO("Found IIO device: %s (target=%s)\n", dev_path, name);
    } else if (ret == -ENOENT) {
        LOG_ERROR("Device '%s' not found\n", name);
        return ret;
    } else {
        LOG_ERROR("Error: %s\n", strerror(-ret));
        return ret;
    }

    return ret;
}
