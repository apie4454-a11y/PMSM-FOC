/*
 * inverter_model.h
 * 
 * 3-Phase IGBT Inverter Model (DC/2 configuration)
 * Ported from Simulink (17-04-2026)
 * 
 * Converts PWM duty cycles to three-phase voltages
 */

#ifndef INVERTER_MODEL_H
#define INVERTER_MODEL_H

#include <stdint.h>

/* Inverter configuration */
#define INVERTER_VDC     59.4f      /* DC bus voltage [V] (from Motor_Parameters) */
#define INVERTER_VDC_2   (INVERTER_VDC / 2.0f)  /* Half DC voltage */

/* PWM period for duty cycle mapping */
#define PWM_PERIOD_COUNTS  6000     /* From DAVE CCU8 configuration */
#define PWM_MAX_DUTY       6000.0f  /* Corresponds to 100% duty = Vdc/2 */

/* Inverter state structure */
typedef struct {
    float vrn;  /* Red phase neutral voltage [V] */
    float vyn;  /* Yellow phase neutral voltage [V] */
    float vbn;  /* Blue phase neutral voltage [V] */
} InverterOutput;

/* Inverter model instance */
typedef struct {
    float vdc;          /* DC bus voltage [V] */
    InverterOutput out; /* Output abc voltages */
} InverterModel;

/* Function prototypes */
void inverter_init(InverterModel *inv, float vdc);

/*
 * Convert PWM duty cycles to phase-to-neutral voltages
 * 
 * Inputs:
 *   inv  - Inverter model instance
 *   d_a, d_b, d_c - PWM duty cycles for phases [0..6000] or [0..1]
 * 
 * Equation (PWM counts -> voltage):
 *   Terminal-to-Midpoint: Vro = (Vdc/2) * (2*duty/PWM_MAX - 1)
 *   Average: V_avg = (Vro + Vyo + Vbo) / 3
 *   Phase-to-Neutral: Vn = Vro - V_avg
 */
void inverter_pwm_to_voltage(InverterModel *inv, float d_a, float d_b, float d_c);

/*
 * Convert discrete switch states to phase-to-neutral voltages (PREFERRED - 3-leg interface)
 * 
 * Inputs:
 *   inv - Inverter model instance
 *   sw_a, sw_b, sw_c - Switch states (1 = top ON/bottom OFF, 0 = bottom ON/top OFF)
 * 
 * Equation (discrete switch -> voltage):
 *   Pole voltage: V_pole = +Vdc/2 if switch=1, else -Vdc/2
 *   Average: V_avg = (Vro + Vyo + Vbo) / 3
 *   Phase-to-Neutral: Vn = Vro - V_avg
 */
void inverter_switch_to_voltage(InverterModel *inv, uint8_t sw_a, uint8_t sw_b, uint8_t sw_c);

#endif /* INVERTER_MODEL_H */
