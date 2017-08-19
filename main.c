/**
  * @file main.c
  * @brief Contains the scheduler and the entry point for the implementation of Project 3 on BCM2837.
  *
  * The scheduler is the main entry of the system.
  * Its purpose is to execute and monitor all the tasks needed to complete a full cycle.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (dgeromichalos)
  * @date August, 2017
  */

/******************************** Inclusions *********************************/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>

#include "data_types.h"

/***************************** Macro Definitions *****************************/

/** The CPU affinity of the process. */
#define NUM_CPUS (0u)

/** The priority that will be given to the created tasks (threads) from the OS.
  * Since the PRREMPT_RT uses 50 as the priority of kernel tasklets and
  * interrupt handlers by default, the maximum available priority is chosen.
  * The priority of each task should be the same, since the Round-Robin
  * scheduling policy is used and each task is executed with the same time slice.
  */
#define TASK_PRIORITY (49u)

/** This is the maximum size of the stack which is guaranteed safe access without faulting. */
#define MAX_SAFE_STACK (128u * 1024u)

/** The number of nsecs per sec. */
#define NSEC_PER_SEC (1000000000ul)

/** The max size of the SSID. */
#define SSID_SIZE (64u)

/** The size of the SSID buffer. */
#define BUFFER_SIZE (32u)

/** The cycle time ratio between the two tasks. */
#define READ_STORE_CYCLE_RATIO (0.1f)  /* Store task run 10 times slower than Read */

/***************************** Type Definitions ******************************/

/** The SSID queue for the read/store (producer/consumer) model. */
struct SSIDQueue {
  char ssid_buffer[BUFFER_SIZE][SSID_SIZE];
  f32_t timestamp_buffer[BUFFER_SIZE];

  u32_t head, tail;
  u8_t full, empty;
  
  pthread_mutex_t mutex;
};

/***************************** Static Variables ******************************/

/** The cycle time between the task calls. */
static u64_t read_cycle_time;
static u64_t store_cycle_time;

/** The timers of the tasks. */
static struct timespec read_task_timer;
static struct timespec store_task_timer;

/** The saved ssids and their timestamp. */
static u64_t ssid_num = 0;
static char** ssids;
static u32_t* num_timestamps;
static f32_t** timestamps;

/** The queue to be used by the two tasks. */
static struct SSIDQueue ssid_queue;

/******************** Static General Function Prototypes *********************/

/**
  * @brief Prefault the stack segment that belongs to this process.
  * @return Void.
  */
static void prefaultStack(void);

/**
  * @brief Update the scheduler's timer with a new interval.
  * @param interval The interval needed to perform the update.
  * @return Void.
  */
static void updateInterval(struct timespec* task_timer, u64_t interval);

/**
  * @brief Get the current time.
  * @return The current time.
  */
static f32_t getTimestamp(void);

/**
  * @brief Write SSIDs and their timestamps to a file.
  */
static void writeToFile(void);

/********************* Static Task Function Prototypes ***********************/

static void INIT_TASK(int argc, char** argv);
static void* READ_TASK(void* ptr);
static void* STORE_TASK(void* ptr);
static void EXIT_TASK(void);

/************************** Static General Functions *************************/

void prefaultStack(void)
{
        unsigned char dummy[MAX_SAFE_STACK];

        memset(dummy, 0, MAX_SAFE_STACK);

        return;
}

void updateInterval(struct timespec* task_timer, u64_t interval)
{
        task_timer->tv_nsec += interval;

        /* Normalize time (when nsec have overflowed) */
        while (task_timer->tv_nsec >= NSEC_PER_SEC)
        {
                task_timer->tv_nsec -= NSEC_PER_SEC;
                task_timer->tv_sec++;
        }
}


f32_t getTimestamp(void)
{
        struct timespec current_t;

        clock_gettime(CLOCK_MONOTONIC, &current_t);

        return current_t.tv_sec + (current_t.tv_nsec / (f32_t)1000000000u);
}

void queueAdd(char* ssid, f32_t timestamp)
{
        strcpy(ssid_queue.ssid_buffer[ssid_queue.tail], ssid);
        ssid_queue.timestamp_buffer[ssid_queue.tail] = timestamp;

        ssid_queue.tail++;

        if (ssid_queue.tail == BUFFER_SIZE)
                ssid_queue.tail = 0;
        if (ssid_queue.tail == ssid_queue.head)
                ssid_queue.full = 1;

        ssid_queue.empty = 0;
}

void queuePop(char* ssid, f32_t* timestamp)
{
        strcpy(ssid, ssid_queue.ssid_buffer[ssid_queue.head]);
        *timestamp = ssid_queue.timestamp_buffer[ssid_queue.head];

        ssid_queue.head++;

        if (ssid_queue.head == BUFFER_SIZE)
                ssid_queue.head = 0;
        if (ssid_queue.head == ssid_queue.tail)
                ssid_queue.empty = 1;

        ssid_queue.full = 0;
}

void copySSIDsToBuffer(void)
{
        f32_t timestamp;
        char ssid[SSID_SIZE];

        FILE *file = popen("/bin/bash searchWifi.sh", "r");

        if (file != NULL)
        {
                timestamp = getTimestamp();

                pthread_mutex_lock(&ssid_queue.mutex);

                while (fgets(ssid, sizeof(ssid) - 1, file) != NULL)
                {
                        if (!ssid_queue.full && strncmp(ssid, "x00", 3))  /* skip if SSID is x00* */
                                queueAdd(ssid, timestamp);
                }

                pthread_mutex_unlock(&ssid_queue.mutex);

                pclose(file);
        }
}

