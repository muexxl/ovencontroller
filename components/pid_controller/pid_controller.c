#include "pid_controller.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "PID";
static const char *NVS_NAMESPACE = "pid_calib";
static const char *NVS_KEY = "cal_data";

#define CALIB_MAGIC 0xCAFEBABE

// Global state
static pid_state_t g_pid_state = {0};
static pid_params_t g_pid_params = {0};
static calib_context_t g_calib_ctx = {0};
static calibration_data_t g_cal_data = {0};
static bool g_is_calibrated = false;

// PWM state
static float g_pwm_duty = 0.0f;
static uint32_t g_pwm_cycle_start = 0;

// Forward declarations
static void analyze_step_response(void);
static void calculate_pid_params(void);
static uint32_t calculate_crc32(const uint8_t *data, size_t length);

// ============================================================================
// NVS Storage
// ============================================================================

esp_err_t pid_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing PID controller");
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // Initialize PID state
    g_pid_state.enabled = false;
    g_pid_state.setpoint = 100.0f;
    g_pid_state.output = 0.0f;
    g_pid_state.integral = 0.0f;
    g_pid_state.prev_error = 0.0f;
    
    // Initialize calibration context
    g_calib_ctx.state = CALIB_IDLE;
    g_calib_ctx.temp_samples = NULL;
    
    // Try to load calibration
    ret = pid_load_calibration();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded calibration from NVS");
        g_is_calibrated = true;
    } else {
        ESP_LOGW(TAG, "No valid calibration found");
        g_is_calibrated = false;
    }
    
    return ESP_OK;
}

esp_err_t pid_save_calibration(const calibration_data_t *cal_data)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, NVS_KEY, cal_data, sizeof(calibration_data_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration saved to NVS");
    }
    
    return ret;
}

esp_err_t pid_load_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t required_size = sizeof(calibration_data_t);
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_get_blob(nvs_handle, NVS_KEY, &g_cal_data, &required_size);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Verify magic and CRC
    if (g_cal_data.magic != CALIB_MAGIC) {
        ESP_LOGE(TAG, "Invalid calibration magic");
        return ESP_ERR_INVALID_CRC;
    }
    
    uint32_t calc_crc = calculate_crc32((uint8_t*)&g_cal_data, 
                                        sizeof(calibration_data_t) - sizeof(uint32_t));
    if (calc_crc != g_cal_data.crc32) {
        ESP_LOGE(TAG, "Calibration CRC mismatch");
        return ESP_ERR_INVALID_CRC;
    }
    
    // Load PID parameters
    memcpy(&g_pid_params, &g_cal_data.pid, sizeof(pid_params_t));
    
    ESP_LOGI(TAG, "Calibration loaded: Kp=%.2f, Ki=%.2f, Kd=%.2f",
             g_pid_params.Kp, g_pid_params.Ki, g_pid_params.Kd);
    
    return ESP_OK;
}

bool pid_is_calibrated(void)
{
    return g_is_calibrated;
}

// ============================================================================
// Calibration
// ============================================================================

