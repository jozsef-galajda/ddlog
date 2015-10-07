/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

/**
 * \file ddlog.c
 * \brief ddlog library implementation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdint.h>

#include "ddlog.h"
#include "ddlog_internal.h"
#include "ddlog_debug.h"
#include "ddlog_display.h"
#include "ddlog_ext.h"

int ddlog_lib_inited = 0;
int ddlog_enabled = 0;

ddlog_buffer_t* ddlog_buffers[DDLOG_MAX_BUF_NUM];
ddlog_buffer_t* ddlog_default_buf = 0;
int ddlog_default_buf_id = -1;

pthread_spinlock_t ddlog_global_lock;
static ddlog_lock_state_t ddlog_global_lock_state = DDLOG_LOCK_UNINITED;

__thread char ddlog_thread_name[16] = {0};
__thread uint8_t ddlog_thread_indent_level = 0;

/******************************************************************************
 *
 * P U B L I C  functions
 *
 ******************************************************************************/


/**
 * \brief Initializes the ddlog library. Allocates the default log buffer.
 *
 * \param size The maximum number of log messages in the log buffer. 
 *             If 0, no default buffer is allocated
 * \return 0 on success, -1 in case of error
 *
 * Initializes the ddlog logging library.
 * If the size parameter is 0, we use the default buffer size.
 */
int ddlog_init(size_t size){
    int spin_res = 0;
    int ext_init_res = DDLOG_RET_ERR;

    if (ddlog_lib_inited == 1) {
        return DDLOG_RET_ALREADY_INITED;
    }

    if (size > DDLOG_MAX_EVENT_NUM) {
        size = DDLOG_MAX_EVENT_NUM;
    }

    /* init the global lock */
    spin_res = pthread_spin_init(&ddlog_global_lock, PTHREAD_PROCESS_PRIVATE);
    if (spin_res) {
        return DDLOG_RET_ERR;
    }
    ddlog_global_lock_state = DDLOG_LOCK_UNLOCKED;

    /* set up the buffer pointer array and allocate the default
     * log buffer
     */
    memset(ddlog_buffers, 0, sizeof(ddlog_buffers));

    if (size != 0) {
        ddlog_buffers[0] = ddlog_init_buffer_internal(size);
        if (ddlog_buffers[0]) {
            ddlog_default_buf = ddlog_buffers[0];
            ddlog_default_buf_id = 0;
        }
    }

    ext_init_res = ddlog_ext_init();
    if (ext_init_res == DDLOG_RET_OK){
        ddlog_enable_internal();
        ddlog_lib_inited = 1;
        return DDLOG_RET_OK;
    }

    return DDLOG_RET_ERR;
}

/**
 * \brief Sets the thread name TLS variable
 *
 * \param thread_name The name of the thread
 *
 * If this variable is set, the events logged by the thread
 * will contain the thread name
 */
void ddlog_thread_init(const char* thread_name){
    if (thread_name){
        snprintf(ddlog_thread_name, sizeof(ddlog_thread_name) - 1, "%s", thread_name);
    }
    ddlog_thread_indent_level = 0;
}

/**
 * \brief Resets ALL ddlog log buffer - clean sheet
 *
 * \return DDLOG_RET_OK on success. DDLOG_RET_ERR in case of any error
 *
 * Resets all log buffers, cleans the messages. After calling this the
 * buffers will contain no messages.
 */
int ddlog_reset(void){
    int res = DDLOG_RET_ERR;
    int lock_res = 0;
    int i = 0;

    if (ddlog_lib_inited) {
        lock_res = ddlog_lock_global(0);
        if (lock_res != DDLOG_RET_OK){
            return lock_res;
        }
        for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
            if (ddlog_buffers[i]){
                res = ddlog_reset_buffer_internal(ddlog_buffers[i]);
                if (res != DDLOG_RET_OK){
                    break;
                }
            }
        }
    }
    lock_res = ddlog_unlock_global();
    return (res == DDLOG_RET_OK && lock_res == DDLOG_RET_OK) ? DDLOG_RET_OK : DDLOG_RET_ERR;
}

