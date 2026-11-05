
#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include "lwp.h"
#include "rr.h"

/* Light Weight Processes (LWP)
creates a new lightweight process which executes the given function
with the given argument
returns the lightweight thread id of the new thread or no thread*/

#ifndef LWP_STACK_SIZE
#define LWP_STACK_SIZE (1024 * 1024)
#endif

static tid_t next_tid = 1;
static struct threadinfo_st original_thread;
static thread current_lwp = NULL;
static thread thread_list_head;
static thread terminated_head;
static thread terminated_tail;
static thread waiting_head;
static thread waiting_tail;
static scheduler current_sched = NULL;
static void lwp_wrap(lwpfun function, void *arg){
    int rval = function(arg);
    lwp_exit(rval);
}

tid_t lwp_create(lwpfun function, void *argument){
    thread new_thread = malloc(sizeof(struct threadinfo_st));
    if(!new_thread) {
        return NO_THREAD;
    }
    memset(new_thread, 0, sizeof(struct threadinfo_st));
    size_t stacksize = LWP_STACK_SIZE;
    void *stack_bottom = mmap(
        NULL,
        stacksize,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1,
        0
    );
    if(stack_bottom == MAP_FAILED){
        free(new_thread);
        return NO_THREAD;
    }

    new_thread->lib_one = thread_list_head;
    thread_list_head = new_thread;
    new_thread->tid = next_tid++;
    new_thread->stack = (unsigned long *)stack_bottom;
    new_thread->stacksize = stacksize;
    new_thread->status = LWP_LIVE;
    new_thread->state.fxsave = FPU_INIT;

    unsigned long *stackpointer = (unsigned long *)((char *)stack_bottom + stacksize);
    stackpointer = (unsigned long*)((unsigned long)stackpointer & ~15);

    *(--stackpointer) = 0;
    *(--stackpointer) = (unsigned long)lwp_wrap;
    *(--stackpointer) = 0;
    new_thread->state.rsp = (unsigned long)stackpointer;
    new_thread->state.rbp = (unsigned long)stackpointer;
    new_thread->state.rdi = (unsigned long)function;
    new_thread->state.rsi = (unsigned long)argument;

    scheduler current_scheduler = lwp_get_scheduler();
    if(current_scheduler && current_scheduler->admit){
        current_scheduler->admit(new_thread);
    }
    return new_thread->tid;
}

/* void lwp_start(void)
converts calling thread into a LWP and yeilds to whichever thread the scheduler chooses*/
void lwp_start(void){
    scheduler current_scheduler = lwp_get_scheduler();
    if(!current_scheduler || !current_scheduler->admit){
        return;
    }
    original_thread.tid = next_tid++;
    original_thread.status = LWP_LIVE;
    original_thread.stack = NULL;
    original_thread.lib_one = thread_list_head;
    thread_list_head = &original_thread;

    current_lwp = &original_thread;
    current_scheduler->admit(&original_thread);
    lwp_yield();
}

