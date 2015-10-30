/*
 * Copyright (c) 2015 Jozsef Galajda <jozsef.galajda@gmail.com>
 * All rights reserved.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include "ddlog.h"
#include "ddlog_ext.h"
#include "private/ddlog_internal.h"
#include "private/ddlog_debug.h"
#include "private/ddlog_display.h"
#include "private/ddlog_display_debug.h"


int start = 0;
int threads_started = 0;
pthread_t *threads;
int test8_run  = 0;

void test2(){
    ddlog_init(15);
    ddlog_log("alma1");
    ddlog_log("alma2");
    ddlog_log("alma3");
    ddlog_log("alma4");
    ddlog_log("alma5");
    ddlog_log("alma6");
    ddlog_log("alma7");
    ddlog_log("alma8");
    ddlog_log("alma9");
    ddlog_display_print_buffer(stderr);
}

pid_t gettid(void){
    return syscall( __NR_gettid );
}



void* thread_routine(void* thread_arg){
    int i = 0;
    int proc_num = (int)(long) thread_arg;
    char thr_name[128];
    char func_name[128];


    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(proc_num, &set);
    if (sched_setaffinity(gettid(), sizeof(cpu_set_t), &set)){
        return NULL;
    }

    memset(thr_name, 0, sizeof(thr_name));
    memset(func_name, 0, sizeof(func_name));
    snprintf(thr_name, sizeof(thr_name) - 1, "thread_%d", proc_num);
    snprintf(func_name, sizeof(func_name) - 1, "thread_routine_%d", proc_num);

    printf("thread %d has been started\n", proc_num);
    while (start == 0) {
        sleep(1);
    }
    printf("thread %d starts logging\n", proc_num);

    for (i = 0; i < 100; i++){
        ddlog_log_long(thr_name, func_name, __LINE__, "alma alma alma alma alma alma alma alma alma alma alma alma papaya");
    }

    return NULL;
}

int setup(){
    int proc_num = 0;
    int i = 0;

    proc_num = (int)sysconf( _SC_NPROCESSORS_ONLN );
    if (proc_num < 0){
        return -1;
    }

    threads_started = proc_num;

    threads = (pthread_t*) malloc(sizeof(pthread_t) * proc_num);
    if (threads == NULL){
        return -1;
    }

    printf( "Starting %d threads...\n", proc_num);
    for (i = 0; i < proc_num; i++){
        if (pthread_create(&threads[i], NULL, thread_routine, (void *)(long)i )){
            i++;
            threads_started = i;
            break;
        }
    }
    printf("threads started: %d\n", threads_started);

    free(threads);
    return 0;
}

void test5(){
    printf("================================================================================\n");
    printf(" Test #5\n");
    printf("================================================================================\n");
    printf("(*) Initialize library with size of 100\n\n");
    ddlog_init(100);
    ddlog_thread_init("main thread");

    ddlog_display_print_all_buffers(stdout);

    printf("(*) Adding log messages (8)\n\n");
    DDLOG_ENTRY;
    DDLOG("Test log: alma1");
    DDLOG_HEX(&start, 87);
    DDLOG("Test log: alma2");
    DDLOG("Test log: alma3");
    DDLOG_BT;
    DDLOG("Test log: alma4");
    DDLOG_LEAVE;
    printf("================================================================================\n");
    printf("================================================================================\n");
    ddlog_display_print_all_buffers(stdout);
    ddlog_reset();
    DDLOG("Test log2: alma1");
    DDLOG("Test log2: alma2");
    DDLOG("Test log2: alma3");
    DDLOG("Test log2: alma4");
    DDLOG("Test log2: alma5");
    DDLOG("Test log2: alma6");
    DDLOG_BT;
    DDLOG_HEX(&start, 100);
    DDLOG("Test log2: alma7");
    DDLOG("Test log2: alma8");
    DDLOG("Test log2: alma9");
    DDLOG("Test log2: alma10");
    printf("================================================================================\n");
    printf("================================================================================\n");
    ddlog_display_print_all_buffers(stdout);
    ddlog_reset();
    DDLOG("Test log2: alma1");
    DDLOG("Test log2: alma2");
    DDLOG("Test log2: alma3");
    DDLOG("Test log2: alma4");
    DDLOG("Test log2: alma5");
    DDLOG("Test log2: alma6");
    DDLOG_BT;
    DDLOG_HEX(&start, 100);
    DDLOG("Test log2: alma7");
    DDLOG("Test log2: alma8");
    DDLOG("Test log2: alma9");
    DDLOG("Test log2: alma10");
    printf("================================================================================\n");
    printf("================================================================================\n");
    ddlog_display_print_all_buffers(stdout);
    ddlog_start_server();
    ddlog_wait_for_server();
    ddlog_cleanup();
}


void test7(int type){
    int i = 0, j = 0;
    char* buffer;
    ddlog_init(50);
    ddlog_thread_init("test_6_main_thread");
    for (i = 0; i < 10; i++){
        for (j = 0; j< 1500; j++){
            switch (type){
                case 1:
                    DDLOG("log entry");
                    break;
                case 2:
                    DDLOG_HEX(&buffer, 15);
                    break;
                case 3:
                    DDLOG_BT;
                    break;
                case 4:
                    DDLOG("log entry sdlkfjsdklfjsdlkfjsdklfj");
                    DDLOG_HEX(&buffer, 98);
                    DDLOG_BT;
                default:
                    break;
            }
        }
        /*ddlog_display_print_buffer(stdout);*/
        ddlog_reset();
    }
    ddlog_cleanup();
}

