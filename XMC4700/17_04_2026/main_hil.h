/*
 * main_hil.h
 * 
 * Hardware-in-Loop Simulation on XMC4700
 * Complete FOC + Motor + Inverter closed-loop simulation
 * 
 * Architecture:
 * - FOC Controller @ 20 kHz (current loop)
 * - Speed Controller @ 2 kHz (cascade from FOC)
 * - Motor Model @ 5e-7 s (fine integration for accuracy)
 * - Inverter Model (duty cycles → abc voltages)
 * - Clarke/Park transforms (abc ↔ dq)
 * 
 * Date: 17-04-2026
 */

#ifndef MAIN_HIL_H
#define MAIN_HIL_H

#include <stdint.h>
#include <math.h>

#include "motor_model.h"
#include "inverter_model.h"
#include "transforms.h"

/* ========== SIMULATION CONFIGURATION ========== */

#define MOTOR_TIMESTEP       5e-7f           /* Fine motor integration timestep [s] */
#define FOC_TIMESTEP         5e-5f           /* 20 kHz current loop [1/20000] */
#define SPEED_TIMESTEP       5e-4f           /* 2 kHz speed loop [1/2000] */

/* Calculate how many motor steps per FOC step */
#define MOTOR_STEPS_PER_FOC  ((uint32_t)(FOC_TIMESTEP / MOTOR_TIMESTEP))  /* Should be ~100 */

/* ========== APPLICATION STATE ========== */

typedef struct {
    /* ===== Controllers ===== */
    float speed_ref;           /* Speed reference [RPM] */
    float max_flux;            /* Back-EMF constant [Wb] */
    
    /* ===== Inverter ===== */
    InverterModel inverter;
    float duty_a;              /* PWM duty cycles [0..6000] */
    float duty_b;
    float duty_c;
    
    /* ===== Motor ===== */
    MotorModel motor;
    
    /* ===== Current Loop Buffers (20 kHz) ===== */
    float id_foc;              /* Current feedback from motor [A] */
    float iq_foc;
    float vd_ref;              /* Voltage reference from FOC [V] */
    float vq_ref;
    
    /* ===== Speed Loop Buffers (2 kHz) ===== */
    float speed_rpm;           /* Mechanical speed [RPM] */
    float omega_mech;          /* Mechanical speed [rad/s] */
    
    /* ===== Counters for multi-rate ===== */
    uint32_t foc_counter;      /* Counter for speed loop decimation */
    uint32_t motor_counter;    /* Counter for motor fine integration */
    
    /* ===== Debug/Telemetry ===== */
    float te_actual;           /* Actual torque from motor */
    float theta_ele;           /* Rotor electrical angle */
    
} HIL_State;

/* ========== FUNCTION PROTOTYPES ========== */

/*
 * Initialize HIL simulation
 */
void hil_init(HIL_State *hil, float vdc, float max_flux);

/*
 * Main HIL step (call from main loop or timer interrupt)
 * 
 * Timing hierarchy:
 * - FOC step (20 kHz) calls motor steps (5e-7 s) MOTOR_STEPS_PER_FOC times
 * - Speed controller runs every 10 FOC steps (2 kHz)
 * - Inverter/Clarke/Park run at FOC rate
 * 
 * Call this at 20 kHz from XMC PWM timer or SysTick
 */
void hil_step_foc(HIL_State *hil);

/*
 * Update motor model with fine timestep
 * Called multiple times per FOC step to maintain numerical stability
 */
void hil_step_motor(HIL_State *hil);

/*
 * Set load torque (for scenario simulation)
 * Can be updated at low rate (< 100 Hz)
 */
void hil_set_load_torque(HIL_State *hil, float T_load);

/*
 * Set speed reference
 * Can be updated at low rate (< 100 Hz)
 */
void hil_set_speed_ref(HIL_State *hil, float speed_ref_rpm);

/*
 * Read telemetry (non-blocking, safe to call anytime)
 */
float hil_get_speed_rpm(const HIL_State *hil);
float hil_get_current_d(const HIL_State *hil);
float hil_get_current_q(const HIL_State *hil);
float hil_get_torque(const HIL_State *hil);
float hil_get_rotor_angle(const HIL_State *hil);

#endif /* MAIN_HIL_H */
