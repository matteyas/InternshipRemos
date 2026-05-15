#include "power_library.h"
#include "modbus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <pthread.h> // mutex for thread-safe modbus access
#include <locale.h>  // used to guarantee '.' as decimal separator for JSON parsing

// ### CONSTANTS
// HPAs
#define POWERLIB_HPA1     0
#define POWERLIB_HPA2     1
#define POWERLIB_MAX_HPAS 2

// Modbus default config
#define POWERLIB_DEFAULT_DEVICE_ID  1
#define POWERLIB_DEFAULT_BAUDRATE   9600
#define POWERLIB_DEFAULT_SERIAL_CFG "8N1"

// ### STRUCTS
typedef struct {
    double a0, a1, a2;
    double b0, b1, b2;
    double c0, c1, c2;
    double fmin, fmax;
    int loaded;
    int enabled;
} PowerLib_Calibration;

typedef struct {
    PowerLib_Calibration calibrations[POWERLIB_MAX_HPAS];
    int calibration_initialized;
    int modbus_initialized;
} PowerLib_Context;

// ### PRIVATE GLOBAL STATE
static PowerLib_Context g_system = {0};
static pthread_mutex_t g_modbus_mutex = PTHREAD_MUTEX_INITIALIZER;

// ### PRIVATE HELPER FUNCTIONS
static const char* find_str_bounded(const char* start, const char* end, const char* search) {
    if (!start || !end || !search) return NULL;
    size_t len = strlen(search);
    if (len == 0 || (end - start) < (ptrdiff_t)len) return NULL;
    
    for (const char* p = start; p <= end - len; p++) {
        if (strncmp(p, search, len) == 0) return p;
    }
    return NULL;
}

static const char* find_json_block(const char* json, const char* key, const char** out_end) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char* p = strstr(json, search);
    if (!p) return NULL;

    p = strchr(p, ':');
    if (!p) return NULL;

    p = strchr(p, '{');
    if (!p) return NULL;

    const char* block_start = p;
    int depth = 0;

    while (*p) {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) p += 2;
                else p++;
            }
            if (!*p) break;
        } else if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                if (out_end) *out_end = p;
                return block_start;
            }
        }
        p++;
    }
    return NULL;
}

static int parse_double_scoped(const char* start, const char* end, const char* key, double* out) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char* p = find_str_bounded(start, end, search);
    if (!p) return 0;

    p = find_str_bounded(p, end, ":");
    if (!p) return 0;

    p++; // move past ':'

    while (p < end &&
           (*p == ' '  ||
            *p == '\t' ||
            *p == '\n' ||
            *p == '\r'))
    {
        p++;
    }

    if (p >= end)
        return 0;

    char* parse_end = NULL;

    double val = strtod(p, &parse_end);

    if (parse_end == p)
        return 0;

    // skip trailing whitespace
    while (parse_end < end &&
           (*parse_end == ' '  ||
            *parse_end == '\t' ||
            *parse_end == '\n' ||
            *parse_end == '\r'))
    {
        parse_end++;
    }

    // only allow legal JSON terminators
    if (parse_end < end   &&
        *parse_end != ',' &&
        *parse_end != '}')
    {
        return 0;
    }

    *out = val;
    return 1;
}

static int parse_bool_scoped(const char* start, const char* end, const char* key, int* out) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char* p = find_str_bounded(start, end, search);
    if (!p) return 0;

    p = find_str_bounded(p, end, ":");
    if (!p) return 0;

    p++;

    while (p < end &&
           (*p == ' '  ||
            *p == '\t' ||
            *p == '\n' ||
            *p == '\r'))
    {
        p++;
    }

    if (p >= end)
        return 0;

    int valid_term(char c)
    {
        return c == ','  ||
               c == '}'  ||
               c == ' '  ||
               c == '\t' ||
               c == '\n' ||
               c == '\r';
    }

    if ((end - p) >= 4 &&
        strncmp(p, "true", 4) == 0)
    {
        const char* after = p + 4;

        if (after >= end || valid_term(*after)) {
            *out = 1;
            return 1;
        }
    }

    if ((end - p) >= 5 &&
        strncmp(p, "false", 5) == 0)
    {
        const char* after = p + 5;

        if (after >= end || valid_term(*after)) {
            *out = 0;
            return 1;
        }
    }

    return 0;
}

