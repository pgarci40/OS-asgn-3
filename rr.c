#include <stdlib.h>
#include "lwp.h"
#include "rr.h"

static thread head = NULL;
static int len = 0;

#define next_thread sched_one
#define prev_thread sched_two

/* admit: add the passed thread to the scheduler's scheduling pool*/
static void rr_admit(thread new){
    len++;
    if(head){
        new->next_thread = head;
        new->prev_thread = head->prev_thread;
        new->prev_thread->next_thread = new;
        head->prev_thread = new;
    }
    else{
        head = new;
        head->next_thread = new;
        head->prev_thread = new;
    }
}

/* remove: remove the passed thread from the scheduler's scheduling pool */
static void rr_remove(thread outgoing){
    len--;
    outgoing->prev_thread->next_thread = outgoing->next_thread;
    outgoing->next_thread->prev_thread = outgoing->prev_thread;

    if(outgoing == head){
        if(outgoing->next_thread != outgoing){
            head = outgoing->next_thread;
        }
        else{
            head = NULL;
        }
    }
}

/* next: return the next thread to be run or NULL if there isn't one*/
static thread rr_next(void){
    thread t = head;
    if (head) {
        head = head->next_thread;
    }
    return t;
}

/* qlen: return the number of runnable threads*/
static int rr_qlen(void){
    return len;
}

struct scheduler rr_publish = {
    NULL, 
    NULL, 
    rr_admit, 
    rr_remove, 
    rr_next, 
    rr_qlen};

scheduler RoundRobin = &rr_publish;

