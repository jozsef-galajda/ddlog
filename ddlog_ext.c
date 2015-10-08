/*
 * Copyright (c) 2015 Jozsef Galajda <jozsef.galajda@gmail.com>
 * All rights reserved.
 */

/**
 * \file ddlog_ext.c
 * \brief ddlog library extended event handling
 *
 * This file contains the implementation of the apis
 * related to extended event handling.
 *
 * Extended events are events/logs containing some sort of binary data
 * and a callback function capable of printing out the event data
 * in a readable format. The event itself is stored as a binary data and
 * only formatted when a print of the event is needed.
 *
 * There are built-in extended events and user-defined extended events.
 * Bult-in:
 *  backtrace: the event contains a stack trace
 *  hexdump: the event contains a binary data printed out as hexdump
 * User defined:
 *  The user has to register an extended event by providing a print callback function.
 *  Logging an extended event means to copy the binary data to the log message.
 *  When the event has to be printed, the registered print callback function is
 *  called with the binary data provided at the time of the log call. For example
 *  one can save a structure in the log and provide a callback function capable of
 *  printing out the structure in readable format.
 *  First the user has to register the extended event by calling the
 *  ddlog_ext_register_event() API. This API returns an event id, which has to be used
 *  from the code when the event is logged.
 *
 *  The built-in events are automatically registered.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <pthread.h>
#include "ddlog.h"
#include "private/ddlog_internal.h"
#include "ddlog_ext.h"

#define DDLOG_EXT_EVENT_MAX 256

struct {
    ddlog_ext_event_info_t events[DDLOG_EXT_EVENT_MAX];
    unsigned int index;
    ddlog_ext_event_type_t next_event_type;
    unsigned char initialized;
    pthread_spinlock_t lock;
} ddlog_ext_events;

int ddlog_ext_init(void){
    int spin_res = 0;
    int ret = DDLOG_RET_ERR;
    memset(ddlog_ext_events.events, 0, sizeof(ddlog_ext_events.events));
    ddlog_ext_events.index = 0;
    ddlog_ext_events.initialized = 0;
    ddlog_ext_events.next_event_type = DDLOG_EXT_EVENT_TYPE_DYNAMIC_START;
    spin_res = pthread_spin_init(&ddlog_ext_events.lock, PTHREAD_PROCESS_PRIVATE);
    if (spin_res == 0){
        ret = ddlog_ext_register_built_in_events();
        if (ret == DDLOG_RET_OK) {
            ddlog_ext_events.initialized = 1;
        }
    }
    return ret;
}

int ddlog_ext_register_built_in_events(void){
    int spin_res = 0;
    int ret = DDLOG_RET_ERR;

    spin_res = pthread_spin_lock(&ddlog_ext_events.lock);
    if (spin_res == 0){
        ddlog_ext_events.events[ddlog_ext_events.index].event_type = DDLOG_EXT_EVENT_TYPE_BT;
        ddlog_ext_events.events[ddlog_ext_events.index].print_callback = ddlog_ext_display_bt;
        ddlog_ext_events.index++;

        ddlog_ext_events.events[ddlog_ext_events.index].event_type = DDLOG_EXT_EVENT_TYPE_HEXDUMP;
        ddlog_ext_events.events[ddlog_ext_events.index].print_callback = ddlog_ext_display_hex_dump;
        ddlog_ext_events.index++;

        spin_res = pthread_spin_unlock(&ddlog_ext_events.lock);
        if (spin_res == 0){
            ret = DDLOG_RET_OK;
        }
    }
    return ret;
}

ddlog_ext_event_type_t ddlog_ext_register_event(ddlog_ext_print_cb_t print_callback){
    int i = 0;
    int ret = DDLOG_EXT_EVENT_TYPE_NONE;
    int spin_res = 0;
    int dup_found = 0;

    if (ddlog_ext_events.initialized){
        spin_res = pthread_spin_lock(&ddlog_ext_events.lock);
        if (spin_res == 0){
            /*
             * Check for duplicates. If the same callback is already registered,
             * return with the registered event id.
             * TODO:
             * Consider allowing duplicates based on a bool parameter in case the same
             * function is used to print out more event types.
             */
            for (i = 0; i < ddlog_ext_events.index; i++){
                if (ddlog_ext_events.events[i].print_callback == print_callback){
                    dup_found = 1;
                    break;
                }
            }
            /*
             * Either return with the already registered event id
             * or try to allocate the new event.
             */
            if (dup_found == 1){
                ret = ddlog_ext_events.events[i].event_type;
            } else {
                if (ddlog_ext_events.index < DDLOG_EXT_EVENT_MAX) {
                    ddlog_ext_events.events[ddlog_ext_events.index].event_type = ddlog_ext_events.next_event_type;
                    ddlog_ext_events.events[ddlog_ext_events.index].print_callback = print_callback;
                    ddlog_ext_events.index++;
                    ret = ddlog_ext_events.next_event_type++;
                }
            }

            spin_res = pthread_spin_unlock(&ddlog_ext_events.lock);
            if (spin_res != 0){
                ddlog_ext_events.initialized = 0;
                ret = DDLOG_EXT_EVENT_TYPE_NONE;
            }
        }
    }
    return ret;
}

