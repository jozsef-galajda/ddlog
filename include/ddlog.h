/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

/**
 * \file ddlog.h
 * \brief Public data structures and funtion definitions.
 *
 * The ddlog library can be used to store internal log information
 * which could be helpful during software development. This should not be
 * used in production code.
 *
 * The messages can be displayed by connecting to the ddlog tcp server started
 * in the ddlog process by telnet.
 */
#ifndef __DDLOG_H
#define __DDLOG_H
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <execinfo.h>

typedef unsigned char ddlog_buffer_id_t;

#define DDLOG_RET_OK             0
#define DDLOG_RET_ERR            -1
#define DDLOG_RET_EVNT_LOCKED    -2
#define DDLOG_RET_ALREADY_INITED -3

#include "ddlog_ext.h"

int ddlog_init(size_t size);
void ddlog_thread_init(const char* thread_name);
int ddlog_reset(void);
int ddlog_reset_buffer_id(ddlog_buffer_id_t buffer_id);
void ddlog_cleanup(void);
ddlog_buffer_id_t ddlog_create_buffer(size_t size);
int ddlog_delete_buffer(ddlog_buffer_id_t buffer_id);
int ddlog_log(const char* message);
int ddlog_log_id(ddlog_buffer_id_t buffer_id, const char* message);
int ddlog_log_long(const char* thread, const char* function, unsigned int line_num, const char* message);
int ddlog_log_long_id(ddlog_buffer_id_t buffer_id, const char* thread, const char* function, unsigned int line_num, const char* message);
void ddlog_toggle_status(void);
int ddlog_get_status(void);
void ddlog_inc_indent(void);
void ddlog_dec_indent(void);
int ddlog_start_server(void);
void ddlog_wait_for_server(void);

#define DDLOG_VA(format_str, ...)                               \
    do {                                                        \
        char buffer[256];                                       \
        memset(buffer, 0, sizeof(buffer));                      \
        snprintf(buffer, sizeof(buffer) - 1,                    \
                 format_str, ## __VA_ARGS__);                   \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, buffer);   \
    } while (0);

#define DDLOG(message)                                          \
    do {                                                        \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, message);  \
    } while (0);

#define DDLOG_ENTRY                                             \
    do {                                                        \
        ddlog_inc_indent();                                     \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, "ENTRY");  \
    } while (0);

#define DDLOG_LEAVE                                             \
    do {                                                        \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, "LEAVE");  \
        ddlog_dec_indent();                                     \
    } while (0);

#define DDLOG_RET_FN(function, ret_type)                        \
    do {                                                        \
        ret_type temp_ret_value;                                \
        temp_ret_value = function;                              \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, "LEAVE");  \
        ddlog_dec_indent();                                     \
        return temp_ret_value;                                  \
    } while (0);

#define DDLOG_RET_EXP(expression)                               \
    do {                                                        \
        ddlog_log_long(NULL, __FUNCTION__, __LINE__, "LEAVE");  \
        ddlog_dec_indent();                                     \
        return (expression);                                    \
    } while (0);

#define DDLOG_INC_IND                                           \
    do {                                                        \
        ddlog_inc_indent();                                     \
    } while (0);

#define DDLOG_DEC_IND                                           \
    do {                                                        \
        ddlog_dec_indent();                                     \
    } while (0);


#endif
