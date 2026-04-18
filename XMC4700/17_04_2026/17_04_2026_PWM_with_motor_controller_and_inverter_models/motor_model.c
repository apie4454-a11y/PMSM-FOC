/*
 * motor_model.c
 * 
 * dq-axis PMSM Motor Model Implementation
 * Ported from Simulink (17-04-2026)
 * 
 * Forward Euler integration with fine timestep (5e-7 s)
 */

#include "motor_model.h"
#include <math.h>

/*
 * Initialize motor model
 */
void motor_init(MotorModel *m, float max_flux, float Ts) {
    m->state.id = 0.0f;
    m->state.iq = 0.0f;
    m->state.omega_mech = 0.0f;
    m->state.theta_ele = 0.0f;
    m->state.max_flux = max_flux;
    
    m->T_load = 0.0f;           /* No load by default */
    m->Ts = Ts;                 /* Fine timestep */
    m->T_e = 0.0f;
    m->omega_ele = 0.0f;
}

/*
 * Single integration step of motor dynamics
 * Inputs: vd, vq (dq voltages from inverter)
 * 
 * Equations (Forward Euler):
 * di_d/dt = (v_d - R*i_d + omega_ele*L*i_q) / L
 * di_q/dt = (v_q - R*i_q - omega_ele*max_flux) / L
 * T_e = 1.5 * pp * max_flux * i_q
 * domega_mech/dt = (T_e - B*omega_mech - T_load) / J
 * dtheta_ele/dt = omega_ele = pp * omega_mech
 */
void motor_step(MotorModel *m, float vd, float vq) {
    float id = m->state.id;
    float iq = m->state.iq;
    float omega_mech = m->state.omega_mech;
    float omega_ele = motor_mech_to_ele(omega_mech);
    
    /* Store for debug */
    m->omega_ele = omega_ele;
    
    /* ========== d-axis current dynamics ========== */
    /* L*di_d/dt = v_d - R*i_d + omega_ele*L*i_q */
    float di_d_dt = (vd - MOTOR_R * id + omega_ele * MOTOR_L * iq) / MOTOR_L;
    float id_new = id + di_d_dt * m->Ts;
    
    /* ========== q-axis current dynamics ========== */
    /* L*di_q/dt = v_q - R*i_q - omega_ele*max_flux */
    float di_q_dt = (vq - MOTOR_R * iq - omega_ele * m->state.max_flux) / MOTOR_L;
    float iq_new = iq + di_q_dt * m->Ts;
    
    /* ========== Torque generation ========== */
    /* T_e = 1.5 * pp * max_flux * i_q */
    float T_e = 1.5f * MOTOR_PP * m->state.max_flux * iq_new;
    m->T_e = T_e;
    
    /* ========== Mechanical dynamics ========== */
    /* J*domega_mech/dt = T_e - B*omega_mech - T_load */
    float domega_mech_dt = (T_e - MOTOR_B * omega_mech - m->T_load) / MOTOR_J;
    float omega_mech_new = omega_mech + domega_mech_dt * m->Ts;
    
    /* ========== Rotor angle integration ========== */
    /* dtheta_ele/dt = omega_ele = pp * omega_mech */
    float theta_ele_new = m->state.theta_ele + omega_ele * m->Ts;
    
    /* Wrap theta to [0, 2*pi) */
    const float TWO_PI = 2.0f * 3.14159265f;
    while (theta_ele_new >= TWO_PI) {
        theta_ele_new -= TWO_PI;
    }
    while (theta_ele_new < 0.0f) {
        theta_ele_new += TWO_PI;
    }
    
    /* ========== Update state ========== */
    m->state.id = id_new;
    m->state.iq = iq_new;
    m->state.omega_mech = omega_mech_new;
    m->state.theta_ele = theta_ele_new;
}
