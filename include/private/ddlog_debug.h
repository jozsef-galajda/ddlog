#ifndef __DDLOG_DEBUG_H
#define __DDLOG_DEBUG_H
#include "private/ddlog_internal.h"

ddlog_buffer_t* ddlog_dbg_get_default_buffer(void);
void ddlog_dbg_print_event(FILE* stream, ddlog_event_t* event);
void ddlog_dbg_print_buffer(FILE* stream, ddlog_buffer_t* buffer);
void ddlog_dbg_print_buffers(FILE* stream);

#endif
