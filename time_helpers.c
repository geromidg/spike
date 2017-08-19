/**
  * @file time_helpers.c
  * @brief Implements various time-related helper functions.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (geromidg@gmail.com)
  * @date August, 2017
  */

/******************************** Inclusions *********************************/

#include "time_helpers.h"

/***************************** Public Functions ******************************/

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

f32_t getCurrentTimestamp(void)
{
  struct timespec current_t;

  clock_gettime(CLOCK_MONOTONIC, &current_t);

  return current_t.tv_sec + (current_t.tv_nsec / (f32_t)1000000000u);
}