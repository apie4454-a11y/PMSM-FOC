/*
 * motor_parameters.h
 *
 * Centralized motor parameters for iPower GM3506 Gimbal Motor
 * Ported from MATLAB Motor_Parameters.m (25-04-2026)
 * 
 * All motor constants are defined here - DO NOT duplicate in other files
 * Include this header in all files that need motor parameters
 */

#ifndef MOTOR_PARAMETERS_H
#define MOTOR_PARAMETERS_H

/* ========== MOTOR ELECTRICAL PARAMETERS ========== */
/* Derived from Motor_Parameters.m */

#define MOTOR_R              8.4f       /* Phase resistance [Ohm] (d-q equivalent) */
#define MOTOR_L              0.003f     /* Phase inductance [H] (d-q equivalent) */
#define MOTOR_PP             11         /* Pole pairs (22P motor configuration) */
#define MOTOR_KV             141.4f     /* Back-EMF constant [RPM/V] */
#define MOTOR_LAMBDA_F       0.00614f   /* Flux linkage [Wb] = Ke_total / pp */

/* ========== MOTOR MECHANICAL PARAMETERS ========== */
/* Calculated from iFligh GM3506 datasheet via power analysis (09-04-2026) */

#define MOTOR_J              3.8e-6f    /* Rotor inertia [kg·m²] */
#define MOTOR_B              4.5e-5f    /* Viscous damping [N·m·s/rad] */
#define MOTOR_TF_COULOMB     0.001f     /* Coulomb friction torque [N·m] */

/* ========== INVERTER & PWM PARAMETERS ========== */
#define INVERTER_FSW         20e3f      /* PWM switching frequency [Hz] */
#define INVERTER_TS          5e-5f      /* PWM period [s] = 1/20kHz */
#define PWM_VDC              59.0f      /* DC-link voltage [V] aligned with Motor_Parameters.m */

/* ========== CONTROL LOOP PARAMETERS ========== */
/* Gains tuned for bandwidth matching (from Motor_Parameters.m) */

/* d-axis current controller (20 kHz) */
#define CURRENT_KP_D         37.699f    /* Proportional gain: L * omega_bw */
#define CURRENT_KI_D         105560.0f  /* Integral gain: R * omega_bw */
#define CURRENT_TS           5e-5f      /* Sampling time [s] */
#define CURRENT_SAT_LOW      -29.5f     /* Saturation limit [V] */
#define CURRENT_SAT_HIGH     29.5f      /* Saturation limit [V] */

/* q-axis current controller (20 kHz, same as d-axis for PMSM) */
#define CURRENT_KP_Q         37.699f
#define CURRENT_KI_Q         105560.0f
#define CURRENT_Q_SAT_LOW    -29.5f
#define CURRENT_Q_SAT_HIGH   29.5f

/* Speed controller (2 kHz decimation from 20 kHz) */
#define SPEED_KP             0.004775f  /* Proportional gain: J * omega_bw */
#define SPEED_KI             0.056549f  /* Integral gain: B * omega_bw */
#define SPEED_TS             0.0005f    /* Sampling time [s] = 1/2kHz */
#define SPEED_SAT_LOW        -0.05f     /* Saturation limit [Nm] */
#define SPEED_SAT_HIGH       0.1f       /* Saturation limit [Nm] */

/* ========== REFERENCE CONTROL PARAMETERS ========== */
#define MOTOR_SPEED_HZ       50.0f      /* Reference motor speed [Hz] (couples to frequency) */
#define MOTOR_SPEED_RPM      (MOTOR_SPEED_HZ * 60.0f)  /* Reference speed [RPM] */

#endif /* MOTOR_PARAMETERS_H */
