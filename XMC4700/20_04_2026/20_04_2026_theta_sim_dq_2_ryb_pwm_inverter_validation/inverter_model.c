/*
 * inverter_model.c
 * 
 * 3-Phase IGBT Inverter Model Implementation
 * Ported from Simulink (17-04-2026)
 */

#include "inverter_model.h"

/*
 * Initialize inverter model
 */
void inverter_init(InverterModel *inv, float vdc) {
    inv->vdc = vdc;
    inv->out.vrn = 0.0f;
    inv->out.vyn = 0.0f;
    inv->out.vbn = 0.0f;
}

/*
 * Convert PWM duty cycles [0..6000 counts] to phase-to-neutral voltages
 * 
 * Equations:
 * 1. Terminal-to-Midpoint (duty in counts):
 *    Vro = (Vdc/2) * (sw_1 - sw_4)
 *    where (sw_1 - sw_4) = (2 * duty_a / PWM_MAX - 1)
 *    
 * 2. Average voltage (common mode):
 *    V_avg = (Vro + Vyo + Vbo) / 3
 *    
 * 3. Phase-to-Neutral:
 *    Vn = Vro - V_avg
 *    Vyn = Vyo - V_avg
 *    Vbn = Vbo - V_avg
 */
void inverter_pwm_to_voltage(InverterModel *inv, float d_a, float d_b, float d_c) {
    /* Normalize duty cycle to [-1, 1] range representing switch state */
    /* duty in counts [0..6000] -> map to [-1, 1] where 3000 is center (50%) */
    float sa = (2.0f * d_a / PWM_MAX_DUTY) - 1.0f;  /* -1 to +1 */
    float sb = (2.0f * d_b / PWM_MAX_DUTY) - 1.0f;
    float sc = (2.0f * d_c / PWM_MAX_DUTY) - 1.0f;
    
    /* Terminal-to-Midpoint voltages (half DC bus) */
    float vdc_2 = inv->vdc / 2.0f;
    float vro = vdc_2 * sa;    /* Phase A relative to midpoint */
    float vyo = vdc_2 * sb;    /* Phase B relative to midpoint */
    float vbo = vdc_2 * sc;    /* Phase C relative to midpoint */
    
    /* Average voltage (common mode) */
    float v_avg = (vro + vyo + vbo) / 3.0f;
    
    /* Phase-to-Neutral voltages (remove common mode) */
    inv->out.vrn = vro - v_avg;
    inv->out.vyn = vyo - v_avg;
    inv->out.vbn = vbo - v_avg;
}

/*
 * Simplified switch interface: 3 binary switches (one per leg)
 * sw = 1: top switch ON (pole = +Vdc/2)
 * sw = 0: bottom switch ON (pole = -Vdc/2)
 */
void inverter_switch_to_voltage(InverterModel *inv, uint8_t sw_a, uint8_t sw_b, uint8_t sw_c) {
    /* Compute effective switch states: 1 = +Vdc/2, 0 = -Vdc/2 */
    float sa = (sw_a == 1) ? 1.0f : -1.0f;  /* Top ON → +1, Bottom ON → -1 */
    float sb = (sw_b == 1) ? 1.0f : -1.0f;
    float sc = (sw_c == 1) ? 1.0f : -1.0f;
    
    /* Terminal-to-Midpoint voltages (pole voltages) */
    float vdc_2 = inv->vdc / 2.0f;
    float vro = vdc_2 * sa;    /* +Vdc/2 or -Vdc/2 */
    float vyo = vdc_2 * sb;
    float vbo = vdc_2 * sc;
    
    /* Average voltage (common mode) */
    float v_avg = (vro + vyo + vbo) / 3.0f;
    
    /* Phase-to-Neutral voltages (remove common mode) */
    inv->out.vrn = vro - v_avg;
    inv->out.vyn = vyo - v_avg;
    inv->out.vbn = vbo - v_avg;
}
