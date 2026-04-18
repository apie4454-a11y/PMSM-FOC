/*
 * foc_algorithm_xmc.h
 * 
 * FOC Algorithm (C port from foc_algorithm_sil.m)
 * For XMC4700 embedded execution (17-04-2026)
 * 
 * Triple-cascade control:
 * - Speed loop @ 2 kHz (calculates torque reference)
 * - Torque loop @ 2 kHz (calculates current reference from torque)
 * - Current loops (d,q) @ 20 kHz (calculates voltage reference)
 */

#ifndef FOC_ALGORITHM_XMC_H
#define FOC_ALGORITHM_XMC_H

#include <stdint.h>

/* ========== PI CONTROLLER STATE ========== */
typedef struct {
    float Kp;              /* Proportional gain */
    float Ki;              /* Integral gain */
    float Ts;              /* Timestep [s] */
    float i_state;         /* Integrator state */
    float sat_low;         /* Anti-windup lower limit */
    float sat_high;        /* Anti-windup upper limit */
} PIController;

/* ========== FOC STATE ========== */
typedef struct {
    /* Motor parameters */
    float R;               /* Resistance [Ohm] */
    float L;               /* Inductance [H] */
    uint16_t pp;           /* Pole pairs */
    
    /* Speed PI controller */
    PIController speed_pi;
    
    /* Current PI controllers */
    PIController id_pi;
    PIController iq_pi;
    
    /* Speed loop decimation (CRITICAL FIX - 17-04-2026) */
    /* Speed PI must run at 2 kHz, but foc_step() called at 20 kHz */
    /* Counter-based decimation: execute speed PI every 10 calls */
    uint16_t speed_decimator;  /* Counter 0-9, execute speed PI when >= 10 */
    float iq_ref_prev;         /* Hold iq_ref during skip cycles */
    float te_ref_prev;         /* Hold te_ref during skip cycles */
    
    /* Debug outputs */
    float speed_error;
    float te_ref;
} FOCController;

/* ========== FUNCTION PROTOTYPES ========== */

/*
 * Initialize FOC controller
 * Sets default gains and PI states to zero
 */
void foc_init(FOCController *foc);

/*
 * Main FOC step (call at 20 kHz)
 * 
 * Inputs:
 *   id_motor, iq_motor   - Current feedback [A]
 *   motor_rpm            - Speed feedback [RPM]
 *   omega_ele            - Electrical speed [rad/s]
 *   speed_ref            - Speed reference [RPM]
 *   max_flux             - Back-EMF constant [Wb]
 * 
 * Outputs:
 *   vd_ref, vq_ref       - Voltage references [V]
 */
void foc_step(FOCController *foc,
              float id_motor, float iq_motor,
              float motor_rpm, float omega_ele,
              float speed_ref, float max_flux,
              float *vd_ref, float *vq_ref);

/*
 * Set speed reference (can be updated at any rate < 20 kHz)
 */
void foc_set_speed_ref(FOCController *foc, float speed_ref);

/*
 * Reset all integrator states (for safe startup)
 */
void foc_reset(FOCController *foc);

#endif /* FOC_ALGORITHM_XMC_H */
