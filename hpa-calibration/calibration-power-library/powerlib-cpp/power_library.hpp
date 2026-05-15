#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <iomanip>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <unistd.h>
#include <mutex>     // mutex for thread-safe modbus access
#include <clocale>   // used to guarantee '.' as decimal separator for JSON parsing

extern "C" {
    #include "modbus.h"
}

namespace PowerLibrary {
    namespace internal {
        // =========================================================
        // Data Structures
        // =========================================================
        struct PowerCalibration {
            double a0 = 0.0, a1 = 0.0, a2 = 0.0;
            double b0 = 0.0, b1 = 0.0, b2 = 0.0;
            double c0 = 0.0, c1 = 0.0, c2 = 0.0;
            double fmin = 0.0, fmax = 0.0;
            bool enabled = false;
        };

        // =========================================================
        // Default Modbus Configuration
        // =========================================================
        inline constexpr int         POWERLIB_DEFAULT_DEVICE_ID  = 1;
        inline constexpr int         POWERLIB_DEFAULT_BAUDRATE   = 9600;
        inline constexpr const char* POWERLIB_DEFAULT_SERIAL_CFG = "8N1";

        // =========================================================
        // Configuration Manager
        //   JSON loader/parser for fetching formula coefficients
        //
        // void load()
        //   Performs file I/O and JSON parsing to populate internal calibration structures
        // parameters:
        //   const std::string& filepath - path to configuration json
        // ---
        //
        // std::optional<PowerCalibration> get_calibration()
        //   Fetches the calibration coefficients for a specific HPA
        // parameters:
        //   size_t hpa_index - specify 0 or 1 for HPA1 or HPA2
        //
        // return:
        //   PowerCalibration - formula coefficients for specific HPA
        // =========================================================
        class PowerConfiguration {
        public:
            // Constructor: set locale for consistent number parsing
            PowerConfiguration() {
                std::setlocale(LC_NUMERIC, "C");
            }

            // load and parse the JSON file
            void load(const std::string& filepath) {
                std::ifstream f(filepath);
                if (!f.is_open()) {
                    throw std::runtime_error("PowerConfiguration: Could not open config file: " + filepath);
                }

                // slurp the whole file into a string
                std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                
                bool loaded_any = false;
                loaded_any |= parse_and_store(json, "powercalculation1", 0);
                loaded_any |= parse_and_store(json, "powercalculation2", 1);

                if (!loaded_any) {
                    throw std::runtime_error("PowerConfiguration: Failed to parse any valid calibration blocks from JSON.");
                }
            }
            
            // get the calibration for a specific HPA
            std::optional<PowerCalibration> get_calibration(size_t hpa_index) const {
                if (hpa_index >= calibrations_.size() || !calibrations_[hpa_index].enabled) {
                    return std::nullopt;
                }
                return calibrations_[hpa_index];
            }

        private:
            static constexpr size_t MAX_HPAS = 2;
            std::array<PowerCalibration, MAX_HPAS> calibrations_ = {};

            // private JSON Parsing Helpers
            bool parse_and_store(std::string_view json, std::string_view key, size_t index) {
                std::string_view block = find_json_block(json, key);
                if (block.empty()) {
                    std::cerr << "PowerConfiguration: JSON: Block '" << key << "' not found or malformed.\n";
                    return false;
                }

                PowerCalibration cal;
                bool ok = true;
                ok &= parse_bool(block, "enabled", cal.enabled);
                ok &= parse_double(block, "a0",    cal.a0);
                ok &= parse_double(block, "a1",    cal.a1);
                ok &= parse_double(block, "a2",    cal.a2);
                ok &= parse_double(block, "b0",    cal.b0);
                ok &= parse_double(block, "b1",    cal.b1);
                ok &= parse_double(block, "b2",    cal.b2);
                ok &= parse_double(block, "c0",    cal.c0);
                ok &= parse_double(block, "c1",    cal.c1);
                ok &= parse_double(block, "c2",    cal.c2);
                ok &= parse_double(block, "fmin",  cal.fmin);
                ok &= parse_double(block, "fmax",  cal.fmax);

                if (ok) {
                    calibrations_[index] = cal;
                    std::cout << "PowerConfiguration: JSON: Loaded '" << key << "' (enabled=" 
                            << (cal.enabled ? "true" : "false") << ")\n";
                    return true;
                } else {
                    std::cerr << "PowerConfiguration: JSON: Missing or bad fields in '" << key << "'\n";
                    return false;
                }
            }

