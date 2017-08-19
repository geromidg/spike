/**
  * @file main.c
  * @brief Contains the scheduler and the entry point for
  *        the implementation of WifiScanner on BCM2837.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (geromidg@gmail.com)
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
#include "time_helpers.h"
#include "wifi_scanner.h"

/***************************** Macro Definitions *****************************/

/** The CPU affinity of the process. */
#define NUM_CPUS (0u)

/** The priority that will be given to the created tasks (threads) from the OS.
  * Since the PRREMPT_RT uses 50 as the priority of kernel tasklets and
  * interrupt handlers by default, the maximum available priority is chosen.
  * The priority of each task should be the same, since the Round-Robin
  * scheduling policy is used and each task is executed with the same time
  * slice.
  */
#define TASK_PRIORITY (49u)

/** This is the maximum size of the stack which
  * is guaranteed safe access without faulting.
  */
#define MAX_SAFE_STACK (128u * 1024u)

/***************************** Static Variables ******************************/

/** The cycle time between the task calls. */
static u64_t read_cycle_time;

/** The timers of the tasks. */
static struct timespec task_timer;

/******************** Static General Function Prototypes *********************/

/**
 * @brief Prefault the stack segment that belongs to this process.
 * @return Void.
 */
static void prefaultStack(void);

/********************* Static Task Function Prototypes *********************/

/**
 * @brief The initial task is run before the threads are created.
 * @return Void.
 */
static void INIT_TASK(int argc, char** argv);

/**
 * @brief The read task scans for wifi.
 * @return Void.
 */
static void* READ_TASK(void* ptr);

/**
 * @brief The store task stores scanned data to a file.
 * @return Void.
 */
static void* STORE_TASK(void* ptr);

/**
 * @brief The exit task is run after the threads are joined.
 * @return Void.
 */
static void EXIT_TASK(void);

/************************** Static General Functions *************************/

void prefaultStack(void)
{
  unsigned char dummy[MAX_SAFE_STACK];

  memset(dummy, 0, MAX_SAFE_STACK);

  return;
}

/************************** Static Task Functions ****************************/

void INIT_TASK(int argc, char** argv)
{
  if (argc != 2)
  {
    perror("Wrong number of arguments");
    exit(-4);
  }

  read_cycle_time = strtoul(argv[1], NULL, 0) * NSEC_PER_SEC;

  initializeWifiScanner();
}

void* READ_TASK(void* ptr)
{
  /* Synchronize tasks's timer. */
  clock_gettime(CLOCK_MONOTONIC, &task_timer);

  while(1)
  {
    /* Calculate next shot */
    updateInterval(&task_timer, read_cycle_time);

    readSSID();

    /* Sleep for the remaining duration */
    (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task_timer, NULL);
  }

  return (void*)NULL;
}

void* STORE_TASK(void* ptr)
{
  while(1)
  {
    storeSSIDs();
  }

  return (void*)NULL;
}

void EXIT_TASK(void)
{
  exitWifiScanner();
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

  /***********************************/

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

  /***********************************/

  INIT_TASK(argc, argv);

  /***********************************/

  pthread_attr_init(&attr_1);
  pthread_attr_getschedparam(&attr_1, &param_1);
  param_1.sched_priority = TASK_PRIORITY;
  pthread_attr_setschedpolicy(&attr_1, SCHED_RR);
  pthread_attr_setschedparam(&attr_1, &param_1);

  (void)pthread_create(&thread_1, &attr_1, (void*)READ_TASK, (void*)NULL);
  pthread_setschedparam(thread_1, SCHED_RR, &param_1);

  /***********************************/

  pthread_attr_init(&attr_2);
  pthread_attr_getschedparam(&attr_2, &param_2);
  param_2.sched_priority = TASK_PRIORITY;
  pthread_attr_setschedpolicy(&attr_2, SCHED_RR);
  pthread_attr_setschedparam(&attr_2, &param_2);

  (void)pthread_create(&thread_2, &attr_2, (void*)STORE_TASK, (void*)NULL);
  pthread_setschedparam(thread_2, SCHED_RR, &param_2);

  /***********************************/

  pthread_join(thread_1, NULL);
  pthread_join(thread_2, NULL);

  EXIT_TASK();

  /***********************************/

  exit(0);
}
