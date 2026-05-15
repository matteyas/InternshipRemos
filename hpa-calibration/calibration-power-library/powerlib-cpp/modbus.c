// C library headers
#include <stdio.h>
#include <string.h>

// Linux headers
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <unistd.h>  // write(), read(), close()
#include <gpiod.h>

#include "modbus.h"

char* _serial_port_name = "/dev/ttyAMA0";
int _serial_port;
int _baudrate;
uint8_t _slave_id;
char _write_buf [256];
struct gpiod_chip *chip;
struct gpiod_line *gpio27;
struct gpiod_line *gpio17;

///////////////////////////////////////////////////////////////////////////////////////
static void configSerialPort(struct termios *tty, serial_config_t config) {
  switch (config) {
  case SERIAL_5N1:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_6N1:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_7N1:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_8N1:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_5N2:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_6N2:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_7N2:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_8N2:
    tty->c_cflag &= ~PARENB; // Disable parity
    tty->c_cflag &= ~PARODD; // Not used
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_5E1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_6E1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_7E1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_8E1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_5E2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_6E2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_7E2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;

  case SERIAL_8E2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even partiy
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag |= CSTOPB;  // 2 stop bits
    break;


  case SERIAL_5O1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_6O1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_7O1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_8O1:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bit
    break;

  case SERIAL_5O2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS5;     // 5 data bits
    tty->c_cflag |= CSTOPB;  // 1 stop bits
    break;

  case SERIAL_6O2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS6;     // 6 data bits
    tty->c_cflag |= CSTOPB;  // 1 stop bits
    break;

  case SERIAL_7O2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS7;     // 7 data bits
    tty->c_cflag |= CSTOPB;  // 1 stop bits
    break;

  case SERIAL_8O2:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag |= PARODD;  // Odd partiy
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag |= CSTOPB;  // 1 stop bits
    break;

  // Default
  case SERIAL_INVALID:
    tty->c_cflag |= PARENB;  // Enable parity
    tty->c_cflag &= ~PARODD; // Even parity
    tty->c_cflag |= CS8;     // 8 data bits
    tty->c_cflag &= ~CSTOPB; // 1 stop bits
  }
}

