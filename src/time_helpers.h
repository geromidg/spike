/**
  * @file time_helpers.h
  * @brief Contains the declarations of functions defined in time_helpers.c.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (geromidg@gmail.com)
  * @date August, 2017
  */

#ifndef TIME_HELPERS_H
#define TIME_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************** Inclusions *********************************/

#include <time.h>

#include "data_types.h"

/***************************** Macro Definitions *****************************/

/** The number of nsecs per sec. */
#define NSEC_PER_SEC (1000000000ul)

/***************************** Public Functions ******************************/

/**
 * @brief Update the tasks's timer with a new interval.
 * @param task_timer The task's timer to be updated.
 * @param interval The interval needed to perform the update.
 * @return Void.
 */
void updateInterval(struct timespec* task_timer, u64_t interval);

/**
 * @brief Get the current time.
 * @return The current time.
 */
f32_t getCurrentTimestamp(void);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* TIME_HELPERS_H */