            static std::string_view find_json_block(std::string_view json, std::string_view key) {
                std::string search = "\"" + std::string(key) + "\"";
                size_t pos = json.find(search);
                if (pos == std::string_view::npos) return {};

                pos = json.find(':', pos);
                if (pos == std::string_view::npos) return {};

                pos = json.find('{', pos);
                if (pos == std::string_view::npos) return {};

                size_t block_start = pos;
                int depth = 0;
                
                for (size_t i = pos; i < json.size(); ++i) {
                    if (json[i] == '"') {
                        i++;
                        while (i < json.size() && json[i] != '"') {
                            if (json[i] == '\\' && i + 1 < json.size()) i += 2;
                            else i++;
                        }
                    } else if (json[i] == '{') {
                        depth++;
                    } else if (json[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            return json.substr(block_start, (i - block_start) + 1);
                        }
                    }
                }
                return {};
            }

            static bool parse_double(std::string_view block, std::string_view key, double& out) {
                std::string search = "\"" + std::string(key) + "\"";

                size_t pos = block.find(search);
                if (pos == std::string_view::npos)
                    return false;

                pos = block.find(':', pos);
                if (pos == std::string_view::npos)
                    return false;

                // move past ':'
                pos++;

                // skip whitespace
                while (pos < block.size() &&
                    std::isspace(static_cast<unsigned char>(block[pos])))
                {
                    pos++;
                }

                if (pos >= block.size())
                    return false;

                std::string value_str(block.substr(pos));

                size_t parsed_chars = 0;

                try {
                    out = std::stod(value_str, &parsed_chars);
                }
                catch (...) {
                    return false;
                }

                // validate trailing characters
                size_t i = parsed_chars;

                while (i < value_str.size() &&
                    std::isspace(static_cast<unsigned char>(value_str[i])))
                {
                    i++;
                }

                // only legal JSON terminators
                if (i < value_str.size() &&
                    value_str[i] != ','  &&
                    value_str[i] != '}')
                {
                    return false;
                }

                return true;
            }

            static bool parse_bool(std::string_view block, std::string_view key, bool& out) {
                std::string search = "\"" + std::string(key) + "\"";

                size_t pos = block.find(search);
                if (pos == std::string_view::npos)
                    return false;

                pos = block.find(':', pos);
                if (pos == std::string_view::npos)
                    return false;

                pos++;

                while (pos < block.size() &&
                    std::isspace(static_cast<unsigned char>(block[pos])))
                {
                    pos++;
                }

                if (pos >= block.size())
                    return false;

                auto valid_terminator = [](char c) {
                    return std::isspace(static_cast<unsigned char>(c))
                        || c == ','
                        || c == '}';
                };

                if (block.substr(pos, 4) == "true") {
                    size_t end = pos + 4;

                    if (end >= block.size() || valid_terminator(block[end])) {
                        out = true;
                        return true;
                    }
                }

                if (block.substr(pos, 5) == "false") {
                    size_t end = pos + 5;

                    if (end >= block.size() || valid_terminator(block[end])) {
                        out = false;
                        return true;
                    }
                }

                return false;
            }
        };

        // =========================================================
        // ModbusDevice:
        //   Interface to Modbus Analog Input device
        //
        // ModbusDevice constructor
        // parameters:
        //   int         device_id - id for modbus device to use, defaults to id 1
        //   int         baudrate  - serial port speed, default config for modbus device is 9600
        //   const char* cfg       - configuration string for serial port, default: 8 bits, no parity, 1 stop bit
        // ---
        //
        // std::optional<double> read_voltage()
        //   Thread-safe access to registers containing voltage readings
        // parameters:
        //   int    hpa_index - 0 or 1 to select HPA1 or HPA2
        //
        // return:
        //   optional<double> - the read voltage (V)
        // =========================================================
        class ModbusDevice {
        public:
            ModbusDevice(int         device_id = POWERLIB_DEFAULT_DEVICE_ID,
                         int         baudrate  = POWERLIB_DEFAULT_BAUDRATE,
                         const char* cfg       = POWERLIB_DEFAULT_SERIAL_CFG)
            {
                int retries = 3;
                while (retries > 0) {
                    if (modbus_begin(device_id, baudrate, getSerialConfig(cfg)) == 0) {
                        return;
                    }
                    std::cerr << "ModbusDevice: Failed to open serial port, retrying after 1 second...\n";
                    sleep(1);
                    retries--;
                }
                
                throw std::runtime_error("ModbusDevice Fatal: could not open serial port.");
            }

            ~ModbusDevice() {
                modbus_close();
            }