void* test8_thr(void* data){
    char* thread_name = (char*) data;
    struct timespec sleep_time;
    struct timespec remaining_time;
    unsigned long counter = 0;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 1000000;

    ddlog_thread_init(thread_name);
    printf("Thread started: %s\n", thread_name);
    while(1){
        if (test8_run == 0) {
            printf("Loop count in thread %s: %lu\n", thread_name, counter);
            break;
        }
        counter++;
        DDLOG("thread message");
        DDLOG_HEX(thread_name, 10);
        DDLOG_BT;
        nanosleep(&sleep_time, &remaining_time);
    }
    return NULL;
}


typedef struct alma{
    int alma1;
    int alma2;
} alma;

typedef struct alma2{
    int a21;
    int a22;
} alma2;


void test6_print_cb(FILE* stream, void* data, size_t size){
    alma* a = (alma*) data;
    fprintf(stream, "alma1: %d\nalma2: %d\n", a->alma1, a->alma2);
}

void test6_print_cb2(FILE* stream, void* data, size_t size){
    alma2* a = (alma2*) data;
    fprintf(stream, "a21: %d\na22: %d\n", a->a21, a->a22);
}



void test6(){
    int data[100];
    int i = 0;
    alma a;
    alma2 a2;
    ddlog_ext_event_type_t et = 0, et2 = 0;
    a.alma1 = 100;
    a.alma2 = 200;

    a2.a21 = 10923;
    a2.a22 = 987;

    for (i = 0; i < 100; i++) {
        data[i] = 1879*i;
    }
    ddlog_init(10);
    ddlog_thread_init("main thread");
    DDLOG("message1");
    DDLOG_BT;
    DDLOG("message2");
    DDLOG_HEX(data, sizeof(data));
    DDLOG("message3");

    et = ddlog_ext_register_event(test6_print_cb);
    printf("event type: %d\n", et);
    et = ddlog_ext_register_event(test6_print_cb);
    printf("event type: %d\n", et);
    et = ddlog_ext_register_event(test6_print_cb);
    printf("event type: %d\n", et);
    et = ddlog_ext_register_event(test6_print_cb);
    printf("event type: %d\n", et);
    et = ddlog_ext_register_event(test6_print_cb);
    printf("event type: %d\n", et);

    et2 = ddlog_ext_register_event(test6_print_cb2);
    printf("event type: %d\n", et2);
    ddlog_ext_log(et, &a, sizeof(a), "Hoki alma");
    ddlog_ext_log(1000, &a, sizeof(a), "Hoki2 alma");
    ddlog_ext_log(et2, &a2, sizeof(a2), "Hoki2222 alma");
    DDLOG_HEX(&a, sizeof(a));

    DDLOG_BT;
    DDLOG_VA("This is a formatted message: %d, %s, %d", 1023, "alma", 12);
    ddlog_display_print_buffer(stdout);
    ddlog_cleanup();
}

void test8(int duration, int reset){
    pthread_t thr1, thr2;
    char* thr1_name = "Thread1";
    char* thr2_name = "Thread2";
    int i = 0;

    ddlog_init(13);
    test8_run = 1;
    pthread_create(&thr1, NULL, test8_thr, (void*)thr1_name);
    pthread_create(&thr2, NULL, test8_thr, (void*)thr2_name);
    for (i = 0; i< duration; i++){
        printf("%d ", i);
        if (reset){
            ddlog_reset();
            printf(" r ");
        }
        fflush(stdout);
        sleep(1);
    }
    test8_run = 0;
    pthread_join(thr1, NULL);
    pthread_join(thr2, NULL);
    ddlog_display_print_buffer(stdout);
    ddlog_cleanup();
}

int test10_b(int param){
    DDLOG_ENTRY;
    printf("%s: %d\n", __FUNCTION__, param);
    /*DDLOG_BT;*/
    DDLOG_LEAVE;
    return param * 100;
}

int test10_a(void){
    DDLOG_ENTRY;
    DDLOG("inside 10_a");
    /*DDLOG_BT;*/
    DDLOG_RET_FN(test10_b(1983), int);
}

int test10_c(int value){
    DDLOG_ENTRY;
    /*DDLOG_BT;*/
    DDLOG("inside 10_c");
    DDLOG_RET_EXP(value *19);
}

void test10(void){
    int a = 0;
    int b = 0;
    ddlog_init(25);
    ddlog_thread_init("main thread");
    DDLOG("test10 has been started");
    DDLOG_ENTRY;
    a = test10_a();
    printf("%s, %d\n", __FUNCTION__, a);
    DDLOG("test10_a has been returned");
    b = test10_c(86);
    printf("%s, %d\n", __FUNCTION__, b);
    DDLOG_LEAVE;
    DDLOG("test10 has been finished");
    ddlog_display_print_buffer(stdout);
    ddlog_start_server();
    ddlog_wait_for_server();
    ddlog_cleanup();

}


int main(){
    test5();
    return 0;
}