esp_err_t pid_start_calibration(void)
{
    if (g_calib_ctx.state != CALIB_IDLE) {
        ESP_LOGW(TAG, "Calibration already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting auto-calibration");
    
    // Allocate sample buffer
    g_calib_ctx.temp_samples = malloc(CALIB_SAMPLES * sizeof(float));
    if (!g_calib_ctx.temp_samples) {
        ESP_LOGE(TAG, "Failed to allocate sample buffer");
        return ESP_ERR_NO_MEM;
    }
    
    memset(g_calib_ctx.temp_samples, 0, CALIB_SAMPLES * sizeof(float));
    g_calib_ctx.sample_count = 0;
    g_calib_ctx.sample_index = 0;
    g_calib_ctx.start_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    g_calib_ctx.steady_start_time = 0;
    g_calib_ctx.progress_pct = 0;
    g_calib_ctx.state = CALIB_INIT;
    
    return ESP_OK;
}

esp_err_t pid_stop_calibration(void)
{
    if (g_calib_ctx.state == CALIB_IDLE) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping calibration");
    
    if (g_calib_ctx.temp_samples) {
        free(g_calib_ctx.temp_samples);
        g_calib_ctx.temp_samples = NULL;
    }
    
    g_calib_ctx.state = CALIB_CANCELLED;
    pwm_set_duty(0.0f);
    
    return ESP_OK;
}

calib_state_t pid_get_calib_state(void)
{
    return g_calib_ctx.state;
}

int pid_get_calib_progress(void)
{
    return g_calib_ctx.progress_pct;
}

const char* pid_get_calib_status(void)
{
    switch (g_calib_ctx.state) {
        case CALIB_IDLE: return "Idle";
        case CALIB_INIT: return "Initializing";
        case CALIB_CHECK_TEMP: return "Checking temperature";
        case CALIB_WARMUP: return "Warming up";
        case CALIB_SETTLE: return "Settling";
        case CALIB_STEP_TEST: return "Running step test";
        case CALIB_ANALYZE: return "Analyzing data";
        case CALIB_CALCULATE: return "Calculating PID";
        case CALIB_COMPLETE: return "Complete";
        case CALIB_ERROR: return g_calib_ctx.error_msg;
        case CALIB_CANCELLED: return "Cancelled";
        default: return "Unknown";
    }
}

esp_err_t pid_get_calib_results(system_model_t *model, pid_params_t *pid)
{
    if (g_calib_ctx.state != CALIB_COMPLETE) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (model) {
        memcpy(model, &g_cal_data.model, sizeof(system_model_t));
    }
    if (pid) {
        memcpy(pid, &g_cal_data.pid, sizeof(pid_params_t));
    }
    
    return ESP_OK;
}

// ============================================================================
// Calibration State Machine (called periodically with current temp)
// ============================================================================

void pid_calibration_update(float current_temp)
{
    static uint32_t last_update = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    
    if (g_calib_ctx.state == CALIB_IDLE || g_calib_ctx.state == CALIB_COMPLETE) {
        return;
    }
    
    // Check timeout
    if (now - g_calib_ctx.start_time > CALIB_TIMEOUT_SEC) {
        ESP_LOGE(TAG, "Calibration timeout");
        snprintf(g_calib_ctx.error_msg, sizeof(g_calib_ctx.error_msg), "Timeout");
        g_calib_ctx.state = CALIB_ERROR;
        pwm_set_duty(0.0f);
        return;
    }
    
    switch (g_calib_ctx.state) {
        case CALIB_INIT:
            ESP_LOGI(TAG, "Calibration initialized");
            g_calib_ctx.state = CALIB_CHECK_TEMP;
            g_calib_ctx.progress_pct = 5;
            break;
            
        case CALIB_CHECK_TEMP:
            if (current_temp > CALIB_AMBIENT_MAX) {
                ESP_LOGE(TAG, "Temperature too high to start: %.1f°C", current_temp);
                snprintf(g_calib_ctx.error_msg, sizeof(g_calib_ctx.error_msg), 
                         "Temp too high: %.1f C", current_temp);
                g_calib_ctx.state = CALIB_ERROR;
                break;
            }
            ESP_LOGI(TAG, "Temperature OK: %.1f°C", current_temp);
            g_calib_ctx.state = CALIB_WARMUP;
            g_calib_ctx.progress_pct = 10;
            pwm_set_duty(CALIB_TEST_POWER);
            break;
            
        case CALIB_WARMUP:
            // Wait until temp starts rising
            if (current_temp > g_calib_ctx.last_temp + 2.0f) {
                ESP_LOGI(TAG, "Temperature rising, entering settle phase");
                g_calib_ctx.state = CALIB_SETTLE;
                g_calib_ctx.steady_start_time = now;
                g_calib_ctx.progress_pct = 20;
            }
            g_calib_ctx.last_temp = current_temp;
            break;
            
        case CALIB_SETTLE:
            // Wait for 30 seconds of stability before starting data collection
            if (now - g_calib_ctx.steady_start_time > CALIB_STEADY_TIME) {
                ESP_LOGI(TAG, "System settled, starting step test");
                g_calib_ctx.state = CALIB_STEP_TEST;
                g_calib_ctx.sample_index = 0;
                g_calib_ctx.sample_count = 0;
                last_update = now;
                g_calib_ctx.progress_pct = 30;
            }
            break;
            
        case CALIB_STEP_TEST:
            // Sample every second
            if (now - last_update >= 1) {
                if (g_calib_ctx.sample_index < CALIB_SAMPLES) {
                    g_calib_ctx.temp_samples[g_calib_ctx.sample_index++] = current_temp;
                    g_calib_ctx.sample_count++;
                    last_update = now;
                    
                    // Update progress (30% to 80%)
                    g_calib_ctx.progress_pct = 30 + (50 * g_calib_ctx.sample_index / CALIB_SAMPLES);
                    
                    // Check if steady-state reached (last 30 samples within threshold)
                    if (g_calib_ctx.sample_index > 60) {
                        bool steady = true;
                        float avg = 0;
                        for (int i = g_calib_ctx.sample_index - 30; i < g_calib_ctx.sample_index; i++) {
                            avg += g_calib_ctx.temp_samples[i];
                        }
                        avg /= 30.0f;
                        
                        for (int i = g_calib_ctx.sample_index - 30; i < g_calib_ctx.sample_index; i++) {
                            if (fabsf(g_calib_ctx.temp_samples[i] - avg) > CALIB_STEADY_THRESH) {
                                steady = false;
                                break;
                            }
                        }
                        
                        if (steady) {
                            ESP_LOGI(TAG, "Steady state reached at sample %d", g_calib_ctx.sample_index);
                            g_calib_ctx.state = CALIB_ANALYZE;
                            pwm_set_duty(0.0f);
                            g_calib_ctx.progress_pct = 85;
                        }
                    }
                } else {
                    // Max samples reached
                    g_calib_ctx.state = CALIB_ANALYZE;
                    pwm_set_duty(0.0f);
                    g_calib_ctx.progress_pct = 85;
                }
            }
            break;
            
        case CALIB_ANALYZE:
            ESP_LOGI(TAG, "Analyzing %d samples", g_calib_ctx.sample_count);
            analyze_step_response();
            g_calib_ctx.state = CALIB_CALCULATE;
            g_calib_ctx.progress_pct = 90;
            break;
            
        case CALIB_CALCULATE:
            ESP_LOGI(TAG, "Calculating PID parameters");
            calculate_pid_params();
            
            // Save calibration
            g_cal_data.magic = CALIB_MAGIC;
            g_cal_data.timestamp = now;
            g_cal_data.crc32 = calculate_crc32((uint8_t*)&g_cal_data, 
                                               sizeof(calibration_data_t) - sizeof(uint32_t));
            
            if (pid_save_calibration(&g_cal_data) == ESP_OK) {
                g_is_calibrated = true;
                memcpy(&g_pid_params, &g_cal_data.pid, sizeof(pid_params_t));
            }
            
            g_calib_ctx.state = CALIB_COMPLETE;
            g_calib_ctx.progress_pct = 100;
            
            // Free sample buffer
            if (g_calib_ctx.temp_samples) {
                free(g_calib_ctx.temp_samples);
                g_calib_ctx.temp_samples = NULL;
            }
            
            ESP_LOGI(TAG, "Calibration complete!");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// Analysis Functions
// ============================================================================

static void analyze_step_response(void)
{
    int n = g_calib_ctx.sample_count;
    float *temps = g_calib_ctx.temp_samples;
    
    // Calculate initial temperature (average of first 10 samples)
    float T0 = 0;
    for (int i = 0; i < 10 && i < n; i++) {
        T0 += temps[i];
    }
    T0 /= 10.0f;
    
    // Calculate steady-state temperature (average of last 30 samples)
    float Tss = 0;
    int start = (n > 30) ? (n - 30) : 0;
    for (int i = start; i < n; i++) {
        Tss += temps[i];
    }
    Tss /= (float)(n - start);
    
    // Calculate process gain
    float delta_T = Tss - T0;
    float K = delta_T / CALIB_TEST_POWER;  // °C per 1% power
    
    // Find dead time (first significant rise)
    int L_samples = 0;
    for (int i = 0; i < n; i++) {
        if (temps[i] > T0 + 1.0f) {
            L_samples = i;
            break;
        }
    }
    float L = (float)L_samples;  // seconds
    
    // Find time constant (63.2% of final value)
    float target_63 = T0 + 0.632f * delta_T;
    int tau_samples = 0;
    for (int i = L_samples; i < n; i++) {
        if (temps[i] >= target_63) {
            tau_samples = i - L_samples;
            break;
        }
    }
    float tau = (float)tau_samples;  // seconds
    
    // Store results
    g_cal_data.model.K = K;
    g_cal_data.model.tau = tau;
    g_cal_data.model.L = L;
    g_cal_data.model.T0 = T0;
    g_cal_data.model.Tss = Tss;
    
    ESP_LOGI(TAG, "System Model: K=%.3f, tau=%.1f, L=%.1f, T0=%.1f, Tss=%.1f",
             K, tau, L, T0, Tss);
}

static void calculate_pid_params(void)
{
    system_model_t *m = &g_cal_data.model;
    pid_params_t *p = &g_cal_data.pid;
    
    // Ziegler-Nichols tuning rules
    if (m->L > 0 && m->K > 0) {
        p->Kp = 1.2f * (m->tau / (m->K * m->L));
        p->Ki = p->Kp / (2.0f * m->L);
        p->Kd = p->Kp * (0.5f * m->L);
        
        // Apply safety limits
        if (p->Kp > 100.0f) p->Kp = 100.0f;
        if (p->Kp < 0.1f) p->Kp = 0.1f;
        if (p->Ki > 10.0f) p->Ki = 10.0f;
        if (p->Ki < 0.01f) p->Ki = 0.01f;
        if (p->Kd > 50.0f) p->Kd = 50.0f;
        if (p->Kd < 0.0f) p->Kd = 0.0f;
    } else {
        // Fallback values
        p->Kp = 5.0f;
        p->Ki = 0.2f;
        p->Kd = 10.0f;
    }
    
    ESP_LOGI(TAG, "PID Parameters: Kp=%.2f, Ki=%.2f, Kd=%.2f", p->Kp, p->Ki, p->Kd);
}

// ============================================================================
// PID Control
// ============================================================================

esp_err_t pid_set_params(const pid_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;
    memcpy(&g_pid_params, params, sizeof(pid_params_t));
    ESP_LOGI(TAG, "PID params updated: Kp=%.2f, Ki=%.2f, Kd=%.2f",
             params->Kp, params->Ki, params->Kd);
    return ESP_OK;
}

esp_err_t pid_get_params(pid_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;
    memcpy(params, &g_pid_params, sizeof(pid_params_t));
    return ESP_OK;
}

void pid_enable(bool enable)
{
    g_pid_state.enabled = enable;
    if (!enable) {
        pid_reset();
    }
    ESP_LOGI(TAG, "PID %s", enable ? "enabled" : "disabled");
}

void pid_set_setpoint(float setpoint)
{
    if (setpoint < PID_TEMP_MIN) setpoint = PID_TEMP_MIN;
    if (setpoint > PID_TEMP_MAX) setpoint = PID_TEMP_MAX;
    
    g_pid_state.setpoint = setpoint;
    ESP_LOGI(TAG, "Setpoint: %.1f°C", setpoint);
}

float pid_get_setpoint(void)
{
    return g_pid_state.setpoint;
}

float pid_compute(float current_temp, float dt)
{
    if (!g_pid_state.enabled || !g_is_calibrated) {
        g_pid_state.output = 0.0f;
        return 0.0f;
    }
    
    // Calculate error
    float error = g_pid_state.setpoint - current_temp;
    
    // Proportional term
    float P = g_pid_params.Kp * error;
    
    // Integral term (with anti-windup)
    g_pid_state.integral += error * dt;
    if (g_pid_state.integral > 100.0f) g_pid_state.integral = 100.0f;
    if (g_pid_state.integral < -100.0f) g_pid_state.integral = -100.0f;
    float I = g_pid_params.Ki * g_pid_state.integral;
    
    // Derivative term
    float derivative = (error - g_pid_state.prev_error) / dt;
    float D = g_pid_params.Kd * derivative;
    
    // Calculate output
    g_pid_state.output = P + I + D;
    
    // Clamp output to 0-100%
    if (g_pid_state.output > 100.0f) g_pid_state.output = 100.0f;
    if (g_pid_state.output < 0.0f) g_pid_state.output = 0.0f;
    
    g_pid_state.prev_error = error;
    
    return g_pid_state.output;
}

void pid_reset(void)
{
    g_pid_state.integral = 0.0f;
    g_pid_state.prev_error = 0.0f;
    g_pid_state.output = 0.0f;
}

// ============================================================================
// PWM Control (Time-Proportional)
// ============================================================================

void pwm_set_duty(float duty_pct)
{
    if (duty_pct < 0.0f) duty_pct = 0.0f;
    if (duty_pct > 100.0f) duty_pct = 100.0f;
    g_pwm_duty = duty_pct;
}

void pwm_update(void)
{
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (g_pwm_cycle_start == 0) {
        g_pwm_cycle_start = now_ms;
    }
    
    uint32_t elapsed = now_ms - g_pwm_cycle_start;
    
    // Reset cycle every PWM_PERIOD_MS
    if (elapsed >= PWM_PERIOD_MS) {
        g_pwm_cycle_start = now_ms;
        elapsed = 0;
    }
    
    // Calculate on-time
    uint32_t on_time = (uint32_t)((g_pwm_duty / 100.0f) * PWM_PERIOD_MS);
    
    // Return true if heater should be on
    // Note: Actual heater control is done externally
    // This just tracks the PWM state
}

float pwm_get_current_duty(void)
{
    return g_pwm_duty;
}

bool pwm_should_be_on(void)
{
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (g_pwm_cycle_start == 0) {
        g_pwm_cycle_start = now_ms;
    }
    
    uint32_t elapsed = now_ms - g_pwm_cycle_start;
    
    if (elapsed >= PWM_PERIOD_MS) {
        g_pwm_cycle_start = now_ms;
        elapsed = 0;
    }
    
    uint32_t on_time = (uint32_t)((g_pwm_duty / 100.0f) * PWM_PERIOD_MS);
    
    return (elapsed < on_time);
}

// ============================================================================
// Utility Functions
// ============================================================================

static uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return ~crc;
}