/**
 * \brief Resets the specified ddlog log buffer
 *
 * \param buffer_id the id of the log buffer to be reset
 * \return DDLOG_RET_OK on success. DDLOG_RET_ERR in case of any error
 *
 * Resets the selected log buffer, cleans the messages. After calling this the
 * buffer will contain no messages.
 */
int ddlog_reset_buffer_id(ddlog_buffer_id_t buffer_id){
    int res = DDLOG_RET_ERR;
    if (ddlog_lib_inited){
        if (buffer_id < DDLOG_MAX_BUF_NUM && ddlog_buffers[buffer_id]){
            res = ddlog_reset_buffer_internal(ddlog_buffers[buffer_id]);
        }
    }
    return res;
}


/**
 * \brief Library cleanup function
 *
 * Generic cleanup routine. Frees all allocated memory (events and the buffers).
 * After calling this function no new logs are accepted.
 */
void ddlog_cleanup(void){
    int i = 0, lock_res = 0;
    if (ddlog_lib_inited){
        lock_res = ddlog_lock_global(0);
        if (lock_res != DDLOG_RET_OK){
            return;
        }
        ddlog_disable_internal();

        /* for all buffers call the internal cleanup routine */
        for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
            if (ddlog_buffers[i]){
                ddlog_cleanup_buffer_internal(ddlog_buffers[i]);
            }
        }

        /* delete all buffer pointers, invalidate the
         * default buffer pointer, release and destroy the global lock */
        ddlog_lib_inited = 0;
        ddlog_default_buf = NULL;
        ddlog_default_buf_id = -1;
        memset(ddlog_buffers, 0, sizeof(ddlog_buffers));

        lock_res = pthread_spin_unlock(&ddlog_global_lock);
        if (lock_res){
            return;
        }
        lock_res = pthread_spin_destroy(&ddlog_global_lock);
        ddlog_global_lock_state = DDLOG_LOCK_UNINITED;
    }
}

/**
 * \brief Create a new ddlog log buffer
 *
 * \param size The maximum number of log messages in the log buffer.
 * \return the index of the new buffer or -1 in case of any error
 *
 * Searches for a free buffer and initializes it.
 * Returns with the index of the new buffer. This ID has to be used
 * as a parameter of the ddlog_*_id functions to select the buffer to
 * place the log message into.
 */
ddlog_buffer_id_t ddlog_create_buffer(size_t size){
    int buffer_index = -1;
    int i = 0;
    int lock_res = DDLOG_RET_ERR;

    if (ddlog_lib_inited){
        lock_res = ddlog_lock_global(0);
        if (lock_res != DDLOG_RET_OK){
            return -1;
        }

        /* search for the first free buffer id */
        for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
            if (ddlog_buffers[i] == NULL){
                buffer_index = i;
                break;
            }
        }

        /* there is a free buffer, initialize it */
        if (buffer_index >= 0) {    
            if (size == 0 || size > DDLOG_MAX_EVENT_NUM) {
                size = DDLOG_MAX_EVENT_NUM;
            }
            ddlog_buffers[buffer_index] = ddlog_init_buffer_internal(size);
            if (ddlog_default_buf == NULL){
                ddlog_default_buf = ddlog_buffers[buffer_index];
                ddlog_default_buf_id = buffer_index;
            }
        }

        lock_res = ddlog_unlock_global();
        if ( buffer_index >= 0 && ddlog_buffers[buffer_index] != NULL && lock_res == DDLOG_RET_OK){
            return buffer_index;
        }
    }
    return -1;
}

/**
 * \brief Logs a new event to the default log buffer
 *
 * \param message The log message string
 * \return 0 on success, -1 in case of error
 *
 * Creates a new log message in the buffer. This is the short version where
 * only the message is used. All the other parameters will be empty.
 */
int ddlog_log(const char* message){
    int res = DDLOG_RET_ERR;
    char* thread_name = NULL;
    if (ddlog_lib_inited && ddlog_default_buf && ddlog_enabled && message) {
        if (ddlog_thread_name[0] != '0'){
            thread_name = ddlog_thread_name;
        }
        res = ddlog_log_internal(ddlog_default_buf, thread_name, NULL, 0, message, NULL, 0, DDLOG_EXT_EVENT_TYPE_NONE);
    }
    return res;
}

