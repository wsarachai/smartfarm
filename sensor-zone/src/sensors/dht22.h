#ifndef DHT22_H_
#define DHT22_H_

#include <stdbool.h>

bool dht22_read(float *humidity, float *temperature);

#endif // DHT22_H_
