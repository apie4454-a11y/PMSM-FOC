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

#include "inverter_test.h" // For inverter model test
#include "motor_test.h"    // For motor model test
#include "pwm_modulator.h"  // SPWM modulator
#include "inverter_model.h" // Inverter model

/* ========== SYSTEM CONSTANTS ========== */
#define VDC_NOMINAL  48.0f      /* DC bus voltage [V] */
#define MAX_FLUX     0.00614f   /* Back-EMF constant [Wb] - from motor parameters */
#define NUM_CYCLES   4          /* Number of motor cycles to capture (adjust for more/fewer cycles) */

/* ========== HIL STATE ========== */
HIL_State hil;  /* Global HIL state variable */

/* ========== PWM MODULATOR & INVERTER ========== */
PWMModulator pwm_mod;      /* SPWM modulator instance */
InverterModel inverter;    /* Inverter model instance */

/* ========== TEST STATE MACHINE ========== */
uint32_t foc_cycle_counter = 0;  /* Counts 20kHz FOC cycles for time tracking */

/* ========== UART COMMUNICATION ========== */
/* Empty handlers - DAVE expects these symbols (linker requirement) */
void UART_RX_Handler(void) {
    /* Not used - RTT only */
}

void UART_TX_Handler(void) {
    /* Not used */
}

/* ========== MAIN ENTRY POINT ========== */
int main(void) {
    DAVE_STATUS_t status;
    
    /* Initialize DAVE APPs (PWM, timers) */
    status = DAVE_Init();
    
    if (status != DAVE_STATUS_SUCCESS) {
        XMC_DEBUG("DAVE initialization failed\n");
        while(1);
    }
    
    /* Initialize RTT */
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "\r\n=== XMC4700 Motor Test via RTT Viewer ===\r\n");
    SEGGER_RTT_WriteString(0, "Initializing HIL system...\r\n");
    
    /* Initialize HIL system */
    hil_init(&hil, VDC_NOMINAL, MAX_FLUX);
    SEGGER_RTT_WriteString(0, "HIL initialized\r\n");
    
    /* Initialize PWM modulator and inverter */
    pwm_modulator_init(&pwm_mod);
    inverter_init(&inverter, PWM_VDC);
    SEGGER_RTT_WriteString(0, "PWM modulator and inverter initialized\r\n");
    
    /* Set speed reference */
    hil_set_speed_ref(&hil, 1000.0f);  /* 1000 RPM */
    SEGGER_RTT_WriteString(0, "Speed ref set to 1000 RPM\r\n");

    /* Run motor test via RTT Viewer */
    SEGGER_RTT_WriteString(0, "\r\n=== Running Motor Model Test ===\r\n");
    motor_model_rtt_test();
    SEGGER_RTT_WriteString(0, "\r\n=== Motor Test Complete ===\r\n");
    SEGGER_RTT_WriteString(0, "Ready for next command\r\n");

    /* ========== MAIN LOOP - RUN FOC CONTINUOUSLY ========== */
    
    while(1) {
        hil_step_foc(&hil);
    }
    
    return 0;
}