/**
 * \brief Logs a new event to the log buffer with the specified id.
 *
 * \param buffer_id The id of the buffer into the message will be placed
 * \param message The log message string
 * \return 0 on success, -1 in case of error
 *
 * Creates a new log message in the buffer with buffer_id. 
 * The buffer has to be created with ddlog_create_buffer().
 * This is the short version where only the message is used. 
 * All the other parameters will be empty.
 */
int ddlog_log_id(ddlog_buffer_id_t buffer_id, const char* message){
    int res = DDLOG_RET_ERR;
    if (ddlog_lib_inited && ddlog_enabled && message){
        if (buffer_id < DDLOG_MAX_BUF_NUM && ddlog_buffers[buffer_id]){
            res = ddlog_log_internal(ddlog_buffers[buffer_id], NULL, NULL, 0, message, NULL, 0, DDLOG_EXT_EVENT_TYPE_NONE);
        }
    }
    return res;
}

/**
 * \brief Logs a new event with all the possible parameters to the default log buffer
 *
 * \param thread The name of the thread generating the log message. (optional)
 * \param function The name of the function generating the log message (optional)
 * \param line_num The line number in the source file of the log message (optional)
 * \param message The log message string
 * \return 0 if success, -1 in case of any error
 *
 * Creates a new log event in the buffer. This is the longest version where all the
 * possible parameters can be provided. The message parameter has to be valid
 * (not NULL), any of the rest can be NULL or 0 in case it is not needed in the
 * message.
 */
int ddlog_log_long(const char* thread, 
        const char* function, 
        unsigned int line_num, 
        const char* message)
{
    int res = DDLOG_RET_ERR;
    char * thread_name = NULL;
    if (ddlog_lib_inited && ddlog_default_buf && ddlog_enabled && message) {
        if (thread == NULL && ddlog_thread_name[0] != '\0'){
            thread_name = ddlog_thread_name;
        }
        res = ddlog_log_internal(ddlog_default_buf, thread_name, function, line_num, message, NULL, 0, DDLOG_EXT_EVENT_TYPE_NONE);
    }
    return res;
}

/**
 * \brief Logs a new event with all the possible parameters to a buffer with a specified id
 *
 * \param buffer_id the id of the buffer into the message will be put
 * \param thread The name of the thread generating the log message. (optional)
 * \param function The name of the function generating the log message (optional)
 * \param line_num The line number in the source file of the log message (optional)
 * \param message The log message string
 * \return 0 if success, -1 in case of any error
 *
 * Creates a new log event in the buffer with id of buffer_id. 
 * The buffer has to be created first  with ddlog_create_buffer(). 
 * This is the longest version where all the possible parameters can be 
 * provided. The message and buffer_id parameters have to be 
 * valid, any of the rest can be NULL or 0 in case it is not needed in the
 * message.
 */
int ddlog_log_long_id(
        ddlog_buffer_id_t buffer_id, 
        const char* thread, 
        const char* function, 
        unsigned int line_num, 
        const char* message)
{    
    int res = DDLOG_RET_ERR;
    if (ddlog_lib_inited && ddlog_enabled && message){
        if (buffer_id < DDLOG_MAX_BUF_NUM && ddlog_buffers[buffer_id]){
            res = ddlog_log_internal(ddlog_buffers[buffer_id], thread, function, line_num, message, NULL, 0, DDLOG_EXT_EVENT_TYPE_NONE);
        }
    }
    return res;
}

void ddlog_toggle_status(void){
    if (ddlog_enabled == 1){
        ddlog_disable_internal();
    } else {
        ddlog_enable_internal();
    }
}

int ddlog_get_status(void){
    return ddlog_enabled;
}

void ddlog_inc_indent(void){
    ddlog_thread_indent_level++;
}

void ddlog_dec_indent(void){
    if (ddlog_thread_indent_level > 0){
        ddlog_thread_indent_level--;
    }
}


/******************************************************************************
 *
 * P R I V A T E  functions
 *
 ******************************************************************************/

