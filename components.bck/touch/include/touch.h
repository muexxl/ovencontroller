#ifndef TOUCH_H
#define TOUCH_H

#include "esp_err.h"

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t points;
} touch_data_t;

esp_err_t cst816_init(void);
esp_err_t cst816_read_touch(touch_data_t *touch);
const char* cst816_gesture_name(uint8_t gesture);
void touch_task(void *pvParameters);

#endif // TOUCH_H