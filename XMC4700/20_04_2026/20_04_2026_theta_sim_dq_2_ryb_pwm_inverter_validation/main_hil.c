/*
 * main_hil.c
 * 
 * Hardware-in-Loop Implementation
 * FOC + Motor + Inverter closed-loop on XMC4700
 */

#include "main_hil.h"
#include <math.h>

/*
 * Initialize HIL system
 */
void hil_init(HIL_State *hil, float vdc, float max_flux) {
    hil->speed_ref = 0.0f;
    hil->max_flux = max_flux;
    hil->duty_a = 3000.0f;  /* Start at 50% duty */
    hil->duty_b = 3000.0f;
    hil->duty_c = 3000.0f;
    
    /* Initialize FOC controller */
    foc_init(&hil->foc);
    
    /* Initialize inverter */
    inverter_init(&hil->inverter, vdc);
    
    /* Compute inverter voltages from initial duty cycles */
    inverter_pwm_to_voltage(&hil->inverter, hil->duty_a, hil->duty_b, hil->duty_c);
    
    /* Initialize motor */
    motor_init(&hil->motor, max_flux, MOTOR_TIMESTEP);
    
    hil->id_foc = 0.0f;
    hil->iq_foc = 0.0f;
    hil->vd_ref = 0.0f;
    hil->vq_ref = 0.0f;
    
    hil->speed_rpm = 0.0f;
    hil->omega_mech = 0.0f;
    
    hil->foc_counter = 0;
    hil->motor_counter = 0;
    
    hil->te_actual = 0.0f;
    hil->theta_ele = 0.0f;
}

/*
 * Main FOC control step (20 kHz)
 * 
 * Sequence:
 * 1. Run motor model MOTOR_STEPS_PER_FOC times (fine integration)
 * 2. Extract motor feedback (id, iq, speed)
 * 3. Call FOC algorithm
 * 4. Inverter converts vd,vq -> abc via Park/Clarke inverse
 * 5. Generate PWM duty cycles
 * 6. Every 10 steps: update speed controller
 */
void hil_step_foc(HIL_State *hil) {
    /* ===== INVERTER UPDATE (convert previous FOC outputs to voltages) ===== */
    /* MUST be before motor steps so motor sees latest voltages */
    inverter_pwm_to_voltage(&hil->inverter, hil->duty_a, hil->duty_b, hil->duty_c);
    
    /* ===== MOTOR INTEGRATION (fine timestep) ===== */
    for (uint32_t i = 0; i < MOTOR_STEPS_PER_FOC; i++) {
        /* Get dq voltages from inverter output */
        /* Inverter outputs RYB, need to transform to dq */
        Park_Output dq_volt = abc_to_dq(
            hil->inverter.out.vrn,
            hil->inverter.out.vyn,
            hil->inverter.out.vbn,
            hil->motor.state.theta_ele
        );
        
        /* Step motor model with dq voltages */
        motor_step(&hil->motor, dq_volt.d, dq_volt.q);
    }
    
    /* ===== FEEDBACK EXTRACTION ===== */
    hil->id_foc = hil->motor.state.id;
    hil->iq_foc = hil->motor.state.iq;
    hil->omega_mech = hil->motor.state.omega_mech;
    hil->speed_rpm = motor_rad_to_rpm(hil->omega_mech);
    hil->te_actual = hil->motor.T_e;
    hil->theta_ele = hil->motor.state.theta_ele;
    
    /* ===== FOC CONTROLLER (20 kHz current loop) ===== */
    float omega_ele = motor_mech_to_ele(hil->omega_mech);
    
    foc_step(
        &hil->foc,
        hil->id_foc,           /* d-current feedback */
        hil->iq_foc,           /* q-current feedback */
        hil->speed_rpm,        /* speed feedback */
        omega_ele,             /* electrical speed */
        hil->speed_ref,        /* speed reference */
        hil->max_flux,         /* back-EMF constant */
        &hil->vd_ref,          /* output: d-voltage ref */
        &hil->vq_ref           /* output: q-voltage ref */
    );
    
    /* ===== INVERTER CONTROL (PWM mapping) ===== */
    /* Convert dq voltages to RYB via proper Park/Clarke inverse */
    RYB_Output ryb_volt = dq_to_ryb(hil->vd_ref, hil->vq_ref, hil->motor.state.theta_ele);
    
    /* Map RYB voltages to duty cycles [0, 6000] */
    const float DUTY_CENTER = 3000.0f;
    const float DUTY_MAX_SWING = 3000.0f;
    const float VDC_2 = 29.7f;  /* Vdc/2 = 59.4/2 */
    
    float duty_r_f = DUTY_CENTER + (ryb_volt.vr / VDC_2) * DUTY_MAX_SWING;
    float duty_y_f = DUTY_CENTER + (ryb_volt.vy / VDC_2) * DUTY_MAX_SWING;
    float duty_b_f = DUTY_CENTER + (ryb_volt.vb / VDC_2) * DUTY_MAX_SWING;
    
    /* Clamp to valid range [0, 6000] */
    if (duty_r_f < 0.0f) duty_r_f = 0.0f;
    if (duty_r_f > 6000.0f) duty_r_f = 6000.0f;
    if (duty_y_f < 0.0f) duty_y_f = 0.0f;
    if (duty_y_f > 6000.0f) duty_y_f = 6000.0f;
    if (duty_b_f < 0.0f) duty_b_f = 0.0f;
    if (duty_b_f > 6000.0f) duty_b_f = 6000.0f;
    
    /* Store duty cycles for NEXT cycle's inverter update */
    hil->duty_a = duty_r_f;
    hil->duty_b = duty_y_f;
    hil->duty_c = duty_b_f;
    
    /* ===== SPEED LOOP DECIMATION (2 kHz) ===== */
    hil->foc_counter++;
    if (hil->foc_counter >= 10) {  /* 20 kHz / 10 = 2 kHz */
        hil->foc_counter = 0;
        /* Speed controller would update here if needed */
        /* Currently handled inside foc_step */
    }
}

/*
 * Set load torque (low-rate, can be called from UART or main loop)
 */
void hil_set_load_torque(HIL_State *hil, float T_load) {
    hil->motor.T_load = T_load;
}

/*
 * Set speed reference (low-rate)
 */
void hil_set_speed_ref(HIL_State *hil, float speed_ref_rpm) {
    hil->speed_ref = speed_ref_rpm;
}

/*
 * Telemetry getters (safe to call anytime)
 */
float hil_get_speed_rpm(const HIL_State *hil) {
    return hil->speed_rpm;
}

float hil_get_current_d(const HIL_State *hil) {
    return hil->id_foc;
}

float hil_get_current_q(const HIL_State *hil) {
    return hil->iq_foc;
}

float hil_get_torque(const HIL_State *hil) {
    return hil->te_actual;
}

float hil_get_rotor_angle(const HIL_State *hil) {
    return hil->theta_ele;
}
