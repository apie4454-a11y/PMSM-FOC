/*
 * pwm_modulator.c
 * 
 * SPWM Modulator Implementation
 * Generates 20 kHz triangle carrier and compares vs reference voltages
 */

#include "pwm_modulator.h"
#include <math.h>

/*
 * Initialize PWM modulator
 */
void pwm_modulator_init(PWMModulator *pwm) {
    pwm->carrier_time = 0.0f;
    pwm->carrier_value = 0.0f;
    pwm->sample_count = 0;
}

/*
 * SPWM step: generate duty cycles from reference voltages
 * 
 * Triangle carrier: peaks at VDC/2, period = 50µs (20 kHz)
 * PWM logic: 
 *   if v_ref > carrier → duty = PWM_MAX_DUTY (switch ON)
 *   if v_ref < carrier → duty = 0 (switch OFF)
 * 
 * This produces binary switching output: ±Vdc/2 stepped waveform
 */
PWMDutyCycles pwm_modulator_step(PWMModulator *pwm, float v_ref_a, float v_ref_b, float v_ref_c, float dt) {
    PWMDutyCycles duty;
    
    /* Update carrier time (sawtooth ramp 0 to PWM_PERIOD_SEC) */
    pwm->carrier_time += dt;
    if (pwm->carrier_time >= PWM_PERIOD_SEC) {
        pwm->carrier_time -= PWM_PERIOD_SEC;
    }
    
    /* Generate triangle carrier (0 to VDC/2 to 0) */
    float vdc_2 = PWM_VDC / 2.0f;
    float half_period = PWM_PERIOD_SEC / 2.0f;
    
    if (pwm->carrier_time < half_period) {
        /* Rising ramp: 0 → VDC/2 */
        pwm->carrier_value = vdc_2 * (pwm->carrier_time / half_period);
    } else {
        /* Falling ramp: VDC/2 → 0 */
        pwm->carrier_value = vdc_2 * (2.0f - pwm->carrier_time / half_period);
    }
    
    /* SPWM: Binary comparison of reference vs carrier */
    /* Saturate reference voltages to valid range [-VDC/2, +VDC/2] */
    float v_a = (v_ref_a > vdc_2) ? vdc_2 : (v_ref_a < -vdc_2) ? -vdc_2 : v_ref_a;
    float v_b = (v_ref_b > vdc_2) ? vdc_2 : (v_ref_b < -vdc_2) ? -vdc_2 : v_ref_b;
    float v_c = (v_ref_c > vdc_2) ? vdc_2 : (v_ref_c < -vdc_2) ? -vdc_2 : v_ref_c;
    
    /* Binary PWM comparison:
     * Shift reference to [0, VDC/2] by adding VDC/2 offset
     * Then compare with carrier which is also [0, VDC/2]
     * If (ref + VDC/2) > carrier → switch ON (duty = PWM_MAX_DUTY)
     * Otherwise → switch OFF (duty = 0)
     */
    float v_a_shifted = v_a + vdc_2;  /* Map [-VDC/2, +VDC/2] to [0, VDC] */
    float v_b_shifted = v_b + vdc_2;
    float v_c_shifted = v_c + vdc_2;
    
    /* Compare with carrier and output binary duty */
    duty.duty_a = (v_a_shifted > pwm->carrier_value) ? PWM_MAX_DUTY : 0.0f;
    duty.duty_b = (v_b_shifted > pwm->carrier_value) ? PWM_MAX_DUTY : 0.0f;
    duty.duty_c = (v_c_shifted > pwm->carrier_value) ? PWM_MAX_DUTY : 0.0f;
    
    return duty;
}