// Parse JSON file -> PowerLib_Calibration
static int parse_cal_block(const char* start, const char* end, PowerLib_Calibration* cal) {
    int ok = 1;
    memset(cal, 0, sizeof(PowerLib_Calibration));

    ok &= parse_bool_scoped  (start, end, "enabled", &cal->enabled);
    ok &= parse_double_scoped(start, end, "a0",      &cal->a0);
    ok &= parse_double_scoped(start, end, "a1",      &cal->a1);
    ok &= parse_double_scoped(start, end, "a2",      &cal->a2);
    ok &= parse_double_scoped(start, end, "b0",      &cal->b0);
    ok &= parse_double_scoped(start, end, "b1",      &cal->b1);
    ok &= parse_double_scoped(start, end, "b2",      &cal->b2);
    ok &= parse_double_scoped(start, end, "c0",      &cal->c0);
    ok &= parse_double_scoped(start, end, "c1",      &cal->c1);
    ok &= parse_double_scoped(start, end, "c2",      &cal->c2);
    ok &= parse_double_scoped(start, end, "fmin",    &cal->fmin);
    ok &= parse_double_scoped(start, end, "fmax",    &cal->fmax);

    cal->loaded = (ok != 0);
    return cal->loaded;
}

static double abs_diff(double a, double b) {
    return (a > b) ? (a - b) : (b - a);
}

// ### PRIVATE IMPLEMENTATION

// Initialize Modbus device
// parameters:
//   int         device_id  - Modbus device ID to connect to (default 1)
//   int         baudrate   - Serial baudrate (default 9600)
//   const char* serial_cfg - Serial configuration string
//
// returns 0 on success, 1 on failure
static int PowerLib_modbus_init(int device_id, int baudrate, const char* serial_cfg) {
    int retries = 3;
    while (retries > 0) {
        if (modbus_begin(device_id, baudrate, getSerialConfig(serial_cfg)) == 0) {
            g_system.modbus_initialized = 1;
            return 0;
        }
        fprintf(stderr, "PowerLib: Failed to open serial port, retrying after 1 second...\n");
        sleep(1);
        retries--;
    }
    
    fprintf(stderr, "PowerLib: Fatal, could not open serial port\n");
    return 1;
}

// Read one modbus register and convert it to voltage
// parameters:
//   int     hpa_index   - 0 or 1 to select HPA1 or HPA2
//   double* out_voltage - output parameter for the read voltage (V)
//
// returns 0 on success, 1 on error
static int PowerLib_modbus_read_voltage(int hpa_index, double* out_voltage) {
    if (!g_system.modbus_initialized) {
        fprintf(stderr, "PowerLib: Modbus not initialized\n");
        return 1;
    }

    // read_input_registers expects an array as a buffer
    uint16_t i_reg[1] = {0};
    int retries = 3;
    modbus_err_t err;

    // thread-safe modbus read with retries
    pthread_mutex_lock(&g_modbus_mutex);
    while (retries-- > 0) {
        err = read_input_registers(hpa_index, 1, i_reg);
        
        if (err == NO_ERR) {
            break;
        }
        usleep(50000); // wait 50ms before retrying
    }
    pthread_mutex_unlock(&g_modbus_mutex);
    
    if (err != NO_ERR) {
        fprintf(stderr, "PowerLib: Modbus error while reading HPA %d: %s\n", hpa_index + 1, getErrorCode(err));
        return 1;
    }

    *out_voltage = i_reg[0] / 1000.0;
    
    if (*out_voltage < 0.0) {
        fprintf(stderr, "PowerLib: HPA %d reports negative voltage, expects positive\n", hpa_index + 1);
        return 1;
    }
    
    return 0;
}

