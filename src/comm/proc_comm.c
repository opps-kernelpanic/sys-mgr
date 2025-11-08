/**
 * @file proc_comm.c
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
#include <stdbool.h>

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
int32_t cmd_executor(char *cmd, bool interact, char *input, char *output)
{
    FILE *pfd;
    int32_t ch;
    int32_t cnt = 0;

    if (!interact) {
        pfd = popen(cmd, "r");
        if (pfd == NULL) {
            LOG_ERROR("Unable to open process");
            return 1;
        }
        while((ch = fgetc(pfd)) != EOF)
            output[cnt++] = ch;
    } else {
        pfd = popen(cmd, "w");
        if (pfd == NULL) {
            LOG_ERROR("Unable to open process");
            return 1;
        }
        fprintf(pfd, "%s", input);
    }

    pclose(pfd);
    return cnt;
}

int32_t exec_cmd_with_interact(char *cmd, char *input)
{
    return cmd_executor(cmd, true, input, NULL);
}

int32_t exec_cmd_get_output(char *cmd, char *output)
{
    return cmd_executor(cmd, false, NULL, output);
}
