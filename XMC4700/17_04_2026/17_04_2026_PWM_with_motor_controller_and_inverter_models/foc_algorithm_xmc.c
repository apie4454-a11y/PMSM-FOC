/*
 * foc_algorithm_xmc.c
 * 
 * FOC Algorithm Implementation (C port from MATLAB)
 * Triple-cascade: Speed (2kHz) -> Torque -> Current (20kHz)
 * 
 * Port of foc_algorithm_sil.m with persistent state as struct
 */

#include "foc_algorithm_xmc.h"
#include <math.h>

/* Motor parameters (from Motor_Parameters.m) */
#define MOTOR_R              8.4f        /* d-q axis resistance [Ohm] */
#define MOTOR_L              0.003f      /* d-q axis inductance [H] */
#define MOTOR_PP             11          /* Pole pairs */

/* PI Gains (physics-based from Motor_Parameters.m) */
/* Speed loop @ 2 kHz */
#define SPEED_KP             0.0004732f
#define SPEED_KI             0.056549f
#define SPEED_TS             0.0005f     /* 1/2000 Hz */
#define SPEED_SAT_LOW        -0.05f
#define SPEED_SAT_HIGH       0.1f

/* Current loops @ 20 kHz */
#define CURRENT_KP           37.699f
#define CURRENT_KI           105560.0f
#define CURRENT_TS           5e-5f       /* 1/20000 Hz */
#define CURRENT_SAT_LOW      -29.5f
#define CURRENT_SAT_HIGH     29.5f

/*
 * Initialize FOC controller
 */
void foc_init(FOCController *foc) {
    /* Motor parameters */
    foc->R = MOTOR_R;
    foc->L = MOTOR_L;
    foc->pp = MOTOR_PP;
    
    /* Speed PI controller */
    foc->speed_pi.Kp = SPEED_KP;
    foc->speed_pi.Ki = SPEED_KI;
    foc->speed_pi.Ts = SPEED_TS;
    foc->speed_pi.i_state = 0.0f;
    foc->speed_pi.sat_low = SPEED_SAT_LOW;
    foc->speed_pi.sat_high = SPEED_SAT_HIGH;
    
    /* d-axis current PI controller */
    foc->id_pi.Kp = CURRENT_KP;
    foc->id_pi.Ki = CURRENT_KI;
    foc->id_pi.Ts = CURRENT_TS;
    foc->id_pi.i_state = 0.0f;
    foc->id_pi.sat_low = CURRENT_SAT_LOW;
    foc->id_pi.sat_high = CURRENT_SAT_HIGH;
    
    /* q-axis current PI controller */
    foc->iq_pi.Kp = CURRENT_KP;
    foc->iq_pi.Ki = CURRENT_KI;
    foc->iq_pi.Ts = CURRENT_TS;
    foc->iq_pi.i_state = 0.0f;
    foc->iq_pi.sat_low = CURRENT_SAT_LOW;
    foc->iq_pi.sat_high = CURRENT_SAT_HIGH;
    
    /* Debug */
    foc->speed_error = 0.0f;
    foc->te_ref = 0.0f;
}

/*
 * Helper: PI controller step
 */
static float pi_step(PIController *pi, float error) {
    /* Proportional term */
    float p_term = pi->Kp * error;
    
    /* Integral term with anti-windup */
    pi->i_state += pi->Ki * pi->Ts * error;
    if (pi->i_state > pi->sat_high) pi->i_state = pi->sat_high;
    if (pi->i_state < pi->sat_low) pi->i_state = pi->sat_low;
    
    /* Output with saturation */
    float output = p_term + pi->i_state;
    if (output > pi->sat_high) output = pi->sat_high;
    if (output < pi->sat_low) output = pi->sat_low;
    
    return output;
}

/*
 * Main FOC step (20 kHz)
 * 
 * Cascade sequence:
 * 1. Speed PI: RPM error -> torque reference
 * 2. Torque->Current: Te_ref -> iq_ref (id_ref = 0 MTPA)
 * 3. Current PIs: id/iq errors -> vd/vq voltage references
 * 4. Decoupling: Add cross-coupling and back-EMF
 */
void foc_step(FOCController *foc,
              float id_motor, float iq_motor,
              float motor_rpm, float omega_ele,
              float speed_ref, float max_flux,
              float *vd_ref, float *vq_ref) {
    
    /* ===== STEP 1: SPEED CONTROL LOOP ===== */
    float rpm_error = speed_ref - motor_rpm;
    float rpm_error_rad_s = rpm_error * (2.0f * 3.14159265f / 60.0f);
    
    float te_ref = pi_step(&foc->speed_pi, rpm_error_rad_s);
    foc->speed_error = rpm_error;
    foc->te_ref = te_ref;
    
    /* ===== STEP 2: TORQUE -> CURRENT REFERENCE ===== */
    /* Te = 1.5 * pp * max_flux * iq  =>  iq_ref = Te / (1.5 * pp * max_flux) */
    float iq_ref = te_ref / (1.5f * foc->pp * max_flux);
    float id_ref = 0.0f;  /* MTPA: keep id = 0 */
    
    /* ===== STEP 3: d-AXIS CURRENT CONTROL ===== */
    float id_error = id_ref - id_motor;
    float vd_star = pi_step(&foc->id_pi, id_error);
    
    /* d-axis decoupling: subtract cross-coupling */
    /* vd_decouple = omega_ele * L * iq_motor */
    float vd_decouple = omega_ele * foc->L * iq_motor;
    float vd_final = vd_star - vd_decouple;
    
    /* Saturate */
    if (vd_final > foc->id_pi.sat_high) vd_final = foc->id_pi.sat_high;
    if (vd_final < foc->id_pi.sat_low) vd_final = foc->id_pi.sat_low;
    
    *vd_ref = vd_final;
    
    /* ===== STEP 4: q-AXIS CURRENT CONTROL ===== */
    float iq_error = iq_ref - iq_motor;
    float vq_star = pi_step(&foc->iq_pi, iq_error);
    
    /* q-axis decoupling: add back-EMF and cross-coupling */
    /* vq_decouple = omega_ele * L * id_motor + omega_ele * max_flux (back-EMF) */
    float vq_decouple = omega_ele * foc->L * id_motor + omega_ele * max_flux;
    float vq_final = vq_star + vq_decouple;
    
    /* Saturate */
    if (vq_final > foc->iq_pi.sat_high) vq_final = foc->iq_pi.sat_high;
    if (vq_final < foc->iq_pi.sat_low) vq_final = foc->iq_pi.sat_low;
    
    *vq_ref = vq_final;
}

/*
 * Set speed reference (can be called at lower rate)
 */
void foc_set_speed_ref(FOCController *foc, float speed_ref) {
    /* Speed reference is read directly in foc_step(), so no storage needed */
    (void)foc;
    (void)speed_ref;
}

/*
 * Reset all integrator states
 */
void foc_reset(FOCController *foc) {
    foc->speed_pi.i_state = 0.0f;
    foc->id_pi.i_state = 0.0f;
    foc->iq_pi.i_state = 0.0f;
    foc->speed_error = 0.0f;
    foc->te_ref = 0.0f;
}
