# Power Estimation Library

C++ and C code available. This readme will focus on the C++ version (`powerlib-cpp/power_library.hpp`).

This library handles three things:
1) Parsing of a JSON configuration file containing calibration data (output from calibration tool)
2) Communication with the Modbus device to read the voltage monitor signal from the HPAs
3) Converting the read voltage and a user supplied frequency (in Hz) to a power estimate

The public interface is simple to use. Construct an instance, passing the path to the JSON configuration file as the only input parameter. Two functions (`get_power_hpa1` and `get_power_hpa1`) are available that take frequency (as a double, specified in Hz, so e.g. 435000000 or 435e6 for 435MHz) as an input parameter and outputs the estimated power (in watts). The public interface is included in full below.

Test code (`powerlib-cpp/power_library_test.cpp`) is available and can be built using `./build-test.sh` and then run with `./power-test`, with a JSON config file included by default. The test code assumes a Modbus device is available.

Note that this library currently depends on a C library for Modbus communication that is [provided by Industrial Shields](https://www.industrialshields.com/blog/raspberry-pi-for-industry-26/how-to-use-modbus-rtu-with-touchberry-panel-upsafepi-646?) (`modbus.h` and `modbus.c`, both included in this repository). The modbus library can be compiled separately (the build script for the test code mentioned in the prior paragraph compiles it).

```cpp
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
```
