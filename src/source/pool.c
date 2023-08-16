#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/pool.h"

// creating a work queue
tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (func == NULL)
        return NULL;

    work = malloc(sizeof(*work));
    work->func = func;
    work->arg = arg;
    work->next = NULL;
    return work;
}

void tpool_work_destroy(tpool_work_t *work)
{
    if (work == NULL)
        return;
    free(work);
}

tpool_work_t *tpool_work_get(tpool_t *tm)
{
    tpool_work_t *work;

    if (tm == NULL)
        return NULL;

    work = tm->work_first;

    if (work == NULL)
        return NULL;

    if (work->next == NULL)
    {
        tm->work_first = NULL;
        tm->work_last = NULL;
    }
    else
    {
        tm->work_first = work->next;
    }

    return work;
}

void *tpool_worker(void *arg)
{
    tpool_t *tm = arg;
    tpool_work_t *work;

    while (1)
    { // keep the thread running

        pthread_mutex_lock(&(tm->work_mutex)); // lock pool

        /**check if there is work
         * if none wait until signaled
         * to check again;
         */
        while (tm->work_first == NULL && tm->stop == 0)
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));

        /*exit if stop*/
        if (tm->stop == 1)
            break;

        // get work from the pool
        work = tpool_work_get(tm);
        // increment working count
        tm->working_cnt++;

        // unlock the pool
        pthread_mutex_unlock(&(tm->work_mutex));

        // process work and destroy the work object
        if (work != NULL)
        {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        // lock and decrement working count
        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;

        // if no work and no threads working , wake the wait function
        if (tm->stop == 0 && tm->working_cnt == 0 && tm->work_first == NULL)
            pthread_cond_signal(&(tm->working_cond));

        pthread_mutex_unlock(&(tm->work_mutex)); // unlock pool
    }
    // this thread is stopping, decrement thread count
    tm->thread_cnt--;

    // signal that a thread has exited
    pthread_cond_signal(&(tm->working_cond));

    pthread_mutex_unlock(&(tm->work_mutex)); // unlock
    return NULL;
}

// create pool
tpool_t *tpool_create(size_t num)
{
    tpool_t *tm;
    pthread_t thread;
    size_t i;

    // set to 2 if num is 0
    if (num == 0)
        num = 2;

    tm = calloc(1, sizeof(*tm));
    tm->thread_cnt = num;

    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);

    tm->work_first = NULL;
    tm->work_last = NULL;

    // spawning threads and detaching them
    for (i = 0; i < num; i++)
    {
        pthread_create(&thread, NULL, tpool_worker, tm);
        pthread_detach(thread);
    }

    return tm;
}

// destroy pool
void tpool_destroy(tpool_t *tm)
{
    tpool_work_t *work;
    tpool_work_t *work2;

    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));
    work = tm->work_first;
    while (work != NULL)
    {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }

    tm->stop = 1; // set stop to 1

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    tpool_wait(tm);

    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->working_cond));
    pthread_cond_destroy(&(tm->work_cond));

    free(tm); // wait for all threads
}

// add work to the queue
int tpool_add_work(tpool_t *tm, thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (tm == NULL)
        return 1;

    work = tpool_work_create(func, arg); // create work object

    if (work == NULL)
        return 1;

    pthread_mutex_lock(&(tm->work_mutex)); // lock

    // adding
    if (tm->work_first == NULL)
    {
        tm->work_first = work;
        tm->work_last = tm->work_first;
    }
    else
    {
        tm->work_last->next = work;
        tm->work_last = work;
    }

    pthread_cond_broadcast(&(tm->work_cond)); // notify that their is work

    pthread_mutex_unlock(&(tm->work_mutex)); // unlock

    return 0;
}

// A blocking function that will only
// return if there is no work
// in the
// pool
void tpool_wait(tpool_t *tm)
{
    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));

    while (1)
    {
        if ((tm->stop == 0 && tm->working_cnt != 0) ||
            (tm->stop == 1 && tm->thread_cnt != 0))
        {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        }
        else
        {
            break;
        }
    }

    pthread_mutex_unlock(&(tm->work_mutex));
}