void storeSSIDs(void)
{              
        u64_t i, j;
        u8_t ssid_found;
        f32_t timestamp;
        char ssid[SSID_SIZE];

        pthread_mutex_lock(&ssid_queue.mutex);

        for (i = 0; i < BUFFER_SIZE; i++)
        {
                if (ssid_queue.empty)
                        break;
                else
                        queuePop(ssid, &timestamp);

                ssid_found = 0;

                for (j = 0; j < ssid_num; j++)
                {
                        if (!strcmp(ssids[j], ssid) &&  /* equal */
                            timestamps[j][num_timestamps[j] - 1] != timestamp)
                        {
                                num_timestamps[j]++;

                                timestamps[j] = realloc(timestamps[j], sizeof(f32_t) * num_timestamps[j]);
                                timestamps[j][num_timestamps[j] - 1] = timestamp;
                               
                                ssid_found = 1;
                                break;
                        }
                }

                if (!ssid_found)
                {
                        ssid_num++;

                        ssids = realloc(ssids, sizeof(char*) * ssid_num);
                        ssids[ssid_num - 1] = malloc(sizeof(char) * SSID_SIZE);
                        strcpy(ssids[ssid_num - 1], ssid);

                        num_timestamps = realloc(num_timestamps, sizeof(u32_t) * ssid_num);
                        num_timestamps[ssid_num - 1] = 1;

                        timestamps = realloc(timestamps, sizeof(f32_t*) * ssid_num);
                        timestamps[ssid_num - 1] = malloc(sizeof(f32_t));
                        timestamps[ssid_num - 1][0] = timestamp;
                }
        }

        pthread_mutex_unlock(&ssid_queue.mutex);
}

void writeToFile(void)
{
        u64_t i, j;

        FILE *file = fopen("ssids.txt", "w");

        if (file != NULL)
        {
                for (i = 0; i < ssid_num; i++)
                {
                        fprintf(file, "%s", ssids[i]);

                        for (j = 0; j < num_timestamps[i]; j++)
                                fprintf(file, "    %.5f\n", timestamps[i][j]);

                        fprintf(file, "\n");
                }

                fclose(file);
        }
}

/*****************************************************************************/

void INIT_TASK(int argc, char** argv)
{
        if (argc != 2)
        {
                perror("Wrong number of arguments");
                exit(-4);
        }

        read_cycle_time = strtoul(argv[1], NULL, 0) * NSEC_PER_SEC;
        store_cycle_time = read_cycle_time * READ_STORE_CYCLE_RATIO;

        ssid_queue.empty = 1;
        ssid_queue.full = 0;
        ssid_queue.head = 0;
        ssid_queue.tail = 0;
        pthread_mutex_init(&ssid_queue.mutex, NULL);
}

void* READ_TASK(void* ptr)
{
        /* Synchronize scheduler's timer. */
        clock_gettime(CLOCK_MONOTONIC, &read_task_timer);

        while(1)
        {
                /* Calculate next shot */
                updateInterval(&read_task_timer, read_cycle_time);

                copySSIDsToBuffer();

                /* Sleep for the remaining duration */
                (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &read_task_timer, NULL);
        }

        return (void*)NULL;
}

void* STORE_TASK(void* ptr)
{
        /* Synchronize scheduler's timer. */
        clock_gettime(CLOCK_MONOTONIC, &store_task_timer);

        while(1)
        {
                /* Calculate next shot */
                updateInterval(&store_task_timer, store_cycle_time);

                storeSSIDs();
                writeToFile();

                /* Sleep for the remaining duration */
                (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &store_task_timer, NULL);
        }

        return (void*)NULL;
}

void EXIT_TASK(void)
{
        free(ssids);
        free(num_timestamps);
        free(timestamps);

        pthread_mutex_destroy(&ssid_queue.mutex);
}

/********************************** Main Entry *******************************/

s32_t main(int argc, char** argv)
{
        cpu_set_t mask;

        pthread_t thread_1;
        pthread_attr_t attr_1;
        struct sched_param param_1;

        pthread_t thread_2;
        pthread_attr_t attr_2;
        struct sched_param param_2;

        /*********************************************************************/

        /* Lock memory. */
        if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1)
        {
                perror("mlockall failed");
                exit(-2);
        }

        prefaultStack();

        CPU_ZERO(&mask);
        CPU_SET(NUM_CPUS, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
        {
                perror("Could not set CPU Affinity");
                exit(-3);
        }

        /*********************************************************************/

        INIT_TASK(argc, argv);

        /*********************************************************************/

        pthread_attr_init(&attr_1);
        pthread_attr_getschedparam(&attr_1, &param_1);
        param_1.sched_priority = TASK_PRIORITY;
        pthread_attr_setschedpolicy(&attr_1, SCHED_RR);
        pthread_attr_setschedparam(&attr_1, &param_1);

        (void)pthread_create(&thread_1, &attr_1, (void*)READ_TASK, (void*)NULL);
        pthread_setschedparam(thread_1, SCHED_RR, &param_1);

        /*********************************************************************/

        pthread_attr_init(&attr_2);
        pthread_attr_getschedparam(&attr_2, &param_2);
        param_2.sched_priority = TASK_PRIORITY;
        pthread_attr_setschedpolicy(&attr_2, SCHED_RR);
        pthread_attr_setschedparam(&attr_2, &param_2);

        (void)pthread_create(&thread_2, &attr_2, (void*)STORE_TASK, (void*)NULL);
        pthread_setschedparam(thread_2, SCHED_RR, &param_2);

        /*********************************************************************/

        pthread_join(thread_1, NULL);
        pthread_join(thread_2, NULL);

        EXIT_TASK();

        /*********************************************************************/

        exit(0);
}
