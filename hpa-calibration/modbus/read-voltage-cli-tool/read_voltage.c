#include <stdio.h>

#include "modbus.h"

#define DEVICE_ID  1
#define BAUDRATE   9600
#define SERIAL_CFG "8N1"
#define FROM_ADDR  0
#define REG_COUNT  2

int main() {
    uint16_t i_reg[REG_COUNT];

    // init modbus serial communication
    if (modbus_begin(DEVICE_ID, BAUDRATE, getSerialConfig(SERIAL_CFG))) {
        fprintf(stderr, "Fatal: Could not open serial port.\n");
        return 1;
    }

    // read modbus
    modbus_err_t err = read_input_registers(FROM_ADDR, REG_COUNT, i_reg);
    
    if (err != NO_ERR) {
        fprintf(stderr, "Modbus Error: %s\n", getErrorCode(err));
        modbus_close();
        return 1;
    }

    // voltage = register / 1000
    printf("HPA1: %.2fV\n", i_reg[0] / 1000.0);
    printf("HPA2: %.2fV\n", i_reg[1] / 1000.0);

    modbus_close();
    return 0;
}
