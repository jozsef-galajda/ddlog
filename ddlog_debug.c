/*
 * Copyright (c) 2015 Jozsef Galajda <jozsef.galajda@gmail.com>
 * All rights reserved.
 */
/**
 * \file ddlog_debug.c
 * \brief ddlog library debug function implementation
 *
 * This source file contains the implementation of the
 * debug functions needed/used during library testing. No function
 * friom this file is needed in the final code.
 */
#include "ddlog.h"
#include "private/ddlog_internal.h"
#include "private/ddlog_display.h"

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
    fprintf(stream, "ddlog_lib_inited  : %d\n", ddlog_internal_is_lib_inited());
    fprintf(stream, "ddlog_enabled     : %d\n", ddlog_internal_is_logging_enabled());
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
    if (ddlog_internal_is_lib_inited()){
        ddlog_lock_global(0);
        fprintf(stream, "\n(*) Grab global lock...\n\n");
        ddlog_dbg_print_lib_status(stream);
        fprintf(stream, "\n");
        fprintf(stream, "==================================================\n");
        fprintf(stream, "                  DDLOG buffers\n");
        fprintf(stream, "==================================================\n");
        for (i = 0; i < DDLOG_MAX_BUF_NUM; i++){
            ddlog_buffer_t* buffer = ddlog_internal_get_buffer_by_id(i);

            fprintf(stream, "Buffer index: %d\n", i);
            if (buffer == NULL){
                fprintf(stream, "This buffer is not initialized.\n");
            } else {
                ddlog_dbg_print_buffer_status(stream, buffer);
                fprintf(stream, "\n(*) Grab buffer lock...\n\n");
                ddlog_lock_buffer_internal(buffer);
                ddlog_dbg_print_buffer_status(stream, buffer);
                fprintf(stream, "--------------------------------------------------\n");
                fprintf(stream, "Contents of this buffer\n");
                fprintf(stream, "--------------------------------------------------\n");
                event = buffer->head;
                start = 1;
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
                ddlog_unlock_buffer_internal(buffer);
            }
            fprintf(stream, "--------------------------------------------------\n");
        }
        ddlog_unlock_global();
    } else {
        fprintf(stream, "The ddlog library has not been initialized\n");
    }
}
