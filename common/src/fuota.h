#ifndef __FUOTA_H__
#define __FUOTA_H__

#include <zephyr/lorawan/lorawan.h>

/**
 * Wait for the datarate to meet or exceed the specified rate
 */
void wait_for_datarate(enum lorawan_datarate dr);

/**
 * Read the reset reason and current version and uplink
 */
void report_version(void);

int fuota_run(void);

#endif /* __FUOTA_H__ */