/**
 * \brief Internal buffer initialization function
 *
 * \param size The maximum number of log messages in the log buffer.
 * \return Pointer to the allocated log buffer or NULL in case of error.
 *
 * By design the library is capable of creating and using multiple log buffers.
 * For the first time only one default log buffer will be used/available for the library
 * user, and the user does not have to provide the log buffer when using the libtrary.
 * The internal functions are capable of working with log buffers other than the default one.
 * Because of this all public library function is a wrapper in which 
 * the internal function is called with the default log buffer.
 */
ddlog_buffer_t* ddlog_init_buffer_internal(size_t size){
    ddlog_buffer_t* buffer = 0;
    ddlog_event_t* event = NULL, *prev_event = NULL;
    unsigned int i = 0;
    int res = 0;

    /* Allocate the log buffer */
    buffer = (ddlog_buffer_t*) malloc(sizeof(ddlog_buffer_t));
    if (buffer == NULL){
        return NULL;
    }
    memset(buffer, 0, sizeof(ddlog_buffer_t));

    /* Allocate all log event structures */
    for (i = 0; i < size; i++){
        event = (ddlog_event_t*) malloc(sizeof(ddlog_event_t));
        if (event == NULL) {
            ddlog_cleanup_buffer_internal(buffer);
            return NULL;
        }
        memset(event, 0, sizeof(ddlog_event_t));
        if (buffer->head == NULL) {
            buffer->head = event;
        } else {
            prev_event->next = event;
        }
        prev_event = event;
    }

    /* Set the defaults, initialize lock */
    event->next = buffer->head;
    buffer->next_write = buffer->head;
    buffer->buffer_size = size;
    res = pthread_spin_init(&buffer->lock, PTHREAD_PROCESS_PRIVATE);
    if (res) {
        ddlog_cleanup_buffer_internal(buffer);
        return NULL;
    }

    return buffer;
}


/**
 * \brief Internal library reset function
 *
 * \param log_buffer The log buffer to be reset
 * \return 0 on success, -1 in case of any error
 *
 * Resets the log buffer provided as a parameter.
 */
int ddlog_reset_buffer_internal(ddlog_buffer_t* log_buffer) {
    int res = DDLOG_RET_ERR, start = 1;
    ddlog_event_t *event = NULL;

    if (log_buffer){
        res = ddlog_lock_buffer_internal(log_buffer);
        if (res){
            return res;
        }

        event = log_buffer->head;
        while (event){
            if (event == log_buffer->head){
                if (start == 1){    /* check if we have reached back to the head again or just starting to delete*/
                    start = 0;
                    ddlog_reset_event_internal(event);
                    event = event->next;
                } else {
                    event = NULL;
                }
            } else {
                ddlog_reset_event_internal(event);
                event = event->next;
            }
        }
        log_buffer->wrapped = 0;
        log_buffer->event_locked = 0;
        log_buffer->next_write = log_buffer->head;
        res = ddlog_unlock_buffer_internal(log_buffer);
    }
    return res;
}

/**
 * \brief Internal event reset function
 *
 * \param event The event to be cleared/reset
 *
 * Deletes all the fields in the event structure provided as a parameter
 */
void ddlog_reset_event_internal(ddlog_event_t* event){
    if (event){
        memset(event->function_name, 0, DDLOG_FNAME_BUF_SIZE);
        memset(event->thread_name, 0, DDLOG_TNAME_BUF_SIZE);
        memset(event->message, 0, DDLOG_MSG_BUF_SIZE);
        event->line_number = 0;
        event->indent_level = 0;
        memset(&event->timestamp, 0, sizeof(struct timeval));
        event->lock = 0;
        event->used = 0;
        if (event->ext_data){
            free(event->ext_data);
            event->ext_data = NULL;
        }
        event->ext_data_size = 0;
        event->ext_event_type = DDLOG_EXT_EVENT_TYPE_NONE;
        event->ext_print_cb = NULL;
    }
}

/**
 * \brief Internal event cleanup function
 *
 * \param event The event to be cleaned up
 *
 * Generic event cleanup routine. 
 * Free the extended data if allocated for the event.
 */
