#include "power_library.hpp"
#include <iostream>

// Test functionality
int main() {
    try {
        // create an estimator
        PowerLibrary::SimplePower estimation;

        // run calculations
        double frequency = 430e6;

        auto w1 = estimation.get_power_hpa1(frequency);
        if (w1) {
            std::cout << "HPA 1 Power: " << *w1 << "W\n";
        }

        auto w2 = estimation.get_power_hpa2(frequency);
        if (w2) {
            std::cout << "HPA 2 Power: " << *w2 << "W\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\nApplication Terminated: " << e.what() << "\n";
        return 1;
    }

    return 0;
}