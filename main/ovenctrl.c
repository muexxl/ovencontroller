#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch.h"


void app_main(void)
{
    ESP_LOGI("OVENCTRL", "Oven control application started.");
    // Application logic goes here
    display_init();
    // Initialize touch controller
    ESP_ERROR_CHECK(cst816_init());
    
    // Create touch reading task
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);

    display_demo();

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
