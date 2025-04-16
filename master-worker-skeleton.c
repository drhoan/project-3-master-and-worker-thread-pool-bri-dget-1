#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce = 0, curr_buf_size = 0;
int total_items, max_buf_size, num_workers, num_masters;

int *buffer;
int consume_index = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

void print_produced(int num, int master)
{
    printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker)
{
    printf("Consumed %d by worker %d\n", num, worker);
}

// produce items and place in buffer
void *generate_requests_loop(void *data)
{
    int thread_id = *((int *)data);

    while (1)
    {
        pthread_mutex_lock(&mutex);

        if (item_to_produce >= total_items)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // wait if buffer is full
        while (curr_buf_size >= max_buf_size)
        {
            pthread_cond_wait(&empty, &mutex);
        }

        // add item to buffer
        buffer[curr_buf_size++] = item_to_produce;
        print_produced(item_to_produce, thread_id);
        item_to_produce++;

        pthread_cond_signal(&full);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// consume items from buffer
void *consume_requests_loop(void *data)
{
    int thread_id = *((int *)data);

    while (1)
    {
        pthread_mutex_lock(&mutex);

        // all items produced and consumed
        if (consume_index >= total_items)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // wait if buffer is empty
        while (curr_buf_size <= 0 && consume_index < total_items)
        {
            pthread_cond_wait(&full, &mutex);
        }

        // extra check to avoid over-consuming
        if (consume_index >= total_items)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // remove item from buffer
        int item = buffer[--curr_buf_size];
        print_consumed(item, thread_id);
        consume_index++;

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int *master_thread_id, *worker_thread_id;
    pthread_t *master_thread, *worker_thread;

    if (argc < 5)
    {
        printf("./master-worker #total_items #max_buf_size #num_workers #num_masters e.g. ./exe 10000 1000 4 3\n");
        exit(1);
    }
    else
    {
        total_items = atoi(argv[1]);
        max_buf_size = atoi(argv[2]);
        num_workers = atoi(argv[3]);
        num_masters = atoi(argv[4]);
    }

    buffer = (int *)malloc(sizeof(int) * max_buf_size);

    // master threads
    master_thread_id = (int *)malloc(sizeof(int) * num_masters);
    master_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_masters);

    for (int i = 0; i < num_masters; i++)
    {
        master_thread_id[i] = i;
        pthread_create(&master_thread[i], NULL, generate_requests_loop, (void *)&master_thread_id[i]);
    }

    // worker threads
    worker_thread_id = (int *)malloc(sizeof(int) * num_workers);
    worker_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_workers);

    for (int i = 0; i < num_workers; i++)
    {
        worker_thread_id[i] = i;
        pthread_create(&worker_thread[i], NULL, consume_requests_loop, (void *)&worker_thread_id[i]);
    }

    // wait for all threads to finish
    for (int i = 0; i < num_masters; i++)
    {
        pthread_join(master_thread[i], NULL);
        printf("master %d joined\n", i);
    }

    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(worker_thread[i], NULL);
        printf("worker %d joined\n", i);
    }

    // cleanup
    free(buffer);
    free(master_thread_id);
    free(master_thread);
    free(worker_thread_id);
    free(worker_thread);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);

    return 0;
}