void ddlog_cleanup_event_internal(ddlog_event_t* event){
    if (event){
        if (event->ext_data){
            free(event->ext_data);
            event->ext_data = NULL;
        }
        free(event);
    }
}


/**
 * \brief Internal library cleanup function
 *
 * \param buffer The ddlog buffer to be released
 *
 * Generic cleanup routine. Free all allocated memory (events and the buffer)
 */
/* TODO: free spinlock of the buffer */
void ddlog_cleanup_buffer_internal(ddlog_buffer_t* buffer){
    ddlog_event_t *event, *tmp = 0;
    int res = 0;
    int start = 1;
    if (buffer){
        res = ddlog_lock_buffer_internal(buffer); /* lock the buffer so no other thread will try to log a new event */
        if (res == -1) {
            return;
        }
        event = buffer->head;
        while (event){
            tmp = event;
            if (event == buffer->head){
                if (start == 1){    /* check if we have reached back to the head again or just starting to delete*/
                    start = 0;
                    event = event->next;
                    ddlog_cleanup_event_internal(tmp);
                } else {
                    event = NULL;
                }
            } else {
                event = event->next;
                ddlog_cleanup_event_internal(tmp);
            }
        }
        free(buffer);
    }
}

/**
 * \brief Internal function for saving a new log message in the buffer
 *
 * \param log_buffer The buffer into the new message will be placed
 * \param thread The thread name from where the message is logged (optional)
 * \param function The name of the function from where the message is logged (optional)
 * \param line_num The source code line number of the log message (optional)
 * \param message The log message string
 * \return 0 on success, -1 in case of error
 *
 * This function is the workhorse of the log message handling.
 * Gets the next free log buffer position and copies the message into the buffer
 * The buffer is locked only for the pointer handling. As soon as the log message
 * structure for the new message is secured, a temporary pointer is provided, and the 
 * buffer lock is released.
 */
int ddlog_log_internal(
        ddlog_buffer_t* log_buffer, 
        const char* thread, 
        const char* function, 
        unsigned int line_num, 
        const char* message,
        void* ext_data,
        size_t ext_data_size,
        ddlog_ext_event_type_t ext_event_type)
{
    ddlog_event_t* event = NULL;
    int res = 0;
    unsigned char lock_state = 0;

    /* grab the buffer lock
     * get the next free slot and release the lock as soon as possible 
     * if the lock cannot be aquired, return with error
     */
    res = ddlog_lock_buffer_internal(log_buffer);
    if (res == 0){
        event = log_buffer -> next_write;
        log_buffer -> next_write = event->next;
        if (log_buffer->next_write == log_buffer->head){
            log_buffer->wrapped++;
        }
        res = ddlog_unlock_buffer_internal(log_buffer);
        if (res) {
            return DDLOG_RET_ERR;
        }
    } else {
        return DDLOG_RET_ERR;
    }

    /* Check if the event is not locked, i.e. other thread is not
     * filling the event structure. This could happen if the buffer
     * is wrapped since the event structure is provided to another
     * thread but that one has not been finished updating the event 
     * structure. 
     *
     * The buffer lock is placed to the buffer only during the
     * next write pointer is selected. If the buffer wraps very 
     * often, the same events could be used by multiple threads
     * so we have to lock the event structure as well.
     *
     * After the event is filled with the data, we release the 
     * event structure lock.
     *
     * If we happen to get a event which is currently locked,
     * we return with -1 and do not store the event.
     */
    lock_state = __sync_lock_test_and_set(&event->lock, 1);
    if (lock_state != 0) {
        /*
         * This event is still in use from another thread.
         * We have to leave now, this event is getting dropped.
         */
        return DDLOG_RET_EVNT_LOCKED;
    }

    gettimeofday(&(event->timestamp), NULL);

    event->thread_name[0] = '\0';
    event->function_name[0] = '\0';
    event->message[0] = '\0';
    /* store thread name if it is provided */
    if (thread){
        strncpy(event->thread_name, thread, DDLOG_TNAME_BUF_SIZE - 1);
    }

    /* store the function name if it is provided */
    if (function){
        strncpy(event->function_name, function, DDLOG_FNAME_BUF_SIZE - 1);
    }

    /* store or clear the line number */
    event->line_number = line_num;

    /* store the log message */
    if (message) {
        strncpy(event->message, message, DDLOG_MSG_BUF_SIZE - 1);
    }

    /* clean up external event data */
    if (event->ext_data != NULL){
        free(event->ext_data);
        event->ext_data = NULL;
        event->ext_data_size = 0;
        event->ext_event_type = DDLOG_EXT_EVENT_TYPE_NONE;
    }

    /* if external log data has been provided, store it in the event */
    if (ext_event_type != DDLOG_EXT_EVENT_TYPE_NONE && ext_data && ext_data_size > 0) {
        event->ext_data = malloc(ext_data_size);
        memcpy(event->ext_data, ext_data, ext_data_size);
        event->ext_event_type = ext_event_type;
        event->ext_print_cb = ddlog_ext_get_print_cb(ext_event_type);
        event->ext_data_size = ext_data_size;
    }

    event->indent_level = ddlog_thread_indent_level;
    event->used = 1;
    __sync_and_and_fetch(&event->lock, 0);

    return DDLOG_RET_OK;
}


