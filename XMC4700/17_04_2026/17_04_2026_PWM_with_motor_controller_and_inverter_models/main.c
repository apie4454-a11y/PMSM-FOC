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

/* ========== SYSTEM CONSTANTS ========== */
#define VDC_NOMINAL  48.0f      /* DC bus voltage [V] */
#define MAX_FLUX     0.00614f   /* Back-EMF constant [Wb] - from motor parameters */

/* ========== HIL STATE ========== */
HIL_State hil;  /* Global HIL state variable */

/* ========== TEST STATE MACHINE ========== */
enum { STATE_IDLE, STATE_INVERTER_TEST } state = STATE_IDLE;
uint32_t test_step = 0;
uint32_t test_steps_total = 0;

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
            
            /* If in test mode and got "OK", signal to continue FOC step */
            if (state == STATE_INVERTER_TEST && strcmp(uart_rx_buffer, "OK") == 0) {
                uart_ok_received = true;  /* Interrupt signals: continue FOC step */
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
            if (strcmp(uart_rx_buffer, "RUN_INVERTER_TEST") == 0) {
                /* Start inverter test */
                state = STATE_INVERTER_TEST;
                test_step = 0;
                test_steps_total = 500;  /* 500 samples over 0->2π for smooth curve */
                
                /* Send acknowledgment */
                char response[128];
                sprintf(response, "INVERTER_TEST starting %lu steps\r\n", test_steps_total);
                UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
                while(UART_0.runtime->tx_busy);
                SEGGER_RTT_WriteString(0, response);
                
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
        
        /* ===== STATE: INVERTER_TEST - Generate sample on each OK from MATLAB ===== */
        if (state == STATE_INVERTER_TEST && uart_ok_received) {
            /* UART interrupt set this flag when OK received */
            uart_ok_received = false;  /* Clear flag for next cycle */
            
            /* Generate one FOC step = one sample */
            float theta = (2.0f * 3.14159265f * test_step) / (float)test_steps_total;
            
            /* Apply constant dq voltage (vd=0, vq=10V) and transform to RYB */
            float vd = 0.0f;
            float vq = 10.0f;
            RYB_Output ryb = dq_to_ryb(vd, vq, theta);
            
            /* Send telemetry: theta,vr,vy,vb */
            char response[256];
            sprintf(response, "%.4f,%.4f,%.4f,%.4f\r\n", theta, ryb.vr, ryb.vy, ryb.vb);
            UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
            while(UART_0.runtime->tx_busy);
            
            test_step++;
            
            /* Check if done */
            if (test_step >= test_steps_total) {
                state = STATE_IDLE;
                SEGGER_RTT_WriteString(0, "INVERTER_TEST complete\r\n");
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
