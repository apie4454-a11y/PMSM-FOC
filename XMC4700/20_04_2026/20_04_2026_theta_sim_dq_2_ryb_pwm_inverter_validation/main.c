/*
 * main.c
 *
 * Hardware-in-Loop Control Firmware for XMC4700
 * FOC + Motor Model + Inverter Model
 * 
 * 20 kHz Control Loop: CCU8_0 PWM interrupt
 * 100 Hz Telemetry: SysTick interrupt
 * 
 * Author: Rizwan Sadik
 * Date: 17-04-2026
 */

#include <stdint.h>
#include "DAVE.h"
#include "main_hil.h"
#include <stdio.h>
#include <string.h>

/* RTT support */
#include "RTT/RTT/SEGGER_RTT.h"
#include "RTT/Config/SEGGER_RTT_Conf.h"

/* Motor and control parameters (CENTRALIZED) */
#include "motor_parameters.h"   /* All motor parameters defined here */

#include "inverter_test.h" // For inverter model test
#include "motor_test.h"    // For motor model test
#include "pwm_modulator.h"  // SPWM modulator
#include "inverter_model.h" // Inverter model
#include "motor_model.h"    // Motor dynamics (dq model) - includes motor_parameters.h
#include "transforms.h"     // Clarke and Park transforms

/* ========== SYSTEM CONSTANTS ========== */
#define VDC_NOMINAL  48.0f      /* DC bus voltage [V] */

/* ========== HIL STATE ========== */
HIL_State hil;  /* Global HIL state variable */

/* ========== PWM MODULATOR & INVERTER ========== */
PWMModulator pwm_mod;      /* SPWM modulator instance */
InverterModel inverter;    /* Inverter model instance */
MotorModel motor_model;    /* Motor dynamics (dq model) instance */

/* ========== TEST STATE MACHINE (RTT LOGGING CONTROL ONLY) ========== */
/* IMPORTANT: These variables control WHEN TO STOP RTT LOGGING, NOT motor model speed!
 * Motor model ALWAYS runs at 1 MHz regardless of test_steps_total.
 * In this version (20_04_2026): NO DECIMATION — logs every iteration (1 MHz sampling).
 * In newer version (25_04_2026): DECIMATED by 50× — logs every 50 iterations (20 kHz sampling).
 */
uint32_t test_step = 0;                                    /* CSV row counter (every iteration in this version) */
uint32_t test_steps_total = 200000;                        /* Target CSV rows to collect @ 1 MHz. When reached, stop logging. */
                                                           /* Motor will have run: test_steps_total × 1 µs = 200000 × 1e-6 = 0.2s */
uint32_t output_counter = 0;
uint32_t foc_cycle_counter = 0;
bool test_running = false;

/* Large CSV text RTT buffer on channel 0 */
static char rtt_text_buffer[204800];  /* 200 KB - enough for 200000 rows */

/* ========== UART COMMUNICATION ========== */
/* Empty handlers - DAVE expects these symbols (linker requirement) */
void UART_RX_Handler(void) {
    /* Not used in RTT-only mode */
}

void UART_TX_Handler(void) {
    /* Not used */
}