/**
 * \brief Internal buffer locking function. Aquire buffer lock.
 *
 * \param buffer Pointer to the buffer to be locked
 * \return 0 in case the lock is aquired, -1 otherwise.
 *
 * Tries to lock the buffer. It the buffer is locked by another thread,
 * the block parameter determines how to proceed. If the parameter is >0,
 * we wait until the lock gets free and we can get it. If the parameter is 0,
 * we do not wait for the lock.
 */
int ddlog_lock_buffer_internal(ddlog_buffer_t* buffer){
    int res = 0;
    if (ddlog_lib_inited && buffer) {
        res = pthread_spin_lock(&buffer->lock);
        return res  == 0 ? DDLOG_RET_OK : DDLOG_RET_ERR;
    }
    return DDLOG_RET_ERR;
}

/**
 * \brief Internal buffer lock release function. Releases buffer lock
 *
 * \param buffer Pointer to the buffer to be released 
 * \return 0 in case the lock is aquired, -1 otherwise.
 */
int ddlog_unlock_buffer_internal(ddlog_buffer_t* buffer) {
    int res = 0;
    if (ddlog_lib_inited && buffer){
        res = pthread_spin_unlock(&buffer->lock);
        return res == 0 ? DDLOG_RET_OK : DDLOG_RET_ERR;
    }
    return DDLOG_RET_ERR;
}

/**
 * \brief Disable the logging
 *
 * Disables the logging globally.
 */
void ddlog_disable_internal(void){
    if (ddlog_enabled == 1){
        __sync_fetch_and_sub(&ddlog_enabled, 1);
    }
}

/**
 * \brief Enable the logging
 *
 * Disables the logging globally.
 */
void ddlog_enable_internal(void){
    if (ddlog_enabled == 0){
        __sync_fetch_and_add(&ddlog_enabled, 1);
    }
}

/**
 * \brief Global locking function
 * 
 * \param full_lock (flag) if not 0 lock all buffers as well and disable logging
 * \return DDLOG_RET_OK if all the locking operation were successful,
 *         DDLOG_RET_ERR otherwise.
 *
 * Grabs the global lock. If the flag is not zero disables the logging and grabs
 * all buffer locks as well. The logging remains blocked until the
 * ddlog_unlock_global is not called.
 * In case of any problems no cleanup is performed as we do not know
 * the status of the library/buffers.
 */
int ddlog_lock_global(int full_lock){
    int res = DDLOG_RET_ERR;
    int pt_res = 0;
    int i = 0;

    if (ddlog_lib_inited && ddlog_global_lock_state == DDLOG_LOCK_UNLOCKED){
        /* grab the global lock */
        pt_res = pthread_spin_lock(&ddlog_global_lock);
        if (pt_res){
            return DDLOG_RET_ERR;
        }
        ddlog_global_lock_state = DDLOG_LOCK_SIMPLE;

        if (full_lock) {
            /* disable the logging globally */
            ddlog_disable_internal();

            /* lock all the buffers individially */
            for (i = 0; i < DDLOG_MAX_BUF_NUM; i++) {
                if (ddlog_buffers[i]) {
                    ddlog_lock_buffer_internal(ddlog_buffers[i]);
                    if (res != DDLOG_RET_OK) {
                        return res;
                    }
                }
            }
            ddlog_global_lock_state = DDLOG_LOCK_FULL;
        }
        return DDLOG_RET_OK;
    }
    return DDLOG_RET_ERR;
}

