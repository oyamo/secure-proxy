
// written by Victor Murimi 8/16/2023

#ifndef SECURE_PROXY_POOL_H
#define SECURE_PROXY_POOL_H
struct tpool;
typedef struct tpool tpool_t;

typedef void * (*thread_func_t)(void * arg);

/**
 * create pool
* @param num = size og the pool
*/
tpool_t * tpool_create(size_t num);

/**
 * destroy pool
 * @param tm = pointer to pool
 * @return void
 * */
void tpool_destroy(tpool_t *tm);

/**
 * add work to the pool
 * @param tm pointer to pool
 * @param func function to run
 * @param args pointer to function func arguments
 * */
int tpool_add_work(tpool_t * tm,thread_func_t func,void * arg);

/**
 * block until all work has been done
 * @param tm pointer to pool
 * */
void tpool_wait(tpool_t * tm);






/*work queue to store functions
* and their arguments
*/
struct tpool_work{
    thread_func_t func;
    void * arg;
    struct tpool_work * next;
};

typedef struct tpool_work tpool_work_t;


//the pool
struct tpool{

    tpool_work_t * work_first;
    tpool_work_t * work_last;


    //for locking
    pthread_mutex_t work_mutex;


    pthread_cond_t work_cond;   //signals the threads
                                //that their is work



    pthread_cond_t working_cond;    //to signals if there is no
                                    // threads working


    size_t working_cnt;//how many threads are active


    size_t thread_cnt;//how many threads


    int stop;//stops the pool; set to 1(true) or 0(false)
};

/**
 * get work from the queue
 * @param tm pointer to pool
**/
tpool_work_t * tpool_work_get(tpool_t *tm);

/**
 * worker function
 * waits for work and processes it
 * */
void * tpool_worker(void *arg);

#endif/* SECURE_PROXY_POOL_H */