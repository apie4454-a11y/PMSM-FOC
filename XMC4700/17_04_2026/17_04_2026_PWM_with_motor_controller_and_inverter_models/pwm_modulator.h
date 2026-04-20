/*
 * pwm_modulator.h
 * 
 * SPWM (Sinusoidal PWM) Modulator with 20 kHz Triangle Carrier
 * Converts reference voltages to PWM duty cycles
 * 
 * Reference: standard SPWM technique
 * Carrier: 20 kHz triangle wave (0 to VDC/2)
 */

#ifndef PWM_MODULATOR_H
#define PWM_MODULATOR_H

#include <stdint.h>

/* PWM Configuration */
#define PWM_FREQ_HZ     20000.0f        /* Carrier frequency [Hz] */
#define PWM_PERIOD_SEC  (1.0f / PWM_FREQ_HZ)  /* Period = 50 µs */
#define PWM_MAX_DUTY    6000.0f         /* Max duty counts (100%) */
#define PWM_VDC         59.4f           /* DC bus voltage [V] */

/* PWM modulator state */
typedef struct {
    float carrier_time;     /* Time within one PWM period [0..PWM_PERIOD_SEC] */
    float carrier_value;    /* Instantaneous triangle carrier [0..VDC/2] */
    uint32_t sample_count;  /* Counter for carrier ramp */
} PWMModulator;

/* Duty cycle output */
typedef struct {
    float duty_a;  /* Phase A duty cycle [0..PWM_MAX_DUTY] */
    float duty_b;  /* Phase B duty cycle [0..PWM_MAX_DUTY] */
    float duty_c;  /* Phase C duty cycle [0..PWM_MAX_DUTY] */
} PWMDutyCycles;

/* Function prototypes */
void pwm_modulator_init(PWMModulator *pwm);

/*
 * Generate SPWM duty cycles from reference voltages
 * 
 * Input:
 *   pwm - PWM modulator state
 *   v_ref_a, v_ref_b, v_ref_c - Reference voltages [V]
 *   dt - Time step [seconds]
 * 
 * Output:
 *   duty - Duty cycles [0..PWM_MAX_DUTY]
 * 
 * Algorithm:
 *   1. Generate triangle carrier ramp
 *   2. Compare reference vs carrier
 *   3. Output duty = PWM_MAX_DUTY * (1 + v_ref / VDC) / 2
 */
PWMDutyCycles pwm_modulator_step(PWMModulator *pwm, float v_ref_a, float v_ref_b, float v_ref_c, float dt);

#endif /* PWM_MODULATOR_H */
