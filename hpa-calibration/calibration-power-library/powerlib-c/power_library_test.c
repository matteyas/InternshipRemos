#include <stdio.h>
#include "power_library.h"

int main(void) {
    // initialize
    if (PowerLib_init("config.json") != 0) {
        printf("Failed to start PowerLib system.\n");
        return 1;
    }

    // read the power
    double watts = 0.0;
    double frequency = 430e6;
    
    if (PowerLib_get_power_hpa1(frequency, &watts) == 0) {
        printf("HPA 1 Output: %.4f W\n", watts);
    } else {
        printf("Failed to read HPA 1.\n");
    }

    if (PowerLib_get_power_hpa2(frequency, &watts) == 0) {
        printf("HPA 2 Output: %.4f W\n", watts);
    } else {
        printf("Failed to read HPA 2.\n");
    }

    // safely close the serial port
    PowerLib_cleanup();

    return 0;
}