// Parse the JSON configuration file and populate the calibration data for each HPA
// parameters:
//   const char* json_path - path to the JSON configuration file
//
// returns 0 on success, 1 on failure
static int PowerLib_calibration_init(const char* json_path) {
    if (!json_path) {
        fprintf(stderr, "PowerLib: no json_path specified\n");
        return 1;
    }
    
    FILE *f = fopen(json_path, "r");
    if (!f) {
        fprintf(stderr, "PowerLib: cannot open %s\n", json_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    if (len < 0) {
        fclose(f);
        return 1;
    }

    char *json = (char*)malloc(len + 1);
    if (!json) {
        fclose(f);
        return 1;
    }

    size_t read_len = fread(json, 1, len, f);
    json[read_len] = '\0';
    fclose(f);

    const char *keys[POWERLIB_MAX_HPAS] = {"powercalculation1", "powercalculation2"};
    int any_loaded = 0;

    for (int i = 0; i < POWERLIB_MAX_HPAS; i++) {
        const char* end = NULL;
        const char* start = find_json_block(json, keys[i], &end);

        if (!start || !end) continue;

        if (parse_cal_block(start, end, &g_system.calibrations[i])) {
            any_loaded = 1;
        }
    }
    free(json);

    if (!any_loaded) {
        fprintf(stderr, "PowerLib: No valid calibrations found in JSON.\n");
        return 1;
    }

    g_system.calibration_initialized = 1;
    return 0;
}

// Estimate power for specific HPA
// parameters:
//   int     hpa_index - 0 or 1 to select HPA1 or HPA2
//   double  voltage   - voltage (V)
//   double  frequency - frequency (Hz)
//   double* out_watts - output parameter for the estimated power (W)
//
// returns 0 on success, 1 on error
static int PowerLib_get_power(int hpa_index, double voltage, double frequency, double* out_watts) {
    if (!g_system.calibration_initialized) {
        fprintf(stderr, "PowerLib: Library not initialized.\n");
        return 1;
    }
    if (!out_watts) {
        fprintf(stderr, "PowerLib: need valid output pointer for HPA %d\n", hpa_index + 1);
        return 1;
    }
    if (hpa_index < 0 || hpa_index >= POWERLIB_MAX_HPAS) {
        fprintf(stderr, "PowerLib: Invalid HPA index %d\n", hpa_index + 1);
        return 1;
    }

    PowerLib_Calibration* cal = &g_system.calibrations[hpa_index];
    if (!cal->loaded || !cal->enabled) {
        fprintf(stderr, "PowerLib: HPA %d is not enabled or not loaded\n", hpa_index + 1);
        return 1;
    }

    if (abs_diff(cal->fmax, cal->fmin) < 1e-6) {
        fprintf(stderr, "PowerLib: HPA %d has minimum frequency = maximum frequency\n", hpa_index + 1);
        return 1;
    }

    if (frequency < cal->fmin || frequency > cal->fmax) {
        fprintf(stderr, "PowerLib Extrapolation Warning: Frequency %.2f Hz is out of calibrated range [%.2f - %.2f]\n", 
                frequency, cal->fmin, cal->fmax);
    }

    // normalize frequency (e.g. from 400MHz..480Mhz to 0..1)
    double f = (frequency - cal->fmin) / (cal->fmax - cal->fmin);
    double f_squared = f * f;

    // estimate power using the formula: P = a*V^2 + b*V + c, where a, b, c are quadratic functions of normalized frequency
    double a = cal->a0 * f_squared + cal->a1 * f + cal->a2;
    double b = cal->b0 * f_squared + cal->b1 * f + cal->b2;
    double c = cal->c0 * f_squared + cal->c1 * f + cal->c2;

    *out_watts = (a * voltage * voltage) + (b * voltage) + c;

    return 0;
}

// ### PUBLIC IMPLEMENTATION

// Initialize library: Set locale, load calibration data from JSON, init Modbus device
// parameters:
//   const char* json_path - path to the JSON configuration file
//
// returns 0 on success, 1 on failure
int PowerLib_init(const char* json_path) {
    // Make sure to use a consistent locale for parsing numbers (e.g. '.' as decimal separator)
    setlocale(LC_NUMERIC, "C");

    if (PowerLib_calibration_init(json_path) != 0) {
        return 1;
    }

    if (PowerLib_modbus_init(POWERLIB_DEFAULT_DEVICE_ID, POWERLIB_DEFAULT_BAUDRATE, POWERLIB_DEFAULT_SERIAL_CFG) != 0) {
        return 1;
    }

    return 0;
}

// Estimate power for HPA 1
// parameters:
//   double  frequency - current frequency the HPA is running at (Hz)
//   double* out_watts - output parameter for the estimated power (W)
//
// returns 0 on success, 1 on error
int PowerLib_get_power_hpa1(double frequency, double* out_watts) {
    // Modbus read
    double voltage = 0.0;
    if (PowerLib_modbus_read_voltage(POWERLIB_HPA1, &voltage) != 0) {
        return 1;
    }

    int result = PowerLib_get_power(POWERLIB_HPA1, voltage, frequency, out_watts);
    if (result != 0) {
        fprintf(stderr, "PowerLib: Failed to get power for HPA 1.\n");
        return 1;
    }

    return 0;
}

// Estimate power for HPA 2
// parameters:
//   double  frequency - current frequency the HPA is running at (Hz)
//   double* out_watts - output parameter for the estimated power (W)
//
// returns 0 on success, 1 on error
int PowerLib_get_power_hpa2(double frequency, double* out_watts) {
    double watts = 0.0;

    // Modbus read
    double voltage = 0.0;
    if (PowerLib_modbus_read_voltage(POWERLIB_HPA2, &voltage) != 0) {
        return 1;
    }

    int result = PowerLib_get_power(POWERLIB_HPA2, voltage, frequency, out_watts);
    if (result != 0) {
        fprintf(stderr, "PowerLib: Failed to get power for HPA 2.\n");
        return 1;
    }

    return 0;
}

// Cleanup library: Close Modbus port
void PowerLib_cleanup() {
    modbus_close();
    g_system.modbus_initialized = 0;
}