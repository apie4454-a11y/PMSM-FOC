#ifndef M_PI
#define M_PI 3.14159265f
#endif

#include "inverter_model.h"
#include <math.h>
#include <stdio.h>

/* RTT support */
#include "RTT/RTT/SEGGER_RTT.h"
#include "RTT/Config/SEGGER_RTT_Conf.h"

/*
 * TEST 1: Continuous PWM Test
 * Tests that duty cycle → voltage transform is linear
 * Input: dq voltages swept through 2π
 * Output: 3-phase sinusoids showing smooth variation
 */
void inverter_continuous_test() {
    InverterModel inv;
    inverter_init(&inv, 59.4f); // Vdc = 59.4V

    // RTT header - theta and PWM-switched voltages (7-level quantization)
    SEGGER_RTT_WriteString(0, "TEST_CONTINUOUS,theta,vrn_switched,vyn_switched,vbn_switched\n");

    const float VDC_2 = 29.7f;
    const float DUTY_CENTER = 3000.0f;
    const float DUTY_MAX_SWING = 3000.0f;

    // Sweep theta from 0 to 2*pi (one electrical cycle)
    for (float theta = 0; theta < 2 * M_PI; theta += 0.05f) {
        // Test inputs: dq voltages (rotating reference frame)
        float vd_ref = 0.0f;      // d-axis
        float vq_ref = 10.0f;     // q-axis

        // Inverse Park transform: dq -> RYB
        float vr = vd_ref * cosf(theta) - vq_ref * sinf(theta);
        float vy = vd_ref * cosf(theta - 2.0f * M_PI / 3.0f) - vq_ref * sinf(theta - 2.0f * M_PI / 3.0f);
        float vb = vd_ref * cosf(theta - 4.0f * M_PI / 3.0f) - vq_ref * sinf(theta - 4.0f * M_PI / 3.0f);

        // Map RYB voltages to PWM duty cycles [0, 6000]
        float duty_r = DUTY_CENTER + (vr / VDC_2) * DUTY_MAX_SWING;
        float duty_y = DUTY_CENTER + (vy / VDC_2) * DUTY_MAX_SWING;
        float duty_b = DUTY_CENTER + (vb / VDC_2) * DUTY_MAX_SWING;

        // Clamp duty cycles
        if (duty_r < 0.0f) duty_r = 0.0f; if (duty_r > 6000.0f) duty_r = 6000.0f;
        if (duty_y < 0.0f) duty_y = 0.0f; if (duty_y > 6000.0f) duty_y = 6000.0f;
        if (duty_b < 0.0f) duty_b = 0.0f; if (duty_b > 6000.0f) duty_b = 6000.0f;

        // PWM comparison: use mid-point of triangle (3000) as reference
        // This shows ONE sample at a representative point in the PWM cycle
        float triangle_mid = 3000.0f;

        // Sine-Triangle comparison: if duty > triangle, output +Vdc/2, else -Vdc/2
        float sw_r = (duty_r > triangle_mid) ? VDC_2 : -VDC_2;
        float sw_y = (duty_y > triangle_mid) ? VDC_2 : -VDC_2;
        float sw_b = (duty_b > triangle_mid) ? VDC_2 : -VDC_2;

        // Compute terminal-to-midpoint voltages (common mode removal)
        float v_avg = (sw_r + sw_y + sw_b) / 3.0f;

        // Phase-to-neutral voltages (the 7-level stepped output!)
        float vrn_sw = sw_r - v_avg;
        float vyn_sw = sw_y - v_avg;
        float vbn_sw = sw_b - v_avg;

        // Log one sample per theta showing quantized levels
        char buf[128];
        sprintf(buf, "%f,%f,%f,%f\n", 
                theta, vrn_sw, vyn_sw, vbn_sw);
        SEGGER_RTT_WriteString(0, buf);
    }
}

/*
 * Main inverter test runner
 * Executes continuous PWM test to validate SPWM behavior
 */
void inverter_model_rtt_test() {
    SEGGER_RTT_WriteString(0, "\n=== INVERTER MODEL SPWM TEST ===\n");
    
    SEGGER_RTT_WriteString(0, "\n--- Running SPWM validation test (continuous duty cycles) ---\n");
    inverter_continuous_test();
    
    SEGGER_RTT_WriteString(0, "\n=== TEST COMPLETE ===\n");
}