/* ========== MAIN ENTRY POINT ========== */
int main(void) {
    DAVE_STATUS_t status;
    
    /* Initialize RTT FIRST - before DAVE */
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigUpBuffer(0, "Terminal", rtt_text_buffer, sizeof(rtt_text_buffer), SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    SEGGER_RTT_WriteString(0, "\r\n=== XMC4700 FOC Motor Logger ===\r\n");
    
    /* Initialize DAVE APPs (PWM, timers) */
    status = DAVE_Init();
    
    if (status != DAVE_STATUS_SUCCESS) {
        XMC_DEBUG("DAVE initialization failed\n");
        while(1);
    }
    
    SEGGER_RTT_WriteString(0, "DAVE initialized successfully\r\n");
    
    /* Initialize HIL system */
    hil_init(&hil, VDC_NOMINAL, MOTOR_LAMBDA_F);
    SEGGER_RTT_WriteString(0, "HIL initialized\r\n");
    
    /* Initialize PWM modulator and inverter */
    pwm_modulator_init(&pwm_mod);
    inverter_init(&inverter, PWM_VDC);
    SEGGER_RTT_WriteString(0, "PWM modulator and inverter initialized\r\n");
    
    /* Initialize motor model (with dt = 1 µs for fine dynamics) */
    motor_init(&motor_model, MOTOR_LAMBDA_F, 1.0e-6f);
    SEGGER_RTT_WriteString(0, "Motor model initialized (dq dynamics, dt=1µs)\r\n");
    
    /* Reset carrier to start from -1.0 (minimum) */
    pwm_mod.carrier_time = 0.0f;
    pwm_mod.carrier_value = -1.0f;
    
    /* Set speed reference */
    hil_set_speed_ref(&hil, 1000.0f);  /* 1000 RPM */
    SEGGER_RTT_WriteString(0, "Speed ref set to 1000 RPM\r\n");
    
    /* Start autonomous test - RTT streaming only */
    test_running = true;
    test_step = 0;
    output_counter = 0;

    /* Print CSV header */
    SEGGER_RTT_WriteString(0, "\r\n====== CSV DATA START ======\r\n");
    SEGGER_RTT_WriteString(0, "RTT_LOGGING_RATE_HZ=1000000\r\n");  /* No decimation: logs every 1 MHz iteration */
    SEGGER_RTT_WriteString(0, "step,theta_sim,vd_ref,vq_ref,vr_ref,vy_ref,vb_ref,vr_mod,vy_mod,vb_mod,carrier,vr_sw,vy_sw,vb_sw\r\n");
    SEGGER_RTT_WriteString(0, "Starting stream...\r\n");

    /* ========== 1 MHz MAIN LOOP - MODULATOR + FOC ========== */
    
    /* CORE SETTING: dt_modulator defines the fundamental sampling timestep
     * 
     * Significance:
     * - Equivalent to MATLAB Simulink "Fixed-step solver" timestep (Ts in ode4/ode5)
     * - All system dynamics evolve by this timestep: theta, currents, speed, FOC calculations
     * - All control algorithms discretized using this dt
     * - PWM carrier updates every dt (1 µs → 1 MHz sampling = 50 samples per 20 kHz cycle)
     * 
     * Comparison to your MATLAB setup:
     * MATLAB (discrete ode4):        XMC4700 (discrete Euler):
     *   Solver: ode4 (RK4)       ←→  Solver: Euler (1st order)
     *   Ts = 5e-7 s (0.5 µs)     ←→  dt = 1.0 / 1000000 = 1e-6 s (1 µs)
     *   Sample rate: 2 MHz       ←→  Loop frequency: 1 MHz
     *   Freq: 2x finer           ←→  Freq: 0.5x (coarser but acceptable)
     * 
     * Your MATLAB was 2x more accurate (0.5 µs), but embedded is still good at 1 µs.
     * Both are discrete! Same conceptual framework, different numerical integration & dt.
     * 
     * Changing dt:
     * - Smaller dt → more accurate motor model, higher control bandwidth, more CPU load
     * - Larger dt → faster loops, less accurate, slower response
     * - Current: 1 µs is good balance (1 MHz = simple calculations on ARM Cortex-M4)
     * - Could reduce to 0.5 µs if CPU allows, would match MATLAB exactly
     */
    float dt_modulator = 1.0f / 1000000.0f;  /* 1 us - CORE TIMING UNIT for all dynamics */
    const float theta_freq_hz = 500.0f;
    const float theta_omega = 2.0f * 3.14159265f * theta_freq_hz;
    const float vd_ref = 0.0f;
    const float vq_ref = 10.0f;
    const float vdc_half = PWM_VDC / 2.0f;
    float theta_sim = 0.0f;
    
    while(1) {
        if (test_running) {
            /* ===== 1 MHz ITERATION - THETA + dq->RYB REFERENCE VOLTAGES ===== */
            RYB_Output ryb_ref = dq_to_ryb(vd_ref, vq_ref, theta_sim);
            float vr_mod = ryb_ref.vr / vdc_half;
            float vy_mod = ryb_ref.vy / vdc_half;
            float vb_mod = ryb_ref.vb / vdc_half;

            /* Run modulator so carrier/comparator and switched outputs are visible in CSV. */
            PWMSwitchStates switches = pwm_modulator_step(&pwm_mod, vr_mod, vy_mod, vb_mod, dt_modulator);
            float carrier = pwm_mod.carrier_value;
            inverter_switch_to_voltage(&inverter, switches.top_a, switches.top_b, switches.top_c);
            float vr_sw = inverter.out.vrn;
            float vy_sw = inverter.out.vyn;
            float vb_sw = inverter.out.vbn;

            test_step++;
            char csv_buf[384];
            snprintf(csv_buf, sizeof(csv_buf),
                "%lu,%.6f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f\r\n",
                test_step,
                theta_sim,
                vd_ref,
                vq_ref,
                ryb_ref.vr,
                ryb_ref.vy,
                ryb_ref.vb,
                vr_mod,
                vy_mod,
                vb_mod,
                carrier,
                vr_sw,
                vy_sw,
                vb_sw);
            SEGGER_RTT_WriteString(0, csv_buf);

            theta_sim += theta_omega * dt_modulator;
            while (theta_sim >= 2.0f * 3.14159265f) {
                theta_sim -= 2.0f * 3.14159265f;
            }

            if (test_step >= test_steps_total) {
                test_running = false;
                char complete_msg[64];
                snprintf(complete_msg, sizeof(complete_msg), "\r\n=== Complete: %lu samples ===\r\n", test_step);
                SEGGER_RTT_WriteString(0, complete_msg);
            }
        } else {
            hil_step_foc(&hil);
        }
    }
    
    return 0;
}
