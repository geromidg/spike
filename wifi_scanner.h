/**
  * @file wifi_scanner.h
  * @brief Contains the declarations of functions defined in wifi_scanner.c.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (geromidg@gmail.com)
  * @date August, 2017
  */

#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************** Inclusions *********************************/

#include <pthread.h>

#include "data_types.h"

/***************************** Macro Definitions *****************************/

/** The max size of the SSID. */
#define SSID_SIZE (64u)

/** The size of the SSID buffer. */
#define BUFFER_SIZE (32u)

/***************************** Type Definitions ******************************/

/** The SSID queue for the read/store (producer/consumer) model. */
struct SSIDQueue {
  char ssid_buffer[BUFFER_SIZE][SSID_SIZE];
  f32_t timestamp_buffer[BUFFER_SIZE];

  u32_t head, tail;
  u8_t full, empty;

  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
};

/***************************** Public Functions ******************************/

/**
* @brief Initialize the module.
* @return Void.
*/
void initializeWifiScanner(void);

/**
* @brief Exit the module and clean up.
* @return Void.
*/
void exitWifiScanner(void);

/**
* @brief Run a shell script to read and store the SSIDs to a buffer.
* @return Void.
*/
void readSSID(void);

/**
* @brief Store locally the SSIDs and timestamp from the buffers.
* @return Void.
*/
void storeSSIDs(void);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  /* WIFI_SCANNER_H */