/**
 * \brief Global unlocking function
 *
 * \return DDLOG_RET_OK if all the unlocking operation were successful,
 *         DDLOG_RET_ERR otherwise.
 *
 * Releases the global lock, releases all the buffer locks and enables logging.
 * In case of any problems no cleanup is performed as we do not know
 * the status of the library/buffers.
 */
int ddlog_unlock_global(void){
    int i = 0;
    int pt_res = 0;
    int res = DDLOG_RET_ERR;

    if (ddlog_lib_inited && (ddlog_global_lock_state == DDLOG_LOCK_SIMPLE || ddlog_global_lock_state == DDLOG_LOCK_FULL)){
        if (ddlog_global_lock_state == DDLOG_LOCK_FULL) {
            for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
                if (ddlog_buffers[i]){
                    res = ddlog_unlock_buffer_internal(ddlog_buffers[i]);
                    if (res != DDLOG_RET_OK){
                        return res;
                    }
                }
            }
        }

        pt_res = pthread_spin_unlock(&ddlog_global_lock);
        if (pt_res) {
            return DDLOG_RET_ERR;
        }

        if (ddlog_global_lock_state == DDLOG_LOCK_FULL){
            ddlog_enable_internal();
        }
        ddlog_global_lock_state = DDLOG_LOCK_UNLOCKED;

        return DDLOG_RET_OK;
    }
    return DDLOG_RET_ERR;
}



int ddlog_internal_get_max_buf_num(void){
    return DDLOG_MAX_BUF_NUM;
}

int ddlog_internal_is_lib_inited(void){
    return (ddlog_lib_inited > 0);
}

int ddlog_internal_is_logging_enabled(void){
    return ddlog_enabled;
}


char* ddlog_internal_get_thread_name(void){
    return ddlog_thread_name;
}

ddlog_buffer_t* ddlog_internal_get_default_buf(void){
    return ddlog_default_buf;
}

ddlog_buffer_id_t ddlog_internal_get_default_buf_id(void){
    return ddlog_default_buf_id;
}

/**
 * \brief Provides the buffer pointer by buffer id
 *
 * \param buffer_id The id of the log buffer
 * 
 * Returns with the buffer pointer by the specified id value
 */
ddlog_buffer_t* ddlog_internal_get_buffer_by_id(ddlog_buffer_id_t buffer_id){
    if (ddlog_lib_inited && buffer_id < DDLOG_MAX_BUF_NUM){
        return ddlog_buffers[buffer_id];
    }
    return NULL;
}


/******************************************************************************
 *
 * I N T E R N A L  D E B U G  functions
 *
 ******************************************************************************/

ddlog_buffer_t* ddlog_dbg_get_default_buffer(void){
    return ddlog_default_buf;
}

void ddlog_dbg_print_event(FILE* stream, ddlog_event_t* event){
    char buffer[256];
    ddlog_display_format_event_str(event, buffer, sizeof(buffer));
    if (buffer[0] != '\0'){
        fprintf(stream, "%s\n", buffer);
    }
}

