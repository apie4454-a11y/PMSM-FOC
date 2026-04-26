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
/* PWM_VDC defined in motor_parameters.h to avoid duplicate definitions */

/* PWM modulator state */
typedef struct {
    float carrier_time;     /* Time within one PWM period [0..PWM_PERIOD_SEC] */
    float carrier_value;    /* Instantaneous triangle carrier [0..VDC/2] */
    uint32_t sample_count;  /* Counter for carrier ramp */
} PWMModulator;

/* PWM switch states output (discrete 0 or 1) */
typedef struct {
    uint8_t top_a;  /* Phase A: 1 = top switch ON, 0 = bottom switch ON */
    uint8_t top_b;  /* Phase B: 1 = top switch ON, 0 = bottom switch ON */
    uint8_t top_c;  /* Phase C: 1 = top switch ON, 0 = bottom switch ON */
} PWMSwitchStates;

/* Function prototypes */
void pwm_modulator_init(PWMModulator *pwm);

/*
 * Generate SPWM switch states from reference voltages
 * 
 * Input:
 *   pwm - PWM modulator state
 *   v_ref_a, v_ref_b, v_ref_c - Reference voltages (normalized [-1, +1])
 *   dt - Time step [seconds]
 * 
 * Output:
 *   PWMSwitchStates - Discrete switch states (top_a, top_b, top_c = 0 or 1)
 * 
 * Algorithm:
 *   1. Generate triangle carrier ramp [-1, +1]
 *   2. Compare reference vs carrier
 *   3. Output: 1 if ref > carrier (top ON), 0 if ref < carrier (bottom ON)
 */
PWMSwitchStates pwm_modulator_step(PWMModulator *pwm, float v_ref_a, float v_ref_b, float v_ref_c, float dt);

#endif /* PWM_MODULATOR_H */
