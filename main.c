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
#define NSEC_PER_SEC (1000000000u)

/** The max size of the SSID. */
#define SSID_SIZE (64u)

/** The cycle time ratio between the two tasks. */
#define READ_STORE_CYCLE_RATIO (0.1f)  /* Store task run 10 times slower than Read */

/***************************** Static Variables ******************************/

/** The cycle time between the task calls. */
static u64_t read_cycle_time;
static u64_t store_cycle_time;

/** The saved ssids and their timestamp. */
static u64_t ssid_num = 0;
static char** ssids;
static char ssid_buffer[32][SSID_SIZE];
static u8_t buffer_index;
static u32_t* num_timestamps;
static f32_t** timestamps;

/** The timers of the tasks. */
static struct timespec read_task_timer;
static struct timespec store_task_timer;

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
static void EXIT_TASK(void);

static void* READ_TASK(void* ptr);
static void* STORE_TASK(void* ptr);

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

void copySSIDsToBuffer(void)
{
        char ssid[SSID_SIZE];

        FILE *file = popen("/bin/bash searchWifi.sh", "r");

        buffer_index = 0;

        if (file != NULL)
        {

                while (fgets(ssid, sizeof(ssid) - 1, file) != NULL)
                {
                        if (strncmp(ssid, "x00", 3))  /* skip if SSID is x00* */
                        {
                                strcpy(ssid_buffer[buffer_index++], ssid);
                        }
                }

                pclose(file);
        }
}

void storeSSIDs(float timestamp)
{              
        u64_t i, j;
        u8_t ssid_found;

        for (i = 0; i < buffer_index; i++)
        {
                ssid_found = 0;

                for (j = 0; j < ssid_num; j++)
                {
                        if (!strcmp(ssids[j], ssid_buffer[i]) &&  /* equal and non-existent */
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
                        strcpy(ssids[ssid_num - 1], ssid_buffer[i]);

                        num_timestamps = realloc(num_timestamps, sizeof(u32_t) * ssid_num);
                        num_timestamps[ssid_num - 1] = 1;

                        timestamps = realloc(timestamps, sizeof(f32_t*) * ssid_num);
                        timestamps[ssid_num - 1] = malloc(sizeof(f32_t));
                        timestamps[ssid_num - 1][0] = timestamp;
                }
        }
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
                        {
                                fprintf(file, "    %.5f\n", timestamps[i][j]);
                        }
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

        read_cycle_time = atoi(argv[1]) * NSEC_PER_SEC;
        store_cycle_time = (u64_t)(read_cycle_time * READ_STORE_CYCLE_RATIO);
}

void* READ_TASK(void* ptr)
{
        /* Synchronize scheduler's timer. */
        clock_gettime(CLOCK_MONOTONIC, &read_task_timer);

        while(1)
        {
                /* Calculate next shot */
                updateInterval(&read_task_timer, read_cycle_time);

                // TODO: Add timestamp to buffer!
                copySSIDsToBuffer();

                /* Sleep for the remaining duration */
                (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &read_task_timer, NULL);
        }

        return (void*)NULL;
}

void* STORE_TASK(void* ptr)
{
        f32_t timestamp;

        /* Synchronize scheduler's timer. */
        clock_gettime(CLOCK_MONOTONIC, &store_task_timer);

        while(1)
        {
                /* Calculate next shot */
                updateInterval(&store_task_timer, store_cycle_time);

                timestamp = getTimestamp();
                storeSSIDs(timestamp);

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
