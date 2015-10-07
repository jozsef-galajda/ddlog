/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

#ifndef __DDLOG_EXT_H
#define __DDLOG_EXT_H
#include <stdio.h>
#include <stddef.h>
#include "ddlog.h"

typedef unsigned int ddlog_ext_event_type_t;
typedef void (*ddlog_ext_print_cb_t)(FILE* stream, void* data, size_t data_size);

typedef struct ddlog_ext_event_info_t {
    ddlog_ext_event_type_t event_type;
    ddlog_ext_print_cb_t print_callback;
} ddlog_ext_event_info_t;

/*
 * - the built-in predefined external events have fix event type value.
 * - the dynamiclaly defined external events will have an event type value
 *   assigned from DDLOG_EXT_EVENT_TYPE_DYNAMIC_START
 * - New built-in external events have to get event type
 *   value from 0 to DDLOG_EXT_EVENT_TYPE_DYNAMIC_START - 1
 */

#define DDLOG_EXT_EVENT_TYPE_NONE            0
#define DDLOG_EXT_EVENT_TYPE_BT              1
#define DDLOG_EXT_EVENT_TYPE_HEXDUMP         2
#define DDLOG_EXT_EVENT_TYPE_LAST DDLOG_EXT_EVENT_TYPE_HEXDUMP
#define DDLOG_EXT_EVENT_TYPE_DYNAMIC_START   100

ddlog_ext_print_cb_t ddlog_ext_get_print_cb(ddlog_ext_event_type_t ext_event_type);

int ddlog_ext_log(ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* message);

int ddlog_ext_log_id(ddlog_buffer_id_t buffer_id,
        ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* message);

int ddlog_ext_log_long(ddlog_ext_event_type_t event_id,
                      void* ext_data,
                      size_t data_size,
                      const char* thread,
                      const char* function_name,
                      int line_number,
                      const char* message);

int ddlog_ext_log_long_id(ddlog_buffer_id_t buffer_id,
        ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* thread_name,
        const char* function_name,
        int line_number,
        const char* message);


int ddlog_ext_event_type_is_valid(ddlog_ext_event_type_t event_type);

int ddlog_ext_register_built_in_events(void);
void ddlog_ext_display_hex_dump(FILE* stream, void* datap, size_t size);
void ddlog_ext_display_bt(FILE* stream, void* datap, size_t size);
int ddlog_ext_init(void);
ddlog_ext_event_type_t ddlog_ext_register_event(ddlog_ext_print_cb_t print_callback);

#define DDLOG_BT                                                \
    do {                                                        \
        void* array[256];                                       \
        size_t size = 0;                                        \
        memset(array, 0 ,sizeof(array));                        \
        size = backtrace(array, sizeof(array));                 \
        ddlog_ext_log_long(DDLOG_EXT_EVENT_TYPE_BT,             \
                          array, size * sizeof(void*),          \
                          NULL, __FUNCTION__,                   \
                          __LINE__, "Backtrace");               \
    } while (0);

#define DDLOG_HEX(data, data_size)                              \
    do {                                                        \
        ddlog_ext_log_long(DDLOG_EXT_EVENT_TYPE_HEXDUMP,        \
                          data, data_size, NULL,                \
                          __FUNCTION__, __LINE__,               \
                          "External log message");              \
    } while (0);


#endif
