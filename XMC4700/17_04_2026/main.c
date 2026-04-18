/*
 * main.c - XMC4700 HIL Integration with Dynamic PWM
 * 
 * 20 kHz FOC + Motor + Inverter closed-loop simulation
 * with PulseView oscilloscope output and dynamic duty cycles
 * 
 * Pin Configuration:
 *   P1.11 (CH1 Inverted) → PulseView channel (20 kHz PWM output)
 *   UART → Telemetry (speed, current, torque)
 * 
 * Date: 17-04-2026
 */

#include "DAVE.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "main_hil.h"
#include "foc_algorithm_xmc.h"

/* ========== GLOBAL STATE ========== */

HIL_State hil;

uint32_t system_time_ms = 0;
uint32_t foc_cycle_count = 0;

/* ========== PWM PERIOD (from DAVE config) ========== */
#define PWM_PERIOD_COUNTS  7200    /* 20 kHz: 144 MHz / 7200 = 20 kHz */
#define DUTY_CENTER        3600    /* 50% duty = 3600 / 7200 */

/* ========== TIMER ISR @ 20 kHz ========== */

/*
 * CCU8 Period Match ISR - Executes FOC control loop
 * 
 * This ISR is called at 20 kHz (every 50 μs)
 * 
 * Sequence:
 * 1. Execute FOC + Motor + Inverter loop (hil_step_foc)
 * 2. Update hardware PWM with new duty cycle
 * 3. Increment counters for lower-rate tasks
 */
void CCU8_0_IRQHandler(void) {
    /* ===== FOC + MOTOR + INVERTER STEP ===== */
    /* This does ALL the work:
     * - Motor model (100 sub-steps internally)
     * - FOC control (speed + current loops)
     * - Inverter model
     * - Feedback closes loop
     */
    hil_step_foc(&hil);
    
    /* ===== UPDATE HARDWARE PWM (TO OSCILLOSCOPE) ===== */
    /* hil.duty_a contains the calculated duty cycle from FOC
     * Range: [0 .. 6000] where 3000 = 50%, 0 = 0%, 6000 = 100%
     * 
     * These same duty values are ALSO used internally in hil_step_foc()
     * to generate the motor voltages via inverter_pwm_to_voltage()
     * 
     * So we get TWO things:
     * 1. Hardware output (P1.11) → oscilloscope sees the PWM
     * 2. Internal loopback → motor model sees it too
     */
    PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, (uint16_t)hil.duty_a);
    
    foc_cycle_count++;
}

/* ========== TIMER ISR @ 1 kHz (SysTick) ========== */

/*
 * Low-rate timer for:
 * - System time tracking
 * - Telemetry (decimated to 100 Hz)
 * - Scenario updates (speed reference, load)
 */
void SysTick_Handler(void) {
    static uint32_t telemetry_div = 0;
    
    system_time_ms += 1;
    telemetry_div++;
    
    if (telemetry_div >= 10) {  /* 100 Hz telemetry */
        telemetry_div = 0;
        
        /* ===== SCENARIO LOGIC ===== */
        
        /* Test 1: Step speed from 0 → 300 RPM at t=1s */
        if (system_time_ms >= 1000 && system_time_ms < 5000) {
            hil_set_speed_ref(&hil, 300.0f);
        }
        /* Test 2: Reduce to 100 RPM at t=5s */
        else if (system_time_ms >= 5000 && system_time_ms < 10000) {
            hil_set_speed_ref(&hil, 100.0f);
        }
        /* Test 3: Back to zero */
        else {
            hil_set_speed_ref(&hil, 0.0f);
        }
        
        /* Load disturbance at t=3s */
        if (system_time_ms >= 3000 && system_time_ms < 4000) {
            hil_set_load_torque(&hil, 0.015f);  /* 15 mNm */
        } else {
            hil_set_load_torque(&hil, 0.0f);
        }
        
        /* ===== TELEMETRY OUTPUT ===== */
        float speed_rpm = hil_get_speed_rpm(&hil);
        float id = hil_get_current_d(&hil);
        float iq = hil_get_current_q(&hil);
        float te = hil_get_torque(&hil);
        float theta = hil_get_rotor_angle(&hil);
        
        printf("[%5u ms] RPM:%7.2f  Id:%7.4f  Iq:%7.4f  Te:%7.5f  θ:%7.4f\r\n",
               system_time_ms, speed_rpm, id, iq, te, theta);
    }
}

/* ========== MAIN ========== */

int main(void) {
    /* Initialize DAVE (PWM_CCU8_0, UART, SysTick, etc.) */
    DAVE_STATUS_t dave_status = DAVE_Init();
    if (dave_status != DAVE_STATUS_SUCCESS) {
        while(1);  /* Halt on DAVE init failure */
    }
    
    /* ===== INITIALIZE HIL SYSTEM ===== */
    
    /* Calculate max flux from motor parameters */
    /* max_flux = pp * Kv * 2π / 60 */
    float motor_pp = 11.0f;
    float motor_kv = 141.4f;  /* iFligh GM3506 spec */
    float max_flux = motor_pp * motor_kv * 2.0f * 3.14159265f / 60.0f;
    
    printf("=== XMC4700 HIL STARTUP ===\r\n");
    printf("Motor: pp=%d, Kv=%.1f, max_flux=%.4f Wb\r\n", 
           (int)motor_pp, motor_kv, max_flux);
    
    /* Initialize HIL (Vdc = 59.4V from power supply) */
    hil_init(&hil, 59.4f, max_flux);
    printf("HIL initialized: Vdc=59.4V\r\n");
    
    /* ===== START PWM ===== */
    
    PWM_CCU8_Start(&PWM_CCU8_0);
    printf("PWM started: 20 kHz, P1.11 output enabled\r\n");
    
    /* Set initial duty to 50% (safe startup) */
    PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, DUTY_CENTER);
    printf("Initial PWM duty: 50%%\r\n");
    
    printf("\n>>> HIL running - scenarios will execute automatically\r\n");
    printf(">>> Connect P1.11 to PulseView for PWM visualization\r\n");
    printf(">>> Motor internally simulated and fed back to FOC\r\n\n");
    
    /* ===== MAIN LOOP (IDLE) ===== */
    
    /*
     * All work happens in ISRs:
     * - 20 kHz CCU8 ISR: hil_step_foc() + PWM update
     * - 1 kHz SysTick: Telemetry + Scenario logic
     */
    while(1) {
        __WFI();  /* Wait for interrupt - low power */
    }
    
    return 0;
}

/* ========== HELPER: Printf via UART ========== */

/*
 * Redirect printf to UART (adds to DAVE UART output)
 * Allows printf() to work directly in main()
 */
#include <errno.h>
#include <unistd.h>

int _write(int file, char *ptr, int len) {
    if (file == STDOUT_FILENO) {
        for (int i = 0; i < len; i++) {
            while (UART_Transmit(&UART_0, (uint8_t)ptr[i]) != UART_STATUS_OK) {
                /* Wait for buffer space */
            }
        }
        return len;
    }
    errno = EIO;
    return -1;
}
