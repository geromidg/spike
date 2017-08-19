/**
  * @file wifi_scanner.c
  * @brief Implements a module to scan the wifi and output the data to a file.
  *
  * @author Dimitrios Panagiotis G. Geromichalos (geromidg@gmail.com)
  * @date August, 2017
  */

/******************************** Inclusions *********************************/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include "time_helpers.h"

#include "wifi_scanner.h"

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

/***************************** Static Variables ******************************/

/** The saved ssids and their timestamp. */
static u64_t ssid_num = 0;
static char** ssids;
static u32_t* num_timestamps;
static f32_t** timestamps;
static f32_t** latencies;

/** The queue to be used by the two tasks. */
static struct SSIDQueue ssid_queue;

/************************ Static Function Prototypes *************************/

/**
 * @brief Add a new SSID and timestamp to the queue.
 * @param ssid The SSID to be added to the queue.
 * @param timestamp The timestamp that corresponds to the SSID.
 * @return Void.
 */
static void queueAdd(char* ssid, f32_t timestamp);

/**
 * @brief Pop an SSID and timestamp from the queue.
 * @param ssid The SSID to be popped from the queue.
 * @param timestamp The timestamp that corresponds to the SSID.
 * @return Void.
 */
static void queuePop(char* ssid, f32_t* timestamp);

/**
 * @brief Write SSIDs and their timestamps to a file.
 * @return Void.
 */
static void writeToFile(void);

/***************************** Static Functions ******************************/

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

void writeToFile(void)
{
  u64_t i, j;

  FILE *file = fopen("ssids.txt", "w");

  if (file != NULL)
  {
    fprintf(file, "SSID\n");
    fprintf(file, "    timestamp  (latency)\n");
    fprintf(file, "=========================\n\n");

    for (i = 0; i < ssid_num; i++)
    {
      fprintf(file, "%s", ssids[i]);

      for (j = 0; j < num_timestamps[i]; j++)
      {
        fprintf(file, "    %.3f", timestamps[i][j]);
        fprintf(file, "   (%.6f)\n", latencies[i][j]);
      }

      fprintf(file, "\n");
    }

    fclose(file);
  }
}

/***************************** Public Functions ******************************/

void initializeWifiScanner(void)
{
  ssid_queue.empty = 1;
  ssid_queue.full = 0;
  ssid_queue.head = 0;
  ssid_queue.tail = 0;
  pthread_mutex_init(&ssid_queue.mutex, NULL);
  pthread_cond_init(&ssid_queue.not_empty, NULL);
  pthread_cond_init(&ssid_queue.not_full, NULL);
}

void exitWifiScanner(void)
{
  free(ssids);
  free(num_timestamps);
  free(timestamps);

  pthread_mutex_destroy(&ssid_queue.mutex);
  pthread_cond_destroy(&ssid_queue.not_empty);
  pthread_cond_destroy(&ssid_queue.not_full);
}

void readSSID(void)
{
  char ssid[SSID_SIZE];

  FILE *file = popen("/bin/bash searchWifi.sh", "r");

  pthread_mutex_lock(&ssid_queue.mutex);
  while (ssid_queue.full)
    pthread_cond_wait(&ssid_queue.not_full, &ssid_queue.mutex);

  if (file != NULL)
  {
    while (fgets(ssid, sizeof(ssid) - 1, file) != NULL)
    {
      /* skip if SSID is x00* */
      if (!ssid_queue.full && strncmp(ssid, "x00", 3))
      {
        queueAdd(ssid, getCurrentTimestamp());
      }
    }

    pclose(file);
  }

  pthread_mutex_unlock(&ssid_queue.mutex);
  pthread_cond_signal(&ssid_queue.not_empty);
}

void storeSSIDs(void)
{
  u64_t i;
  u8_t ssid_found;
  f32_t timestamp;
  char ssid[SSID_SIZE];

  pthread_mutex_lock(&ssid_queue.mutex);
  while (ssid_queue.empty)
    pthread_cond_wait(&ssid_queue.not_empty, &ssid_queue.mutex);

  queuePop(ssid, &timestamp);

  ssid_found = 0;

  for (i = 0; i < ssid_num; i++)
  {
    if (!strcmp(ssids[i], ssid) &&  /* equal */
        timestamps[i][num_timestamps[i] - 1] != timestamp)
    {
      num_timestamps[i]++;

      timestamps[i] = realloc(timestamps[i],
        sizeof(f32_t) * num_timestamps[i]);
      timestamps[i][num_timestamps[i] - 1] = timestamp;

      latencies[i] = realloc(latencies[i], sizeof(f32_t) * num_timestamps[i]);
      latencies[i][num_timestamps[i] - 1] = getCurrentTimestamp() - timestamp;

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

    latencies = realloc(latencies, sizeof(f32_t*) * ssid_num);
    latencies[ssid_num - 1] = malloc(sizeof(f32_t));
    latencies[ssid_num - 1][0] = getCurrentTimestamp() - timestamp;
  }

  pthread_mutex_unlock(&ssid_queue.mutex);
  pthread_cond_signal(&ssid_queue.not_full);

  writeToFile();
}