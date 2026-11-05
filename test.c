#include <stdio.h>
#include "lwp.h"

static int failures = 0;
static char order[16];
static int pos = 0;
static tid_t seen_tid = NO_THREAD;

static void check(int cond, const char *name){
    if(cond){
        printf("PASS: %s\n", name);
    }
    else{
        printf("FAIL: %s\n", name);
        failures++;
    }
}

static int return_value(void *arg){
    return (int)(long)arg;
}

static int log_char(void *arg){
    int i;
    for(i = 0; i < 3; i++){
        order[pos++] = *(char *)arg;
        lwp_yield();
    }
    return 0;
}

static int child(void *arg){
    return 7;
}

static int parent(void *arg){
    lwp_create(child, NULL);
    return 5;
}

static int yield_then_return(void *arg){
    int i;
    for(i = 0; i < 3; i++){
        lwp_yield();
    }
    return (int)(long)arg;
}

static int record_tid(void *arg){
    seen_tid = lwp_gettid();
    return 0;
}

static void test_yield_alone(void){
    lwp_yield();
    check(1, "yield with no other threads returns");
}

static void test_wait_empty(void){
    check(lwp_wait(NULL) == NO_THREAD, "wait with no threads returns NO_THREAD");
}

static void test_exit_status(void){
    tid_t t1, t2, t3;
    int status;
    int ok = 1;

    t1 = lwp_create(return_value, (void *)1);
    t2 = lwp_create(return_value, (void *)2);
    t3 = lwp_create(return_value, (void *)3);

    if(lwp_wait(&status) != t1 || LWPTERMSTAT(status) != 1) ok = 0;
    if(lwp_wait(&status) != t2 || LWPTERMSTAT(status) != 2) ok = 0;
    if(lwp_wait(&status) != t3 || LWPTERMSTAT(status) != 3) ok = 0;

    check(ok, "exit statuses and FIFO wait order");
    check(lwp_wait(NULL) == NO_THREAD, "wait after all reaped returns NO_THREAD");
}

static void test_interleave(void){
    char a = 'A';
    char b = 'B';
    int i;
    int ok = 1;

    pos = 0;
    lwp_create(log_char, &a);
    lwp_create(log_char, &b);
    while(lwp_wait(NULL) != NO_THREAD);

    if(pos != 6) ok = 0;
    for(i = 0; i + 1 < pos; i++){
        if(order[i] == order[i + 1]) ok = 0;
    }
    check(ok, "two threads alternate on yield");
}

static void test_child_create(void){
    int status;
    int sum = 0;
    int count = 0;

    lwp_create(parent, NULL);
    while(lwp_wait(&status) != NO_THREAD){
        sum += LWPTERMSTAT(status);
        count++;
    }
    check(count == 2 && sum == 12, "thread can create another thread");
}

static void test_many(void){
    int i;
    int status;
    int sum = 0;
    int count = 0;

    for(i = 0; i < 100; i++){
        if(lwp_create(yield_then_return, (void *)(long)i) == NO_THREAD){
            check(0, "create 100 threads");
            return;
        }
    }
    while(lwp_wait(&status) != NO_THREAD){
        sum += LWPTERMSTAT(status);
        count++;
    }
    check(count == 100 && sum == 4950, "100 threads run and are all reaped");
}

static void test_gettid(void){
    tid_t t;

    t = lwp_create(record_tid, NULL);
    check(tid2thread(t) != NULL, "tid2thread finds a live thread");
    lwp_wait(NULL);
    check(seen_tid == t, "lwp_gettid matches tid from lwp_create");
    check(tid2thread(t) == NULL, "tid2thread returns NULL after thread is reaped");
}

int main(void){
    lwp_start();

    test_yield_alone();
    test_wait_empty();
    test_exit_status();
    test_interleave();
    test_child_create();
    test_many();
    test_gettid();

    printf("%s\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return failures;
}