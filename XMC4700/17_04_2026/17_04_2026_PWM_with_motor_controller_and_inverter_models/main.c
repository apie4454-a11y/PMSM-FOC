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

#include "DAVE.h"
#include "main_hil.h"
#include <stdio.h>
#include <stdint.h>

/* ========== GLOBAL HIL STATE ========== */
HIL_State hil;
static uint32_t systick_counter = 0;
static uint32_t telemetry_counter = 0;

/* ========== MOTOR PARAMETERS ========== */
#define VDC_NOMINAL      59.4f      /* Nominal DC bus voltage [V] */
#define MAX_FLUX         0.1628f    /* Back-EMF constant from Motor_Parameters.m [Wb] */

/* ========== SCENARIO TIMING ========== */
static uint32_t time_ms = 0;
static float speed_ref_scenario = 0.0f;
static float load_ref_scenario = 0.0f;

/* ========== ISR: 20 kHz FOC Control Loop ========== */
/*
 * Called by CCU8_0 period match interrupt (PWM timer)
 * Executes FOC algorithm and updates PWM duty cycle
 */
void CCU8_0_IRQHandler(void) {
    /* Step HIL (20 kHz): runs FOC + motor + inverter */
    hil_step_foc(&hil);
    
    /* Update hardware PWM with new duty cycle */
    /* Only using duty_a (phase A) for P1.11 visualization */
    PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, (uint16_t)hil.duty_a);
}

/* ========== ISR: 1 kHz System Tick ========== */
/*
 * Decimated to 100 Hz for telemetry and scenario updates
 */
void SysTick_Handler(void) {
    systick_counter++;
    time_ms++;
    
    /* Decimate to 100 Hz (every 10 ms) */
    telemetry_counter++;
    if (telemetry_counter >= 10) {
        telemetry_counter = 0;
        
        /* ===== SCENARIO EXECUTION (100 Hz) ===== */
        /* Test 1: Speed step 0 -> 300 RPM at t=1000ms */
        if (time_ms >= 1000 && time_ms < 5000) {
            speed_ref_scenario = 300.0f;
        }
        /* Test 2: Speed step 300 -> 100 RPM at t=5000ms */
        else if (time_ms >= 5000) {
            speed_ref_scenario = 100.0f;
        }
        
        /* Test 3: Load +15mNm at t=3000ms, remove at t=4000ms */
        if (time_ms >= 3000 && time_ms < 4000) {
            load_ref_scenario = 0.015f;  /* 15 mNm */
        } else {
            load_ref_scenario = 0.0f;
        }
        
        /* Update HIL references */
        hil_set_speed_ref(&hil, speed_ref_scenario);
        hil_set_load_torque(&hil, load_ref_scenario);
        
        /* ===== TELEMETRY OUTPUT (100 Hz) ===== */
        printf("[%5lu ms] RPM: %8.2f  Id: %7.4f  Iq: %7.4f  Te: %8.5f  θ: %7.4f\r\n",
               time_ms,
               hil_get_speed_rpm(&hil),
               hil_get_current_d(&hil),
               hil_get_current_q(&hil),
               hil_get_torque(&hil),
               hil_get_rotor_angle(&hil));
    }
}

/* ========== MAIN ENTRY POINT ========== */
int main(void) {
    DAVE_STATUS_t status;
    
    /* Initialize DAVE APPs (UART, PWM, timers) */
    status = DAVE_Init();
    
    if (status != DAVE_STATUS_SUCCESS) {
        XMC_DEBUG("DAVE initialization failed\n");
        while(1);
    }
    
    /* Print startup banner */
    printf("\r\n");
    printf("=== XMC4700 HIL FIRMWARE ===\r\n");
    printf("Motor: pp=11, Kv=141.4 RPM/V, max_flux=%.4f Wb\r\n", MAX_FLUX);
    printf("HIL initialized: Vdc=%.1f V\r\n", VDC_NOMINAL);
    
    /* Initialize HIL system */
    hil_init(&hil, VDC_NOMINAL, MAX_FLUX);
    
    printf("FOC algorithm ready\r\n");
    printf("Motor model ready\r\n");
    printf("Inverter model ready\r\n");
    
    /* Start PWM (20 kHz on CCU8) */
    PWMSP_CCU8_Start(&PWM_CCU8_0);
    printf("PWM started: 20 kHz, P1.11 output enabled\r\n");
    printf("Initial PWM duty: 50%%\r\n");
    
    printf("\r\n>>> HIL running - scenarios will execute automatically\r\n");
    printf(">>> Connect P1.11 to PulseView for PWM visualization\r\n");
    printf(">>> Motor internally simulated and fed back to FOC\r\n\r\n");
    
    /* Main loop - wait for interrupts */
    while(1) {
        __WFI();  /* Wait for interrupt */
    }
}