/**
 * \brief
 * \param
 * \return
 *
 */
int ddlog_ext_log(ddlog_ext_event_type_t event_type, void* ext_data, size_t data_size, const char* message){
    return ddlog_ext_log_long_id(ddlog_internal_get_default_buf_id(),
            event_type, ext_data, data_size, NULL, NULL, 0, message);
}

int ddlog_ext_log_id(ddlog_buffer_id_t buffer_id,
        ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* message)
{
    return ddlog_ext_log_long_id(buffer_id, event_type, ext_data, data_size, NULL, NULL, 0, message);
}

int ddlog_ext_log_long(ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* thread,
        const char* function_name,
        int line_number,
        const char* message)
{
    return ddlog_ext_log_long_id(ddlog_internal_get_default_buf_id(),
            event_type, ext_data, data_size, thread, function_name, line_number, message);
}

int ddlog_ext_log_long_id(ddlog_buffer_id_t buffer_id,
        ddlog_ext_event_type_t event_type,
        void* ext_data,
        size_t data_size,
        const char* thread_name,
        const char* function_name,
        int line_number,
        const char* message)
{
    const char *thread_name_p = NULL;
    int res = DDLOG_RET_ERR;
    ddlog_buffer_t* buffer = ddlog_internal_get_buffer_by_id(buffer_id);

    if (ddlog_internal_is_lib_inited()
            && ddlog_internal_is_logging_enabled()
            && ddlog_ext_events.initialized == 1
            && ddlog_ext_event_type_is_valid(event_type)
            && buffer != NULL)
    {
        if (thread_name == NULL) {
            if (ddlog_internal_get_thread_name()[0] != '0'){
                thread_name_p = ddlog_internal_get_thread_name();
            }
        } else {
            thread_name_p = thread_name;
        }

        res = ddlog_log_internal(buffer, thread_name_p, function_name, line_number, message, ext_data, data_size, event_type);
    }
    return res;
}

ddlog_ext_print_cb_t ddlog_ext_get_print_cb(ddlog_ext_event_type_t ext_event_type){
    ddlog_ext_print_cb_t ret = NULL;
    int spin_res = 0;
    int i = 0;

    spin_res = pthread_spin_lock(&ddlog_ext_events.lock);
    if (spin_res == 0){

        for (i = 0; i < ddlog_ext_events.index; i++){
            if (ddlog_ext_events.events[i].event_type == ext_event_type){
                ret = ddlog_ext_events.events[i].print_callback;
                break;
            }
        }

        spin_res = pthread_spin_unlock(&ddlog_ext_events.lock);
        if (spin_res != 0){
            ret = NULL;
            ddlog_ext_events.initialized = 0;
        }
    } else {
        ddlog_ext_events.initialized = 0;
    }
    return ret;
}

int ddlog_ext_event_type_is_valid(ddlog_ext_event_type_t event_type){
    return ((event_type > DDLOG_EXT_EVENT_TYPE_NONE && event_type <= DDLOG_EXT_EVENT_TYPE_LAST) ||
            (event_type >= DDLOG_EXT_EVENT_TYPE_DYNAMIC_START && event_type < ddlog_ext_events.next_event_type));
}
