#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define MICROSECONDS_IN_MILLISECOND (1000)



void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int ret;

    
    usleep(thread_func_args->thread_wait_to_obtain_ms * MICROSECONDS_IN_MILLISECOND);
    
    ret = pthread_mutex_lock (thread_func_args->thread_mutex);  //locking mutex    
    if(ret != 0)
    {
        ERROR_LOG("mutex locking failed\n");
    }
    
    usleep(thread_func_args->thread_wait_to_release_ms * MICROSECONDS_IN_MILLISECOND);
    
    ret = pthread_mutex_unlock (thread_func_args->thread_mutex);    //unlocking mutex
    if(ret != 0)
    {
        ERROR_LOG("mutex unlocking failed");
    }
    
    //since thread completed, set status to true
    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     * 
     * See implementation details in threading.h file comment block
     */

    int ret;

    struct thread_data* thread_param = (struct thread_data*)malloc(sizeof(struct thread_data));
    if(thread_param == NULL)
    {
        ERROR_LOG("dynamic memory allocation failed");
    }

            
        thread_param->thread_wait_to_obtain_ms = wait_to_obtain_ms;
        thread_param->thread_wait_to_release_ms = wait_to_release_ms;
        thread_param->thread_mutex = mutex;
        thread_param->thread_complete_success = false;


        ret = pthread_create(thread, NULL, threadfunc, thread_param);
        if(ret == 0)
        {            
            return true;    //thread creation successful
        }
        

    ERROR_LOG("Failed to create thread : error %d", ret);
    return false;
}

