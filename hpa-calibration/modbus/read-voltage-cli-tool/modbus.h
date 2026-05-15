#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include <termios.h> // Contains POSIX terminal control definitions

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SERIAL_5N1,
  SERIAL_6N1,
  SERIAL_7N1,
  SERIAL_8N1,
  SERIAL_5N2,
  SERIAL_6N2,
  SERIAL_7N2,
  SERIAL_8N2,

  SERIAL_5E1,
  SERIAL_6E1,
  SERIAL_7E1,
  SERIAL_8E1,
  SERIAL_5E2,
  SERIAL_6E2,
  SERIAL_7E2,
  SERIAL_8E2,

  SERIAL_5O1,
  SERIAL_6O1,
  SERIAL_7O1,
  SERIAL_8O1,
  SERIAL_5O2,
  SERIAL_6O2,
  SERIAL_7O2,
  SERIAL_8O2,

  SERIAL_INVALID,
} serial_config_t;

typedef enum {
    NO_ERR = 0,
    SEND_ERR,
    READ_ERR,
    NOT_SAME_MESS_ERR,
    BAD_RESPONSE_ERR,
    CRC_ERR,
    ILLEGAL_FUNCTION,
    ILLEGAL_ADDRESS,
    ILLEGAL_VALUE
} modbus_err_t;

int modbus_begin(uint8_t slave_id, int baudrate, serial_config_t serial_c);
int modbus_close(void);

modbus_err_t do_modbus_petition(uint8_t FC, uint16_t address, uint16_t value, uint8_t* values, uint16_t quantity, uint8_t* target);
modbus_err_t write_single_coil(uint16_t address, uint8_t value);
modbus_err_t write_single_register(uint16_t address, uint16_t value);
modbus_err_t write_multiple_coils(uint16_t address, uint8_t* values, uint16_t quantity);
modbus_err_t write_multiple_registers(uint16_t address, uint16_t* values, uint16_t quantity);
modbus_err_t read_coils(uint16_t address, uint16_t quantity, uint8_t* target);
modbus_err_t read_input_registers(uint16_t address, uint16_t quantity, uint16_t* target);
modbus_err_t read_discrete_inputs(uint16_t address, uint16_t quantity, uint8_t* target);
modbus_err_t read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* target);

uint16_t calculateCRC16(const uint8_t *data, uint8_t length);
int getbaud(int baud);

int send_frame(char* buf, uint8_t buflen);
int read_frame(char* buf);

serial_config_t getSerialConfig(const char *str);

const char *getErrorCode(modbus_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_H */