/* void lwp_yield(void)
yields control to another LWP based on teh scheduler choice)*/
void lwp_yield(void){
    scheduler current_scheduler = lwp_get_scheduler();
    if(!current_scheduler || !current_scheduler->next){
        return;
    }
    thread next_thread = current_scheduler->next();
    if(!next_thread || next_thread == current_lwp){
        return;
    }
    /*saves the current LWP's context*/
    thread old_lwp = current_lwp;
    current_lwp = next_thread;

    swap_rfiles(&(old_lwp->state), &(next_thread->state));
}
/* void lwp_exit(int exitval)
terminates the current LWP and yeilds to whichever thread the shceduler chooses*/
void lwp_exit(int exitval){
    scheduler current_scheduler = lwp_get_scheduler();
    if (current_lwp){
        current_lwp->status = MKTERMSTAT(LWP_TERM, exitval);

        /*remove from pool*/
        if(current_scheduler && current_scheduler->remove){
            current_scheduler->remove(current_lwp);
        }
        /* check blocking */
        if(waiting_head != NULL){
            thread waiting_thread = waiting_head;
            waiting_head = waiting_head->exited;
            if(waiting_head == NULL){
                waiting_tail = NULL;
            }
            current_lwp->exited = NULL;
            if(terminated_head == NULL){
                terminated_head = current_lwp;
                terminated_tail = current_lwp;
            }
            else{
                terminated_tail->exited = current_lwp;
                terminated_tail = current_lwp;
            }

            /*finish wait*/
            if (current_scheduler && current_scheduler -> admit) {
                current_scheduler->admit(waiting_thread);
            }
        }
        else{
            current_lwp->exited = NULL;
            if(terminated_head == NULL) {
                terminated_head = current_lwp;
                terminated_tail = current_lwp;
            }
            else{
                terminated_tail->exited = current_lwp;
                terminated_tail = current_lwp;
            }
        }
        /* free its memory*/
        if(current_scheduler && current_scheduler->next){
            thread next_thread = current_scheduler->next();
            if(next_thread){
                current_lwp = next_thread;
                swap_rfiles(NULL, &(next_thread->state));
            }
        }

        current_lwp = NULL;
        exit (exitval & 0xFF);

    }
}

/* tid_t lwp_wait(int* status)
waits for thread to terminate, deallocates its resources, and reports its termination status*/
static thread terminated_head = NULL;
static thread terminated_tail = NULL;
static thread waiting_head = NULL;
static thread waiting_tail = NULL;

tid_t lwp_wait(int *status){
    scheduler current_scheduler = lwp_get_scheduler();
    if(!current_scheduler){
        return NO_THREAD;
    }
    /* FIFO */
    if(terminated_head != NULL){
        thread terminated_thread = terminated_head;
        terminated_head = terminated_head->exited;
        if(terminated_head == NULL){
            terminated_tail = NULL;
        }
        tid_t finished_tid = terminated_thread->tid;

        if(status != NULL){
            *status = LWPTERMSTAT(terminated_thread->status);
        }

        if(terminated_thread->stack){
            munmap(terminated_thread->stack, terminated_thread->stacksize);
        }
        free(terminated_thread);
        return finished_tid;
    }
    /*dont continue to block if none*/
    if(current_scheduler->qlen() <= 1){
        return NO_THREAD;
    }
    /*remove current thread from pool*/
    current_scheduler->remove(current_lwp);

    /*adding to queue*/
    current_lwp->exited = NULL;
    if (waiting_head == NULL){
        waiting_head = current_lwp;
        waiting_tail = current_lwp;
    }
    else{
        waiting_tail->exited = current_lwp;
        waiting_tail = current_lwp;
    }
    /*yield to next thread*/
    lwp_yield();

        /*now avilable in terminated_head*/
    return lwp_wait(status);

}
/*returns thread ID of currently running LWP*/
tid_t lwp_gettid(void){
    if(current_lwp == NULL){
        return NO_THREAD;
    }
    return current_lwp->tid;
}

/*returns thread pointer*/
static thread thread_list_head = NULL;

thread tid2thread(tid_t tid){
    thread curr = thread_list_head;
    while(curr != NULL){
        if(curr->tid == tid){
            return curr;
        }
        curr = curr->lib_one;
    }
    return NULL;
}

scheduler lwp_get_scheduler(void){
    if(current_sched == NULL){
        current_sched = RoundRobin;
    }
    return current_sched;
}

void lwp_set_scheduler(scheduler fun){
    scheduler old = lwp_get_scheduler();
    scheduler new_sched = (fun == NULL) ? RoundRobin : fun;
    thread t;

    if(new_sched == old){
        return;
    }
    if(new_sched->init){
        new_sched->init();
    }
    while((t = old->next()) != NULL){
        old->remove(t);
        new_sched->admit(t);
    }
    if(old->shutdown){
        old->shutdown();
    }
    current_sched = new_sched;
}