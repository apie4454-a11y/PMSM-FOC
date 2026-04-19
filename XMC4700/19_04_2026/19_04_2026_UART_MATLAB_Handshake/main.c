/*
 * main.c
 *
 * UART Handshake Protocol with MATLAB
 * Minimal implementation to test message exchange and synchronization
 * 
 * Protocol:
 * 1. XMC waits for message from MATLAB
 * 2. XMC echoes back the message
 * 3. XMC waits for "OK" confirmation
 * 4. Repeat
 * 
 * Created on: 2026 Apr 19
 * Author: Rizwan Sadik
 */

#include "DAVE.h"
#include <stdio.h>
#include <string.h>

/* ========== UART COMMUNICATION ========== */
#define UART_BUF_SIZE 128
char uart_rx_buffer[UART_BUF_SIZE];
uint8_t uart_rx_idx = 0;
volatile bool uart_message_ready = false;
uint8_t uart_temp_byte;

/* UART RX Interrupt Handler - triggered on byte reception */
void UART_RX_Handler(void) {
    char c = (char)uart_temp_byte;
    if (c == '\n' || c == '\r') {
        if (uart_rx_idx > 0) {
            uart_rx_buffer[uart_rx_idx] = '\0';  /* Terminate string */
            uart_message_ready = true;
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
int main(void)
{
    DAVE_STATUS_t status;

    /* Initialize DAVE APPs  */
    status = DAVE_Init();

    if (status != DAVE_STATUS_SUCCESS)
    {
        XMC_DEBUG("DAVE initialization failed\n");
        while(1U) {
        }
    }

    /* Arm UART RX interrupt */
    UART_Receive(&UART_0, &uart_temp_byte, 1);

    /* ========== MAIN LOOP - HANDSHAKE STATE MACHINE ========== */
    volatile bool waiting_for_ok = false;
    uint32_t message_count = 0;
    
    while(1U) {
        /* State 1: Wait for data message from MATLAB */
        if (uart_message_ready && !waiting_for_ok) {
            message_count++;
            
            /* Echo back what we received */
            char response[256];
            sprintf(response, "XMC received: %s\r\n", uart_rx_buffer);
            UART_Transmit(&UART_0, (uint8_t*)response, strlen(response));
            while(UART_0.runtime->tx_busy);  /* Wait for TX to complete */
            
            /* Now wait for OK signal from MATLAB */
            waiting_for_ok = true;
            uart_rx_idx = 0;
            uart_message_ready = false;
        }
        
        /* State 2: Wait for OK from MATLAB before continuing */
        if (uart_message_ready && waiting_for_ok) {
            if (strcmp(uart_rx_buffer, "OK") == 0) {
                waiting_for_ok = false;
            } else {
                waiting_for_ok = false;  /* Reset and try again */
            }
            
            /* Reset for next cycle */
            uart_rx_idx = 0;
            uart_message_ready = false;
        }
    }
    
    return 0;
}
