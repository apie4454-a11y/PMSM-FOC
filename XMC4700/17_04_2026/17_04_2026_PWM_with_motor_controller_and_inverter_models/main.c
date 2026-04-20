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
enum { STATE_IDLE, STATE_DQ_TO_ABC_STREAM_TEST } state = STATE_IDLE;
uint32_t test_step = 0;
uint32_t test_steps_total = 0;
uint32_t foc_cycle_counter = 0;  /* Counts 20kHz FOC cycles for time tracking */

/* ========== UART COMMUNICATION ========== */
#define UART_BUF_SIZE 128
char uart_rx_buffer[UART_BUF_SIZE];
uint8_t uart_rx_idx = 0;
volatile bool uart_message_ready = false;
volatile bool uart_ok_received = false;  /* Flag set by interrupt when OK arrives */
uint8_t uart_temp_byte;

/* UART RX Interrupt Handler - triggered on byte reception */
void UART_RX_Handler(void) {
    char c = (char)uart_temp_byte;
    if (c == '\n' || c == '\r') {
        if (uart_rx_idx > 0) {
            uart_rx_buffer[uart_rx_idx] = '\0';  /* Terminate string */
            uart_message_ready = true;
            
            /* Debug: show what was received */
            SEGGER_RTT_printf(0, "RX: [%s]\r\n", uart_rx_buffer);
            
            /* If in test mode and got "OK", signal to continue to next step */
            if (state == STATE_DQ_TO_ABC_STREAM_TEST && strcmp(uart_rx_buffer, "OK") == 0) {
                uart_ok_received = true;  /* Interrupt signals: continue */
                SEGGER_RTT_WriteString(0, "✓ OK matched! uart_ok_received = true\r\n");
            }
        }
    } else if (uart_rx_idx < UART_BUF_SIZE - 1) {
        uart_rx_buffer[uart_rx_idx++] = c;
    }
    /* Re-arm UART to catch next byte */
    UART_Receive(&UART_0, &uart_temp_byte, 1);
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
    SEGGER_RTT_WriteString(0, "\r\n=== XMC4700 HIL with RTT ===\r\n");
    SEGGER_RTT_WriteString(0, "FOC + Motor Model + Inverter Model\r\n");
    
    /* Arm UART RX interrupt - listen for incoming bytes from MATLAB */
    UART_Receive(&UART_0, &uart_temp_byte, 1);
    SEGGER_RTT_WriteString(0, "UART armed - ready for MATLAB input\r\n");
    
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
    
    /* Start PWM (20 kHz on CCU8) */
   // PWM_CCU8_Start(&PWM_CCU8_0);
    //SEGGER_RTT_WriteString(0, "PWM started: 20 kHz\r\n");
    //SEGGER_RTT_WriteString(0, "Telemetry streaming...\r\n\r\n");

    /* ========== MAIN LOOP - INTERRUPT-DRIVEN HANDSHAKE ========== */
    /* Main runs FOC normally; UART interrupt drives test */
    
    while(1) {
        /* ===== STATE: IDLE - Wait for MATLAB command ===== */
        if (state == STATE_IDLE && uart_message_ready) {
            /* Parse MATLAB command */
            if (strcmp(uart_rx_buffer, "RUN_DQ_TO_ABC_STREAM_TEST") == 0) {
                /* Start dq->abc coordinate transform stream test */
                state = STATE_DQ_TO_ABC_STREAM_TEST;
                test_step = 0;
                test_steps_total = 2000;  /* 2000 samples over multiple cycles */
                
                /* Send header and acknowledgment */
                char response[128];
                sprintf(response, "DQ_TO_ABC_STREAM_TEST starting %lu steps\r\n", test_steps_total);
                UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
                while(UART_0.runtime->tx_busy);
                SEGGER_RTT_WriteString(0, response);
                
                /* Send CSV header */
                UART_Transmit(&UART_0, (uint8_t*)"Sample,theta,vr,vy,vb\r\n", 23);
                while(UART_0.runtime->tx_busy);
                
                /* Trigger first sample immediately */
                uart_ok_received = true;
            } else {
                /* Echo unknown command */
                char response[256];
                sprintf(response, "XMC received: %s\r\n", uart_rx_buffer);
                UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
                while(UART_0.runtime->tx_busy);
                SEGGER_RTT_WriteString(0, response);
            }
            
            uart_rx_idx = 0;
            uart_message_ready = false;
        }
        
        /* ===== STATE: DQ_TO_ABC_STREAM_TEST - Generate coordinate transform sample on each OK from MATLAB ===== */
        if (state == STATE_DQ_TO_ABC_STREAM_TEST && uart_ok_received) {
            /* UART interrupt set this flag when OK received */
            uart_ok_received = false;  /* Clear flag for next cycle */

            /* Generate one FOC step = one sample */
            float theta = (2.0f * 3.14159265f * NUM_CYCLES * test_step) / (float)test_steps_total;

            /* ===== STEP 1: Apply constant dq voltage (vd=0, vq=10V) and transform to RYB ===== */
            float vd = 0.0f;
            float vq = 10.0f;
            RYB_Output ryb_ref = dq_to_ryb(vd, vq, theta);  /* Reference voltages */

            /* ===== STEP 2: Simple Linear Duty Mapping (no SPWM) ===== */
            /* Direct voltage to PWM duty conversion: map [-VDC/2, +VDC/2] to [0, 6000] */
            float vdc = 59.4f;
            PWMDutyCycles duty;
            duty.duty_a = 3000.0f + (ryb_ref.vr / (vdc/2.0f)) * 3000.0f;
            duty.duty_b = 3000.0f + (ryb_ref.vy / (vdc/2.0f)) * 3000.0f;
            duty.duty_c = 3000.0f + (ryb_ref.vb / (vdc/2.0f)) * 3000.0f;

            /* ===== STEP 3: Inverter Model (apply DC bus and non-idealities) ===== */
            inverter_pwm_to_voltage(&inverter, duty.duty_a, duty.duty_b, duty.duty_c);
            
            /* Get actual inverter output voltages */
            float vr_actual = inverter.out.vrn;
            float vy_actual = inverter.out.vyn;
            float vb_actual = inverter.out.vbn;

            /* Wrap theta to [0, 2π) for CSV output (while keeping continuous theta for calculation) */
            float theta_wrapped = fmodf(theta, 2.0f * 3.14159265f);

            /* Send telemetry over UART: sample,theta,vr,vy,vb (actual inverter output) */
            char response[128];
            sprintf(response, "%lu,%.6f,%.6f,%.6f,%.6f\r\n", test_step + 1, theta_wrapped, vr_actual, vy_actual, vb_actual);
            UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
            while(UART_0.runtime->tx_busy);

            test_step++;

            /* Check if done */
            if (test_step >= test_steps_total) {
                state = STATE_IDLE;
                SEGGER_RTT_printf(0, "DQ_TO_ABC_STREAM_TEST complete: %lu samples\r\n", test_step);
            }

            uart_rx_idx = 0;
            uart_message_ready = false;
        }
        
        /* ===== Run FOC normally in IDLE state ===== */
        if (state == STATE_IDLE) {
            hil_step_foc(&hil);
        }
    }
    
    return 0;
}
