#include "DAVE.h"

// PWM update variables
uint32_t duty_cycle_value = 3600;  // 50% of 7200 period (from DAVE calc)

int main(void) {
    // Initialize DAVE - this sets up the PWM with your configuration
    if (DAVE_Init() != DAVE_STATUS_SUCCESS) {
        while(1);  // Halt if init fails
    }

    // Start PWM - outputs begin running at 20 kHz
    // P1.5 (CH1 Direct):  50% duty at 20 kHz
    // P1.11 (CH1 Invert): Complementary with 97.2 ns dead time
    PWM_CCU8_Start(&PWM_CCU8_0);
    // Main loop - PWM runs autonomously in hardware
    while(1) {
        // PWM is free-running at 20 kHz
        // You can update duty cycle here if needed:

        // Example: sweep duty cycle 0% → 100% → 0%
        // duty_cycle_value += 100;
        // if (duty_cycle_value > 7200) duty_cycle_value = 0;
        // PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, duty_cycle_value);

        // For now, just keep 50% (3600) running
    }

    return 0;
}
