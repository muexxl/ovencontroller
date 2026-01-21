#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Operating range
#define PID_TEMP_MIN        50.0f
#define PID_TEMP_MAX        300.0f

// Calibration parameters
#define CALIB_MAX_TEMP      150.0f
#define CALIB_TIMEOUT_SEC   600
#define CALIB_AMBIENT_MAX   40.0f
#define CALIB_STEADY_THRESH 2.0f
#define CALIB_STEADY_TIME   30
#define CALIB_TEST_POWER    50.0f   // 50% power for step test
#define CALIB_SAMPLES       600     // 10 minutes at 1Hz

// PWM parameters
#define PWM_PERIOD_MS       10000   // 10 second period

// System model from step response
typedef struct {
    float K;        // Process gain (°C/% power)
    float tau;      // Time constant (seconds)
    float L;        // Dead time (seconds)
    float T0;       // Initial temperature
    float Tss;      // Steady-state temperature
} system_model_t;

// PID parameters
typedef struct {
    float Kp;       // Proportional gain
    float Ki;       // Integral gain
    float Kd;       // Derivative gain
} pid_params_t;

// Calibration data (stored in NVS)
typedef struct {
    uint32_t magic;             // 0xCAFEBABE
    system_model_t model;
    pid_params_t pid;
    uint32_t timestamp;
    uint32_t crc32;
} calibration_data_t;

// Calibration state
typedef enum {
    CALIB_IDLE = 0,
    CALIB_INIT,
    CALIB_CHECK_TEMP,
    CALIB_WARMUP,
    CALIB_SETTLE,
    CALIB_STEP_TEST,
    CALIB_ANALYZE,
    CALIB_CALCULATE,
    CALIB_COMPLETE,
    CALIB_ERROR,
    CALIB_CANCELLED
} calib_state_t;

// PID controller state
typedef struct {
    float setpoint;
    float output;
    float integral;
    float prev_error;
    uint32_t last_update_ms;
    bool enabled;
} pid_state_t;

// Calibration context
typedef struct {
    calib_state_t state;
    float *temp_samples;
    int sample_count;
    int sample_index;
    uint32_t start_time;
    uint32_t steady_start_time;
    float last_temp;
    char error_msg[64];
    int progress_pct;
} calib_context_t;

// Function declarations
esp_err_t pid_init(void);
esp_err_t pid_load_calibration(void);
esp_err_t pid_save_calibration(const calibration_data_t *cal_data);
bool pid_is_calibrated(void);

// Calibration control
esp_err_t pid_start_calibration(void);
esp_err_t pid_stop_calibration(void);
calib_state_t pid_get_calib_state(void);
int pid_get_calib_progress(void);
const char* pid_get_calib_status(void);
esp_err_t pid_get_calib_results(system_model_t *model, pid_params_t *pid);

// PID control
esp_err_t pid_set_params(const pid_params_t *params);
esp_err_t pid_get_params(pid_params_t *params);
void pid_enable(bool enable);
void pid_set_setpoint(float setpoint);
float pid_get_setpoint(void);
float pid_compute(float current_temp, float dt);
void pid_reset(void);

// PWM control (time-proportional)
void pwm_set_duty(float duty_pct);  // 0-100%
void pwm_update(void);
float pwm_get_current_duty(void);

#endif // PID_CONTROLLER_H