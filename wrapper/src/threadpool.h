#include <pthread.h>
#include <stdlib.h>

typedef struct worker_t
{
	void *(*func)(void *);
	void *arg;
	struct worker_t *next;
}worker; 

typedef struct threadpool_t
{
	pthread_mutex_t *lock;
	pthread_cond_t *ready;
    pthread_mutex_t l;
    pthread_cond_t r;

	int head;
	int tail;
    worker *queue;
	int i_queue;
    
	pthread_t *thread_array;
	int i_thread_array;

	int b_shutdown;
	int i_worker_idle;
	int i_thread_started;
}threadpool;

typedef struct task_t
{
    void *(*func)(void *);
    void *arg;
}task;

enum shutdownmethod
{
    IMMIDIATE = 1,
    GRACEFUL = 2
};

int threadpool_destroy(threadpool *pool, int flag);
int threadpool_free(threadpool *pool);
int threadpool_destroy(threadpool *pool, int flag);
threadpool* threadpool_init(int i_threads, int i_workers);
int threadpool_add(threadpool *pool, void *(*function)(void *), void *argument, int flags);
