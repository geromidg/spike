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

/** The number of nsecs per sec/msec. */
#define NSEC_PER_SEC (1000000000u)
#define NSEC_PER_MSEC (1000000u)

/** The max size of the SSID. */
#define SSID_SIZE (32u)

/***************************** Static Variables ******************************/

/** The cycle time between the task calls. */
static u64_t cycle_time;

/** The saved ssids and their timestamp. */
static char** ssids;
static u32_t* num_timestamps;
static f32_t** timestamps;

/** The main timer of the scheduler. */
static struct timespec main_task_timer;

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
static void updateInterval(u64_t interval);

/**
  * @brief Get the current time.
  * @return The current time.
  */
f32_t getTimestamp(void);

/********************* Static Task Function Prototypes ***********************/

static void INIT_TASK(int argc, char** argv);
static void EXIT_TASK(void);

static void* MAIN_TASK(void* ptr);

/************************** Static General Functions *************************/

void prefaultStack(void)
{
        unsigned char dummy[MAX_SAFE_STACK];

        memset(dummy, 0, MAX_SAFE_STACK);

        return;
}

void updateInterval(u64_t interval)
{
        main_task_timer.tv_nsec += interval;

        /* Normalize time (when nsec have overflowed) */
        while (main_task_timer.tv_nsec >= NSEC_PER_SEC)
        {
                main_task_timer.tv_nsec -= NSEC_PER_SEC;
                main_task_timer.tv_sec++;
        }
}


f32_t getTimestamp(void)
{
        struct timespec current_t;

        clock_gettime(CLOCK_MONOTONIC, &current_t);

        return current_t.tv_sec + (current_t.tv_nsec / (f32_t)1000000000u);
}

/*****************************************************************************/

void INIT_TASK(int argc, char** argv)
{
        if (argc != 2)
        {
                perror("Wrong number of arguments");
                exit(-4);
        }

        cycle_time = atoi(argv[1]) * NSEC_PER_MSEC;
}

void* MAIN_TASK(void* ptr)
{
        u64_t i;
        u64_t size = 0;
        char ssid[SSID_SIZE];
        f32_t timestamp;
        u8_t ssid_found;

        /* Synchronize scheduler's timer. */
        clock_gettime(CLOCK_MONOTONIC, &main_task_timer);

        while(1)
        {                
                /* Calculate next shot */
                updateInterval(cycle_time);

                timestamp = getTimestamp();

                // TODO: Store script result first in an (array) buffer and then read!
                FILE *script = popen("/bin/bash searchWifi.sh", "r");             
                while (fgets(ssid, sizeof(ssid) - 1, script) != NULL)
                {
                        ssid_found = 0;

                        for (i = 0; i < size; i++)
                        {
                                if (!strcmp(ssid, ssids[i]))  /* equal */
                                {
                                        // timestamps = realloc (timestamps, sizeof(f32_t*));

                                        ssid_found = 1;
                                        break;
                                }
                        }

                        if (!ssid_found)
                        {
                                size++;

                                ssids = realloc (ssids, sizeof(char*) * SSID_SIZE * size);
                                strcpy(ssids[size - 1], ssid);

                                num_timestamps = realloc (num_timestamps, sizeof(u32_t) * size);
                                num_timestamps[size - 1] = 1;

                                // timestamps = realloc (timestamps, sizeof(f32_t*));
                        }
                }
                pclose(script);

                FILE *file = fopen("ssids.txt", "w");
                for (i = 0; i < size; i++)
                {
                        fprintf(file, "%s\n", ssids[i]);
                        fprintf(file, "%u\n", num_timestamps[i]);
                }
                fclose(file);

                /* Sleep for the remaining duration */
                (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &main_task_timer, NULL);
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
        struct sched_param parm_1;

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
        pthread_attr_getschedparam(&attr_1, &parm_1);
        parm_1.sched_priority = TASK_PRIORITY;
        pthread_attr_setschedpolicy(&attr_1, SCHED_RR);
        pthread_attr_setschedparam(&attr_1, &parm_1);

        (void)pthread_create(&thread_1, &attr_1, (void*)MAIN_TASK, (void*)NULL);
        pthread_setschedparam(thread_1, SCHED_RR, &parm_1);

        /*********************************************************************/

        pthread_join(thread_1, NULL);

        EXIT_TASK();

        /*********************************************************************/

        exit(0);
}
