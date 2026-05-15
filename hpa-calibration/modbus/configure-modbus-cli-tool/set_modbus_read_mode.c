#include <stdio.h>

#include "modbus.h"

#define DEVICE_ID  1
#define BAUDRATE   9600
#define SERIAL_CFG "8N1"

#define REG_COUNT  8
#define FROM_ADDR  0x1000

int main() {
    uint16_t i_reg[REG_COUNT];

    // init modbus serial communication
    if (modbus_begin(DEVICE_ID, BAUDRATE, getSerialConfig(SERIAL_CFG))) {
        fprintf(stderr, "Fatal: Could not open serial port.\n");
        return 1;
    }

    // read modbus
    printf("Note that if the device is configured to 0-10V, this should output 0 for all registers.\n");
    modbus_err_t err = read_holding_registers(FROM_ADDR, REG_COUNT, i_reg);
    if (err != NO_ERR) {
        fprintf(stderr, "Modbus Error: %s\n", getErrorCode(err));
        modbus_close();
        return 1;
    }

    for (int i = 0; i < REG_COUNT; i++) {
        printf("Register 0x%04X: %hu\n", FROM_ADDR + i, i_reg[i]);
    }

    printf("\n");

    printf("Input 'y' to configure device to 0-10V, anything else to skip: ");
    char c = getchar();

    if (c != 'y' && c != 'Y') {
        modbus_close();
        return 0;
    }

    printf("Writing default values to registers...\n");
    // set all data types on modbus device to 0-10V (0x0000)
    for (int address = FROM_ADDR; address < FROM_ADDR + REG_COUNT; address++) {
        int value = 0;
        err = write_single_register(address, value);
        if (err != NO_ERR) {
            fprintf(stderr, "Modbus Error: %s\n", getErrorCode(err));
            modbus_close();
            return 1;
        }
        printf("Register 0x%04X: set to %hu\n", address, value);
    }

    modbus_close();
    return 0;
}
