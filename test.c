#include <stdio.h>
#include "lwp.h"

int thread_function(void *arg){
    char *name = (char *)arg;
    int i;
    for(i = 0; i < 3; i++){
        printf("Thread %s is running (%d)\n", name, i);
        lwp_yield();
    }
    printf("Thread %s exiting\n", name);
    return 0;
}

int main(void){
    printf("Starting LWP test...\n");
    lwp_create(thread_function, "A");
    lwp_create(thread_function, "B");

    lwp_start();
    while(lwp_wait(NULL) != NO_THREAD);
    printf("LWP system finished.\n");
    return 0;
}