void ddlog_dbg_print_buffer(FILE* stream, ddlog_buffer_t* buffer){
    int res = 0;
    int start = 1;
    ddlog_event_t* event = NULL;

    if (buffer){
        res = ddlog_lock_buffer_internal(buffer);
        if (res){
            return;
        }
        fprintf(stream, "--------------------------------------------------\n");
        fprintf(stream, " Buffer head         : %p\n", (void*) buffer->head);
        fprintf(stream, " Buffer next         : %p\n", (void*) buffer->next_write);
        fprintf(stream, " Buffer size         : %u\n", (unsigned int) buffer->buffer_size);
        fprintf(stream, " Buffer wrapped      : %d\n", buffer->wrapped);
        fprintf(stream, " Buffer event locked : %d\n", buffer->event_locked);
        fprintf(stream, "--------------------------------------------------\n");
        event = buffer->head;
        while(event){
            if (event == buffer->head){
                if (start == 1){
                    start = 0;
                    ddlog_dbg_print_event(stream, event);
                    event = event->next;
                } else {
                    event = NULL;
                }
            } else {
                ddlog_dbg_print_event(stream, event);
                event = event->next;
            }
        }

        res = ddlog_unlock_buffer_internal(buffer);
    } else {
        fprintf(stream, "The buffer is not initialized\n");
    }
}


void ddlog_dbg_print_lib_status(FILE* stream){
    if (stream == NULL){
        return;
    }
    fprintf(stream, "--------------------------------------------------\n");
    fprintf(stream, "                   Library status\n");
    fprintf(stream, "--------------------------------------------------\n");
    fprintf(stream, "ddlog_lib_inited  : %d\n", ddlog_lib_inited);
    fprintf(stream, "ddlog_global_lock : 0x%08x\n", ddlog_global_lock);
    fprintf(stream, "ddlog_enabled     : %d\n", ddlog_enabled);
}

void ddlog_dbg_print_buffer_status(FILE* stream, const ddlog_buffer_t* buffer){
    if (stream == NULL || buffer == NULL){
        return;
    }
    fprintf(stream, "--------------------------------------------------\n");
    fprintf(stream, "                   Buffer status\n");
    fprintf(stream, "--------------------------------------------------\n");
    fprintf(stream, "Buffer head         : %p\n", (void*) buffer->head);
    fprintf(stream, "Buffer next         : %p\n", (void*) buffer->next_write);
    fprintf(stream, "Buffer size         : %u\n", (unsigned int) buffer->buffer_size);
    fprintf(stream, "Buffer wrapped      : %d\n", buffer->wrapped);
    fprintf(stream, "Buffer event locked : %d\n", buffer->event_locked);
    fprintf(stream, "Buffer lock:        : 0x%08x\n", buffer->lock);
}

void ddlog_dbg_print_buffers(FILE* stream){
    int i = 0;
    int start = 1;
    ddlog_event_t* event = NULL;

    if (stream == NULL){
        return;
    }

    ddlog_dbg_print_lib_status(stream);
    if (ddlog_lib_inited){
        ddlog_lock_global(0);
        fprintf(stream, "\n(*) Grab global lock...\n\n");
        ddlog_dbg_print_lib_status(stream);
        fprintf(stream, "\n");
        fprintf(stream, "==================================================\n");
        fprintf(stream, "                  DDLOG buffers\n");
        fprintf(stream, "==================================================\n");
        for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
            fprintf(stream, "Buffer index: %d\n", i);
            if (ddlog_buffers[i] == NULL){
                fprintf(stream, "This buffer is not initialized.\n");
            } else {
                ddlog_dbg_print_buffer_status(stream, ddlog_buffers[i]);
                fprintf(stream, "\n(*) Grab buffer lock...\n\n");
                ddlog_lock_buffer_internal(ddlog_buffers[i]);
                ddlog_dbg_print_buffer_status(stream, ddlog_buffers[i]);
                fprintf(stream, "--------------------------------------------------\n");
                fprintf(stream, "Contents of this buffer\n");
                fprintf(stream, "--------------------------------------------------\n");
                event = ddlog_buffers[i]->head;
                start = 1;
                while(event){
                    if (event == ddlog_buffers[i]->head){
                        if (start == 1){
                            start = 0;
                            ddlog_dbg_print_event(stream, event);
                            event = event->next;
                        } else {
                            event = NULL;
                        }
                    } else {
                        ddlog_dbg_print_event(stream, event);
                        event = event->next;
                    }
                }
                ddlog_unlock_buffer_internal(ddlog_buffers[i]);
            }
            fprintf(stream, "--------------------------------------------------\n");
        }
        ddlog_unlock_global();
    } else {
        fprintf(stream, "The ddlog library has not been initialized\n");
    }
}
