#ifndef __DDLOG_DISPLAY_H
#define __DDLOG_DISPLAY_H
#include <stdio.h>
#include <stddef.h>
#include <sys/time.h>
#include "ddlog.h"
#include "private/ddlog_internal.h"


void ddlog_display_event(FILE* stream, const ddlog_event_t* event);
void ddlog_display_format_timestamp(char* buffer, size_t size, const struct timeval* t);
void ddlog_display_format_event_str(const ddlog_event_t* event, char* buffer, size_t buffer_size);
void ddlog_display_print_buffer_id(FILE* stream, ddlog_buffer_id_t buffer_id);
void ddlog_display_print_buffer(FILE* stream);
void ddlog_display_print_buffer_list(FILE* stream);

void ddlog_display_enable_indention(void);
void ddlog_display_disable_indention(void);


#endif
