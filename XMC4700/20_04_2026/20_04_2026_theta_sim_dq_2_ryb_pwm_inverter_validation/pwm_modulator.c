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
 * SPWM step: generate switch states from reference voltages
 * 
 * Triangle carrier: normalized to [-1, +1], period = 50µs (20 kHz)
 * PWM logic: 
 *   if v_ref > carrier → switch_top = 1 (top ON, bottom OFF)
 *   if v_ref < carrier → switch_top = 0 (top OFF, bottom ON)
 * 
 * Output: Discrete switch states (0 or 1 for each phase)
 */
PWMSwitchStates pwm_modulator_step(PWMModulator *pwm, float v_ref_a, float v_ref_b, float v_ref_c, float dt) {
    PWMSwitchStates switches;
    
    /* Update carrier time (sawtooth ramp 0 to PWM_PERIOD_SEC) */
    pwm->carrier_time += dt;
    if (pwm->carrier_time >= PWM_PERIOD_SEC) {
        pwm->carrier_time -= PWM_PERIOD_SEC;
    }
    
    /* Generate normalized triangle carrier in [-1, 1] */
    float half_period = PWM_PERIOD_SEC / 2.0f;
    if (pwm->carrier_time < half_period) {
        /* Rising ramp: -1 → +1 */
        pwm->carrier_value = -1.0f + 2.0f * (pwm->carrier_time / half_period);
    } else {
        /* Falling ramp: +1 → -1 */
        pwm->carrier_value = 1.0f - 2.0f * ((pwm->carrier_time - half_period) / half_period);
    }
    
    /* SPWM: Binary comparison of normalized reference vs normalized carrier */
    /* Saturate reference voltages to valid range [-1, +1] */
    float v_a = (v_ref_a > 1.0f) ? 1.0f : (v_ref_a < -1.0f) ? -1.0f : v_ref_a;
    float v_b = (v_ref_b > 1.0f) ? 1.0f : (v_ref_b < -1.0f) ? -1.0f : v_ref_b;
    float v_c = (v_ref_c > 1.0f) ? 1.0f : (v_ref_c < -1.0f) ? -1.0f : v_ref_c;

    /* Generate switch states: 1 = top switch ON, 0 = bottom switch ON */
    switches.top_a = (v_a > pwm->carrier_value) ? 1 : 0;
    switches.top_b = (v_b > pwm->carrier_value) ? 1 : 0;
    switches.top_c = (v_c > pwm->carrier_value) ? 1 : 0;
    
    return switches;
}
