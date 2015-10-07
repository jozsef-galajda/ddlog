/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "ddlog.h"
#include "ddlog_internal.h"
#include "ddlog_display.h"

int ddlog_display_indention_enabled = 0;

void ddlog_display_format_timestamp(char* buffer, size_t size, const struct timeval* t){
    struct tm bdt;
    size_t len = 0;

    memset(&bdt, 0, sizeof(bdt));
    memset(buffer, 0, size);

    localtime_r(&t->tv_sec, &bdt);
    strftime(buffer, size - 1, "%m/%d/%y %H:%M:%S", &bdt);
    len = strlen(buffer);
    snprintf(buffer + len, size - len - 1, ".%-6ld", t->tv_usec);
}

void ddlog_display_format_indent(char* buffer, size_t size, uint8_t indent_level){
    memset(buffer, 0, size);
    buffer[0] = ' ';
    if (ddlog_display_indention_enabled == 1 && indent_level > 0) {
        snprintf(buffer + 1, size - 1, "%*c", indent_level*2, ' ');
    }
}

void ddlog_display_format_event_str(const ddlog_event_t* event, char* buffer, size_t buffer_size){
    char timestamp_str[30];
    char indent_str[30];

    if (event == NULL || buffer == NULL || buffer_size == 0){
        return;
    }

    memset(buffer, 0, buffer_size);
    ddlog_display_format_timestamp(timestamp_str, sizeof(timestamp_str), &event->timestamp);
    ddlog_display_format_indent(indent_str, sizeof(indent_str), event->indent_level);

    snprintf(buffer, buffer_size - 1,
            "%s%s[%s:%s:%u]: %s",
            timestamp_str,
            indent_str, 
            event->thread_name[0] != '\0' ? event->thread_name : "-",
            event->function_name[0] != '\0' ? event->function_name : "-",
            event->line_number,
            event->message[0] != '\0' ? event->message : "-");
}


void ddlog_display_event(FILE* stream, const ddlog_event_t* event){
    char buffer[256];

    memset(buffer, 0, sizeof(buffer));
    ddlog_display_format_event_str(event, buffer, sizeof(buffer));
    fprintf(stream, "%s\n", buffer);
    if (event->ext_event_type != DDLOG_EXT_EVENT_TYPE_NONE && event->ext_data &&
            event->ext_data_size > 0 && event->ext_print_cb){
        fprintf(stream, "\n");
        event->ext_print_cb(stream, event->ext_data, event->ext_data_size);
        fprintf(stream, "\n");
    }
}

/* print the defaul buffer */
void ddlog_display_print_buffer(FILE* stream){
    ddlog_display_print_buffer_id(stream, 0);
}

/*print a buffer with a specified id */
void ddlog_display_print_buffer_id(FILE* stream, ddlog_buffer_id_t buffer_id){
    ddlog_event_t* event = NULL, *start = NULL;
    int res = 0;
    int first = 1;

    ddlog_buffer_t* buffer = ddlog_internal_get_buffer_by_id(buffer_id);
    if (stream && buffer){
        res = ddlog_lock_buffer_internal(buffer);
        if (res){
            return;
        }

        if (buffer->wrapped != 0){
            event = start = buffer->next_write;
        } else {
            event = start = buffer->head;
        }

        while (event){
            if (event == start){
                if (first == 1){
                    first = 0;
                    if (event->used == 1){
                        ddlog_display_event(stream, event);
                        event = event->next;
                    } else {
                        event = NULL;
                    }
                } else {
                    event = NULL;
                }
            } else {
                if (event->used){

                    ddlog_display_event(stream, event);
                    event=event->next;
                } else {
                    event = NULL;
                }
            }
        }
        res = ddlog_unlock_buffer_internal(buffer);
    }
}


/**
 * \brief Print the buffer list and status
 *
 * \param stream The stream to print the buffer list into
 *
 * Prints a short buffer list and status into the stream provided as a parameter.
 */
void ddlog_display_print_buffer_list(FILE* stream){
    int i = 0;
    if (ddlog_internal_is_lib_inited() && stream){
        for (i = 0; i < ddlog_internal_get_max_buf_num(); i++){
            fprintf(stream, "DDLOG log buffer #%d: %s\n", i, ddlog_internal_get_buffer_by_id(i) ? "initialized" : "not initialized");
        }
    }
}

void ddlog_display_enable_indention(void){
    ddlog_display_indention_enabled = 1;
}

void ddlog_display_disable_indention(void){
    ddlog_display_indention_enabled = 0;
}
