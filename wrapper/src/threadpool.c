#include "threadpool.h"

static void* worker_routine(void *p)
{
    threadpool *pool = (threadpool *)p;
    task t;
    while(1)
    {
        pthread_mutex_lock(pool->lock);
        while(!pool->b_shutdown && pool->i_worker_idle == 0)
            pthread_cond_wait(pool->ready, pool->lock);
        
        if(pool->b_shutdown == IMMIDIATE || (pool->b_shutdown == GRACEFUL && pool->i_worker_idle == 0))
            break;
        
        t.func = pool->queue[pool->head].func;
        t.arg = pool->queue[pool->head].arg;
        pool->head = (pool->head + 1) % pool->i_queue;
        pool->i_worker_idle--;
        
        pthread_mutex_unlock(pool->lock);
        t.func(t.arg);
    }
    pool->i_thread_started--;
    
    pthread_mutex_unlock(pool->lock);
    pthread_exit(NULL);
    return NULL;
}

int threadpool_free(threadpool *pool)
{
    if(pool == NULL || pool->i_thread_started != 0)
        return -1;
    if(pool->thread_array)
    {
        free(pool->thread_array);
        free(pool->queue);
        
        pthread_mutex_lock(pool->lock);
        pthread_mutex_destroy(pool->lock);
        pthread_cond_destroy(pool->ready);
    }
    free(pool);
    return 0;
}

int threadpool_destroy(threadpool *pool, int flag)
{
    int i;
    if(NULL == pool || pthread_mutex_lock(pool->lock) != 0)
        return 1;
    
    if(pool->b_shutdown)
        return 0;
    pool->b_shutdown = flag;
    if((pthread_cond_broadcast(pool->ready)) != 0 || pthread_mutex_unlock(pool->lock))
        return 2;
    for(i = 0; i < pool->i_thread_started; i++)
        if(pthread_join(pool->thread_array[i], NULL) != 0)
            return 3;
    if(threadpool_free(pool) != 0)
        return 4;
    return 0;
}

threadpool* threadpool_init(int i_threads, int i_workers)
{
    threadpool * pool;
    int i;
    if(NULL == (pool = (threadpool*)malloc(sizeof(threadpool))))
        goto err;
    pool->b_shutdown = 0;
    pool->i_queue = i_workers;
    pool->head = 0;
    pool->tail = 0;
    pool->i_worker_idle = 0;
    pool->i_thread_array = 0;
    pool->i_thread_started = 0;
    pool->thread_array = (pthread_t *)malloc(sizeof(pthread_t) * i_threads);
    pool->queue = (worker *)malloc(sizeof(worker) * i_workers);
    pool->ready = &pool->r;
    pool->lock = &pool->l;
    
    if(pthread_cond_init(pool->ready, NULL) != 0 ||
       pthread_mutex_init(pool->lock, NULL) != 0 ||
       pool->queue == NULL ||
       pool->thread_array == NULL)
        goto err;
    for(i = 0; i < i_threads; i++)
    {
        if(pthread_create(&pool->thread_array[i], NULL, worker_routine, (void *)pool) != 0)
        {
            threadpool_destroy(pool, 2);
            return NULL;
        }
        pool->i_thread_array++;
        pool->i_thread_started++;
    }
    return pool;
err:
    if(pool)
        threadpool_free(pool);
    return NULL;
}

int threadpool_add(threadpool *pool, void *(*function)(void *), void *argument, int flags)
{
    int err = 0;
    int next;
    (void) flags;
    
    if(pool == NULL || function == NULL)
        return 1;
    
    if(pthread_mutex_lock(pool->lock) != 0)
        return 2;
    
    next = (pool->tail + 1) % pool->i_queue;
    
    /* Are we full ? */
    if(pool->i_worker_idle == pool->i_queue)
        return 3;
    /* Are we shutting down ? */
    if(pool->b_shutdown)
        return 4;
        
    /* Add task to queue */
    pool->queue[pool->tail].func = function;
    pool->queue[pool->tail].arg = argument;
    pool->tail = next;
    pool->i_worker_idle += 1;
        
    /* pthread_cond_broadcast */
    if(pthread_cond_signal(pool->ready) != 0)
        return 4;
    if(pthread_mutex_unlock(pool->lock) != 0)
        return 5;
    return err;
}
