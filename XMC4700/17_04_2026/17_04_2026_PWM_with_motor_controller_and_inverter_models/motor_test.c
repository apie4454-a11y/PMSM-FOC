/*
 * motor_test.c
 * 
 * Motor Model Unit Test
 * Applies known dq voltage input and logs motor dynamics
 * Use output to compare against MATLAB SIL predictions
 */

#include "motor_model.h"
#include <stdio.h>

/* RTT support */
#include "RTT/RTT/SEGGER_RTT.h"
#include "RTT/Config/SEGGER_RTT_Conf.h"

/*
 * Motor Model Test
 * Apply fixed dq voltage, log resulting speed/current/torque
 */
void motor_model_rtt_test() {
    MotorModel motor;  /* Use MotorModel, not MotorState */
    motor_init(&motor, 0.00614f, 0.001f);  /* lambda_f=0.00614, dt=1ms */

    // RTT header
    SEGGER_RTT_WriteString(0, "MOTOR_TEST,step,time_ms,vd,vq,id,iq,omega_mech,omega_ele,te,theta_ele\n");

    /* Test case: constant vd=0, vq=10V for 2 seconds (2000 steps @ 1kHz) */
    float vd_ref = 0.0f;
    float vq_ref = 10.0f;
    const int NUM_STEPS = 2000;  /* 2 seconds at 1kHz */

    for (int step = 0; step < NUM_STEPS; step++) {
        float time_ms = step * 1.0f;  /* 1ms per step */

        /* Step motor model */
        motor_step(&motor, vd_ref, vq_ref);

        /* Log state */
        char buf[256];
        sprintf(buf, "%d,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
                step, time_ms, vd_ref, vq_ref,
                motor.state.id, motor.state.iq,
                motor.state.omega_mech, motor.state.omega_mech * 11.0f,  /* ele = mech * pp */
                motor.T_e, motor.state.theta_ele);
        SEGGER_RTT_WriteString(0, buf);

        /* Yield every 100 steps to avoid blocking */
        if ((step % 100) == 0) {
            SEGGER_RTT_WriteString(0, "");  /* Allow RTT to flush */
        }
    }

    SEGGER_RTT_WriteString(0, "MOTOR_TEST_COMPLETE\n");
}
