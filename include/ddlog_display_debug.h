/*
 * Copyright (c) 2014 Jozsef Galajda <jgalajda@pannongsm.hu>
 * All rights reserved.
 */

#ifndef __DDLOG_DISPLAY_DEBUG_H
#define __DDLOG_DISPLAY_DEBUG_H
#include <stdio.h>
#include "ddlog.h"

void ddlog_display_debug_print_buffer_id(FILE* stream, ddlog_buffer_id_t buffer_id);
void ddlog_display_debug_print_all_buffers(FILE* stream, int print_status, int print_events);

#endif