            std::optional<double> read_voltage(int hpa_index) const {
                // read_input_registers expects an array buffer
                uint16_t i_reg[1] = {0};
                
                modbus_err_t err;
                int retries = 3;
                
                // code-block for thread-safe reading from modbus device
                {
                    // lock mutex with auto release on scope end
                    std::lock_guard<std::mutex> lock(modbus_mutex_);

                    while (retries-- > 0) {
                        err = read_input_registers(hpa_index, 1, i_reg);
                        if (err == NO_ERR) {
                            break;
                        }
                        usleep(50000);
                    }
                }
                
                if (err != NO_ERR) {
                    std::cerr << "ModbusDevice: Read voltage error on HPA " << (hpa_index + 1) << ": " << getErrorCode(err) << "\n";
                    return std::nullopt;
                }

                // modbus device voltage conversion: register / 1000
                return i_reg[0] / 1000.0;
            }
        private:
                mutable std::mutex modbus_mutex_;
        };

        // =========================================================
        // PowerEstimation:
        //   Estimate power based on voltage and frequency
        //
        // PowerEstimation Constructor
        // parameters:
        //   PowerConfiguration& config - structure with coefficients for power estimation
        // ---
        //
        // std::optional<double> calculate_power()
        //   Function mapping (voltage, frequency) -> (watt)
        // parameters:
        //   int    hpa_index - 0 or 1 to select HPA1 or HPA2
        //   double voltage   - voltage (V)
        //   double frequency - frequency (Hz)
        //
        // return:
        //   optional<double> - estimated power (W) or nullopt on error
        // =========================================================
        class PowerEstimation {
        public:
            explicit PowerEstimation(const PowerConfiguration& config) : config_(config) {}

            std::optional<double> calculate_power(int hpa_index, double voltage, double frequency) const {
                if (voltage < 0.0) {
                    std::cerr << "PowerEstimation: HPA " << (hpa_index + 1) << ": Expects positive voltage, got " << voltage << "V (check wiring).\n";
                    return std::nullopt;
                }

                auto cal_opt = config_.get_calibration(hpa_index);
                if (!cal_opt) {
                    std::cerr << "PowerEstimation: HPA " << (hpa_index + 1) << ": not loaded or disabled.\n";
                    return std::nullopt;
                }

                const auto& cal = *cal_opt;
                if (std::abs(cal.fmax - cal.fmin) < 1e-6) {
                    std::cerr << "PowerEstimation: HPA " << (hpa_index + 1) << ": fmin == fmax, will lead to division by zero.\n";
                    return std::nullopt;
                }

                if (frequency < cal.fmin || frequency > cal.fmax) {
                    std::cerr << "PowerEstimation Warning: HPA " << (hpa_index + 1) << ": frequency " << frequency 
                              << " out of calibrated range [" << cal.fmin << " - " << cal.fmax << "]\n";
                }

                // normalize the frequency (e.g. 400e6..480e6 -> 0..1)
                double f = (frequency - cal.fmin) / (cal.fmax - cal.fmin);
                double f_squared = f * f;

                double a = cal.a0 * f_squared + cal.a1 * f + cal.a2;
                double b = cal.b0 * f_squared + cal.b1 * f + cal.b2;
                double c = cal.c0 * f_squared + cal.c1 * f + cal.c2;

                return a * voltage * voltage + b * voltage + c;
            }

        private:
            const PowerConfiguration& config_;
        };
    } // namespace internal

    // =========================================================
    // SimplePower:
    //   Easy to use wrapper with two exposed functions
    // 
    // SimplePower constructor
    // parameters:
    //   const std::string* json_path - path to json containing formula coefficients
    // ---
    //
    // std::optional<double> get_power_hpa1(), get_power_hpa2()
    //   Estimate power on a specific HPA based on frequency. Voltage fetched from modbus device.
    // parameters:
    //   double frequency - current frequency the corresponding HPA is running at (Hz)
    //
    // return:
    //   optional<double> - estimated power (W) or nullopt on error
    // =========================================================
    class SimplePower {
    public:
        explicit SimplePower(const std::string& json_path = "config.json") 
            // initialize coefficient storage, estimation engine, modbus interface
            : config_(),
            estimation_(config_),
            modbus_() // defaults to id 1, baud 9600, serial 8N1
        {
            // load coefficients
            config_.load(json_path);
        }

        std::optional<double> get_power_hpa1(double frequency) {
            auto voltage = modbus_.read_voltage(HPA1);
            if (!voltage) { return std::nullopt; }

            return estimation_.calculate_power(HPA1, *voltage, frequency);
        }

        std::optional<double> get_power_hpa2(double frequency) {
            auto voltage = modbus_.read_voltage(HPA2);
            if (!voltage) { return std::nullopt; }

            return estimation_.calculate_power(HPA2, *voltage, frequency);
        }

    private:
        internal::PowerConfiguration config_;
        internal::PowerEstimation estimation_;
        internal::ModbusDevice modbus_;

        static constexpr int HPA1 = 0;
        static constexpr int HPA2 = 1;
    };
} // namespace PowerLibrary