///////////////////////////////////////////////////////////////////////////////////////
int modbus_begin(uint8_t slave_id, int baudrate, serial_config_t serial_c) {

  // Setup port
  _serial_port = open(_serial_port_name, O_RDWR);

  _baudrate = baudrate;
  _slave_id = slave_id;

  // Create new termios struct, we call it 'tty' for convention
  struct termios tty;
  // Read in existing settings, and handle any error
  if(tcgetattr(_serial_port, &tty) != 0) {
      fprintf(stderr, "Error %i from tcgetattr: %s\n", errno, strerror(errno));
      return 1;
  }

  // configure serial port parameters accordingly
  configSerialPort(&tty, serial_c);

  tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
  tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO; // Disable echo
  tty.c_lflag &= ~ECHOE; // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo
  tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

  tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
  // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
  // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

  tty.c_cc[VTIME] = 1;    // Wait for up to (1 deciseconds), returning as soon as any data is received.
  tty.c_cc[VMIN] = 0;

  // Set in/out baud rate to be var _baudrate
  cfsetispeed(&tty, getbaud(_baudrate));
  cfsetospeed(&tty, getbaud(_baudrate));

  // Save tty settings, also checking for error
  if (tcsetattr(_serial_port, TCSANOW, &tty) != 0) {
      fprintf(stderr, "Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
  }

  const char *chipname = "gpiochip0";

  chip = gpiod_chip_open_by_name(chipname);

  if (chip == NULL) {
    fprintf(stderr, "Error while opening by name\n");
    return 1;
  }

  gpio17 = gpiod_chip_get_line(chip, 17);
  gpio27 = gpiod_chip_get_line(chip, 27);

  if (gpio17 == NULL) {
    fprintf(stderr, "Error while getting line GPIO 17\n");
    return 1;
  }

  if (gpio27 == NULL) {
    fprintf(stderr, "Error while getting line GPIO 27\n");
    return 1;
  }

  if (gpiod_line_request_output(gpio17, "modbusrtu", 0)) {
    fprintf(stderr, "Error while setting GPIO17 to OUTPUT\n");
    return 1;
  }

  if (gpiod_line_request_output(gpio27, "modbusrtu", 0)) {
    fprintf(stderr, "Error while setting GPIO27 to OUTPUT\n");
    return 1;
  }

  // End
  return 0;

}

///////////////////////////////////////////////////////////////////////////////////////
int modbus_close(void) {

  return close(_serial_port);

}

///////////////////////////////////////////////////////////////////////////////////////
void add_uint16(char **ptr, uint16_t value) {
    **ptr = (value >> 8) & 0xFF; (*ptr)++;
    **ptr = value & 0xFF; (*ptr)++;
}

///////////////////////////////////////////////////////////////////////////////////////
int validate_uint16_be(char **ptr, uint16_t expected) {
    uint16_t value = ((*ptr)[1] << 8) | (*ptr)[0];
    (*ptr) += 2;
    return value == expected;
}


///////////////////////////////////////////////////////////////////////////////////////
int validate_uint16_le(char **ptr, uint16_t expected) {
    uint16_t value = ((*ptr)[0] << 8) | (*ptr)[1];
    (*ptr) += 2;
    return value == expected;
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t do_modbus_petition(uint8_t FC, uint16_t address, uint16_t value, uint8_t* values, uint16_t quantity, uint8_t* target) {

  char *ptr = _write_buf;
  *ptr++ = _slave_id;
  *ptr++ = FC;
  add_uint16(&ptr, address);

  switch (FC) {
  case 0x05:  // Write single coil
    *ptr++ = value ? 0xFF : 0x00;
    *ptr++ = 0x00;
    break;
  case 0x06:  // Write single register
    add_uint16(&ptr, value);
    break;
  case 0x0F:  // Write multiple coils
    add_uint16(&ptr, quantity);
    int byte_count = (quantity + 7) / 8;
    *ptr++ = byte_count;
    for (int i = 0; i < byte_count; i++) {
      uint8_t byte = 0;
      for (int bit = 0; bit < 8 && (i * 8 + bit) < quantity; bit++) {
        byte |= (values[i * 8 + bit] << bit);
      }
      *ptr++ = byte;
    }
    break;
  case 0x10:  // Write multiple registers
    add_uint16(&ptr, quantity);
    *ptr++ = quantity * 2;
    for (int i = 0; i < quantity * 2; i++) {
      *ptr++ = values[i+1];
      *ptr++ = values[i++];
    }
    break;
  case 0x01:  // Read coils
  case 0x02:  // Read discrete inputs
  case 0x03:  // Read holding registers
  case 0x04:  // Read input registers
    add_uint16(&ptr, quantity);
    break;
  default:
    /* fprintf(stderr, "Error: Bad Function Code\n"); */
    return ILLEGAL_FUNCTION;
  }

  // Calculate and append CRC
  uint16_t crc = calculateCRC16((const uint8_t *)_write_buf, ptr - _write_buf);
  add_uint16(&ptr, crc);

  // Send frame and handle errors
  uint8_t wlen = ptr - _write_buf;
  if (send_frame(_write_buf, wlen) != 0) {
    /* fprintf(stderr, "Error: Unable to send\n"); */
    return SEND_ERR;
  }

  // Read response
  char response[256];
  int resp_len = read_frame(response);
  if (resp_len <= 0) {
    /* fprintf(stderr, "Error: No response\n"); */
    return READ_ERR;
  }

  ptr = response;
  if (*ptr++ != _slave_id) {
    /* fprintf(stderr, "Error: Address is wrong\n"); */
    return NOT_SAME_MESS_ERR;
  }

  FC = *ptr++;

  switch (FC) {
  case 0x05:  // Write single coil
    if (!validate_uint16_le(&ptr, address) ||
        !validate_uint16_be(&ptr, value)) {
      /* fprintf(stderr, "Error: Not same message\n"); */
      return NOT_SAME_MESS_ERR;
    }
    break;
  case 0x06:  // Write single register
    if (!validate_uint16_le(&ptr, address) ||
        !validate_uint16_le(&ptr, value)) {
      /* fprintf(stderr, "Error: Not same message\n"); */
      return NOT_SAME_MESS_ERR;
    }
    break;
  case 0x0F:  // Write multiple coils
  case 0x10:  // Write multiple registers
    if (!validate_uint16_le(&ptr, address) ||
        !validate_uint16_le(&ptr, quantity)) {
      /* fprintf(stderr, "Error: Not same message\n"); */
      return NOT_SAME_MESS_ERR;
    }
    break;
  case 0x01:  // Read coils
  case 0x02:  // Read discrete inputs
  {
    uint8_t byte_count = (quantity + 7) / 8;
    if (*ptr++ != byte_count) {
      /* fprintf(stderr, "Error: Bad response\n"); */
      return BAD_RESPONSE_ERR;
    }
    for (int i = 0; i < byte_count; i++) {
      for (int bit = 0; bit < 8 && (i * 8 + bit) < quantity; bit++) {
        target[i * 8 + bit] = (*ptr & (1 << bit)) ? 1 : 0;
      }
      ptr++;
    }
    break;
  }
  case 0x03:
  case 0x04:  // Read input registers
    if (*ptr++ != quantity * 2) {
      /* fprintf(stderr, "Error: Bad response\n"); */
      return BAD_RESPONSE_ERR;
    }
    for (int i = 0; i < quantity * 2; i++) {
      target[i+1] = *ptr++;
      target[i++] = *ptr++;
    }
    break;
  // Exceptions
  default:
  {
    uint8_t error = *ptr++;

    // Check CRC first
    uint16_t received_crc = (ptr[1] | (ptr[0] << 8));
    if (calculateCRC16((const uint8_t *)response, ptr - response) != received_crc) {
      /* fprintf(stderr, "Error: CRC is wrong\n"); */
      return CRC_ERR;
    }

    if (error == 0x01) {
      /* fprintf(stderr, "Error: Illegal function\n"); */
      return ILLEGAL_FUNCTION;
    }
    else if (error == 0x02) {
      /* fprintf(stderr, "Error: Illegal address\n"); */
      return ILLEGAL_ADDRESS;
    }
    else if (error == 0x03) {
      /* fprintf(stderr, "Error: Illegal value\n"); */
      return ILLEGAL_VALUE;
    }
    else {
      /* fprintf(stderr, "Error: Bad response\n"); */
      return BAD_RESPONSE_ERR;
    }
    break;
  }
  }

  // Check CRC
  uint16_t received_crc = (ptr[1] | (ptr[0] << 8));
  if (calculateCRC16((const uint8_t *)response, ptr - response) != received_crc) {
    /* fprintf(stderr, "Error: CRC is wrong\n"); */
    return CRC_ERR;
  }

  // Success
  return NO_ERR;
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t write_single_coil(uint16_t address, uint8_t value) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x05;
  return do_modbus_petition(FC, address, value ? 0xFF : 0x00, NULL,  0, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t write_single_register(uint16_t address, uint16_t value) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x06;
  return do_modbus_petition(FC, address, value, NULL, 0, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t write_multiple_coils(uint16_t address, uint8_t* values, uint16_t quantity) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x0F;
  return do_modbus_petition(FC, address, 0, values, quantity, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t write_multiple_registers(uint16_t address, uint16_t* values, uint16_t quantity) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x10;
  return do_modbus_petition(FC, address, 0, (uint8_t*)values, quantity, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t read_coils(uint16_t address, uint16_t quantity, uint8_t* target) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x01;
  return do_modbus_petition(FC, address, 0, NULL, quantity, target);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t read_discrete_inputs(uint16_t address, uint16_t quantity, uint8_t* target) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x02;
  return do_modbus_petition(FC, address, 0, NULL, quantity, target);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t read_input_registers(uint16_t address, uint16_t quantity, uint16_t* target) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x04;
  return do_modbus_petition(FC, address, 0, NULL, quantity, (uint8_t*)target);
}

///////////////////////////////////////////////////////////////////////////////////////
modbus_err_t read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* target) {
  // Prepare Modbus RTU frame
  uint8_t FC = 0x03;
  return do_modbus_petition(FC, address, 0, NULL, quantity, (uint8_t*)target);
}

///////////////////////////////////////////////////////////////////////////////////////
int send_frame(char* buf, uint8_t buflen) {

  // Setup pin 27 HIGH
  if (gpiod_line_set_value(gpio17, 1) != 0) {
    fprintf(stderr, "Error: Error while setting GPIO 17 to HIGH\n");
    return 1;
  }

  // Setup pin 17 HIGH
  if (gpiod_line_set_value(gpio27, 1) != 0) {
    fprintf(stderr, "Error: Error while setting GPIO 27 to HIGH\n");
    return 1;
  }

  int t = 10 * buflen * 1000000 / _baudrate;
  usleep(t);
  usleep(1000);

  // Write to port
  if (write(_serial_port, buf, buflen) < 0) {
    fprintf(stderr, "Error: Error writing to serial port\n");
    return 1;
  }

  t = 10 * buflen * 1000000 / _baudrate;
  usleep(t);
  usleep(1000);

  // Setup pin 27 LOW
  if (gpiod_line_set_value(gpio17, 0) != 0) {
    fprintf(stderr, "Error: Error while setting GPIO 17 to LOW\n");
    return 1;
  }

  // Setup pin 17 LOW
  if (gpiod_line_set_value(gpio27, 0) != 0) {
    fprintf(stderr, "Error: Error while setting GPIO 27 to LOW\n");
    return 1;
  }

  usleep(5000);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
int read_frame(char* buf) {
  char c;
  int n;
  char* ptr = buf;
  int num_bytes = 0;

  do {
    n = read(_serial_port, &c, 1);
    if (n < 0) {
      fprintf(stderr, "Error: Read serial port fail\n");
      return -1;
    } else if (n > 0) {
      *ptr++ = c;
      ++num_bytes;
    }
  } while (n > 0);

  return num_bytes;
}

///////////////////////////////////////////////////////////////////////////////////////
uint16_t calculateCRC16(const uint8_t *data, uint8_t length) {
  const uint16_t polynomial = 0xA001;
  uint16_t crc = 0xFFFF;

  for (uint8_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; ++j) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ polynomial;
      } else {
        crc >>= 1;
      }
    }
  }

  return (crc >> 8) | (crc << 8);
}

///////////////////////////////////////////////////////////////////////////////////////
serial_config_t getSerialConfig(const char *str) {
  if (strcmp(str, "5N1") == 0) return SERIAL_5N1;
  if (strcmp(str, "6N1") == 0) return SERIAL_6N1;
  if (strcmp(str, "7N1") == 0) return SERIAL_7N1;
  if (strcmp(str, "8N1") == 0) return SERIAL_8N1;
  if (strcmp(str, "5N2") == 0) return SERIAL_5N2;
  if (strcmp(str, "6N2") == 0) return SERIAL_6N2;
  if (strcmp(str, "7N2") == 0) return SERIAL_7N2;
  if (strcmp(str, "8N2") == 0) return SERIAL_8N2;

  if (strcmp(str, "5E1") == 0) return SERIAL_5E1;
  if (strcmp(str, "6E1") == 0) return SERIAL_6E1;
  if (strcmp(str, "7E1") == 0) return SERIAL_7E1;
  if (strcmp(str, "8E1") == 0) return SERIAL_8E1;
  if (strcmp(str, "5E2") == 0) return SERIAL_5E2;
  if (strcmp(str, "6E2") == 0) return SERIAL_6E2;
  if (strcmp(str, "7E2") == 0) return SERIAL_7E2;
  if (strcmp(str, "8E2") == 0) return SERIAL_8E2;

  if (strcmp(str, "5O1") == 0) return SERIAL_5O1;
  if (strcmp(str, "6O1") == 0) return SERIAL_6O1;
  if (strcmp(str, "7O1") == 0) return SERIAL_7O1;
  if (strcmp(str, "8O1") == 0) return SERIAL_8O1;
  if (strcmp(str, "5O2") == 0) return SERIAL_5O2;
  if (strcmp(str, "6O2") == 0) return SERIAL_6O2;
  if (strcmp(str, "7O2") == 0) return SERIAL_7O2;
  if (strcmp(str, "8O2") == 0) return SERIAL_8O2;

  return SERIAL_INVALID;
}

///////////////////////////////////////////////////////////////
const char *getErrorCode(modbus_err_t err) {
  switch (err) {
  case NO_ERR: return "No Error";
  case SEND_ERR: return "Send Error";
  case READ_ERR: return "Read Error";
  case NOT_SAME_MESS_ERR: return "Not Same Message Error";
  case BAD_RESPONSE_ERR: return "Bad Response Error";
  case CRC_ERR: return "CRC Error";
  case ILLEGAL_FUNCTION: return "Illegal Function";
  case ILLEGAL_ADDRESS: return "Illegal Address";
  case ILLEGAL_VALUE: return "Illegal Value";
  default: return "Unknown Error";
  }
}

///////////////////////////////////////////////////////////////////////////////////////
int getbaud(int baud) {
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default:
        return B0;
    }
}
