/*
 * motor_model.h
 * 
 * dq-axis PMSM Motor Model
 * Ported from Simulink to C (17-04-2026)
 * 
 * Solves dq motor equations:
 * L*di_d/dt = v_d - R*i_d + omega_ele*L*i_q
 * L*di_q/dt = v_q - R*i_q - omega_ele*max_flux
 * T_e = 1.5*pp*max_flux*i_q
 * J*domega_mech/dt = T_e - B*omega_mech - T_load
 * omega_ele = pp * omega_mech
 * theta_ele = integral(omega_ele)
 */

#ifndef MOTOR_MODEL_H
#define MOTOR_MODEL_H

#include <stdint.h>
#include <math.h>
#include "motor_parameters.h"  /* All motor constants defined here */

/* Motor state structure */
typedef struct {
    float id;              /* d-axis current [A] */
    float iq;              /* q-axis current [A] */
    float omega_mech;      /* Mechanical speed [rad/s] */
    float theta_ele;       /* Electrical rotor angle [rad] */
    float max_flux;        /* Back-EMF constant [Wb] */
} MotorState;

/* Motor model instance */
typedef struct {
    MotorState state;
    float T_load;          /* Load torque [N*m] (set by user) */
    float Ts;              /* Timestep [s] - typically 5e-7 for fine integration */
    
    /* Debug outputs */
    float T_e;             /* Electrical torque [N*m] */
    float omega_ele;       /* Electrical speed [rad/s] */
} MotorModel;

/* Function prototypes */
void motor_init(MotorModel *m, float max_flux, float Ts);
void motor_step(MotorModel *m, float vd, float vq);

/* Inline helper: Convert mechanical speed to electrical */
static inline float motor_mech_to_ele(float omega_mech) {
    return MOTOR_PP * omega_mech;
}

/* Inline helper: Convert mechanical speed to RPM */
static inline float motor_rad_to_rpm(float omega_mech) {
    return omega_mech * 60.0f / (2.0f * 3.14159265f);
}

/* Inline helper: Convert RPM to mechanical speed */
static inline float motor_rpm_to_rad(float rpm) {
    return rpm * 2.0f * 3.14159265f / 60.0f;
}

#endif /* MOTOR_MODEL_H */
