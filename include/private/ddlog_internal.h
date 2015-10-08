/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

/**
 * \file ddlog_internal.h
 * \brief Internal data structures and funtion definitions.
 */
#ifndef __DDLOG_INTERNAL_H
#define __DDLOG_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <sys/time.h>

#include "ddlog.h"
#include "ddlog_ext.h"
#define UNUSED __attribute__ ((unused))

#define DDLOG_MAX_EVENT_NUM  128
#define DDLOG_MAX_BUF_NUM    5
#define DDLOG_FNAME_BUF_SIZE 32
#define DDLOG_TNAME_BUF_SIZE 32
#define DDLOG_MSG_BUF_SIZE   256

/**
 * \struct ddlog_event_t
 * \brief Structure to hold all log event specific data.
 */
typedef struct ddlog_event_t {
    char function_name[DDLOG_FNAME_BUF_SIZE]; /*!< Name of the function the log comes from. Optional. */
    char thread_name[DDLOG_TNAME_BUF_SIZE];   /*!< Thread name from the log comes from. Optional. */
    char message[DDLOG_MSG_BUF_SIZE];         /*!< The log message itself */
    unsigned int line_number ;               /*!< The line number of the log message in the code */
    struct timeval timestamp;                /*!< Timestamp of the log message */
    struct ddlog_event_t* next;               /*!< The next log event. Events are stored in linked list */
    unsigned char used;                      /*!< Event slot is free/used */
    unsigned char lock;                      /*!< The event structure is locked (getting populated with data */
    void* ext_data;                          /*!< Extended log data if log is special */
    size_t ext_data_size;                    /*!< The size of the extended log data */
    ddlog_ext_event_type_t ext_event_type;    /*!< The external event type if any */
    ddlog_ext_print_cb_t ext_print_cb;        /*!< Function to print/format external data to stream */
    uint8_t indent_level;                    /*!< Log message ident level */
} ddlog_event_t;


/**
 * \struct ddlog_buffer_t
 * \brief Structure to hold all log buffer related information
 */
typedef struct ddlog_buffer_t {
    ddlog_event_t* head;         /*!< Head of the event data list*/
    ddlog_event_t* next_write;   /*!< The next write position in the list*/
    size_t buffer_size;         /*!< The number of events (log buffer capacity)*/
    int wrapped;                /*!< Number of buffer wraps*/
    pthread_spinlock_t lock;    /*!< Buffer lock for pointer operations */
    unsigned int event_locked;  /*!< Counter of msg drops because of event is locked */
} ddlog_buffer_t;

typedef enum {
    DDLOG_LOCK_UNINITED = 0,
    DDLOG_LOCK_UNLOCKED = 1,
    DDLOG_LOCK_SIMPLE = 2,
    DDLOG_LOCK_FULL = 3
} ddlog_lock_state_t;


ddlog_buffer_t* ddlog_init_buffer_internal(size_t size);
int ddlog_reset_buffer_internal(ddlog_buffer_t* log_buffer);
void ddlog_cleanup_buffer_internal(ddlog_buffer_t* buffer);
void ddlog_reset_event_internal(ddlog_event_t* event);

int ddlog_log_internal(ddlog_buffer_t* log_buffer, const char* thread,
        const char* function, unsigned int line_num,
        const char* message, void* ext_data, size_t ext_data_size,
        ddlog_ext_event_type_t event_type);

int ddlog_lock_buffer_internal(ddlog_buffer_t* buffer);
int ddlog_unlock_buffer_internal(ddlog_buffer_t* buffer);
int ddlog_lock_global(int full_lock);
int ddlog_unlock_global(void);

void ddlog_enable_internal(void);
void ddlog_disable_internal(void);

int ddlog_internal_get_max_buf_num(void);
int ddlog_internal_is_lib_inited(void);
int ddlog_internal_is_logging_enabled(void);
ddlog_buffer_t* ddlog_internal_get_default_buf(void);
ddlog_buffer_id_t ddlog_internal_get_default_buf_id(void);
ddlog_buffer_t* ddlog_internal_get_buffer_by_id(ddlog_buffer_id_t buffer_id);
char* ddlog_internal_get_thread_name(void);

#endif
