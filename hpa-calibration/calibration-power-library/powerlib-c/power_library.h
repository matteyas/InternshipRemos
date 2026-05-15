#ifndef POWERLIB_H
#define POWERLIB_H

// Initializes the system, modbus, configuration (returns 0 success, 1 error)
int PowerLib_init(const char* json_path);

// Fetches the current power. (returns 0 for success, 1 for error)
int PowerLib_get_power_hpa1(double frequency, double* out_watts);
int PowerLib_get_power_hpa2(double frequency, double* out_watts);

// Safely shuts down the modbus hardware connection
void PowerLib_cleanup(void);

#endif // POWERLIB_H