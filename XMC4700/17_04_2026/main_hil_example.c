/*
 * main_hil_example.c
 * 
 * Complete example of HIL integration into XMC4700 firmware
 * Demonstrates 20 kHz control loop, UART telemetry, scenario testing
 * 
 * NOTE: This is a TEMPLATE. Adapt to your actual DAVE project structure.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

/* DAVE includes (adjust per your DAVE project) */
#include "DAVE.h"

/* HIL includes */
#include "main_hil.h"
#include "foc_algorithm_xmc.h"

/* ========== GLOBAL STATE ========== */

HIL_State hil;
FOCController foc;

/* System time (ms) for scenario testing */
uint32_t system_time_ms = 0;

/* UART buffer */
char uart_rx_buf[64];
uint8_t uart_rx_idx = 0;

/* Telemetry counters */
uint32_t foc_step_count = 0;
uint32_t telemetry_counter = 0;

/* ========== INITIALIZATION ========== */

void hil_system_init(void) {
    /* Calculate max flux linkage */
    /* max_flux = pp * Kv * 2π / 60 */
    float motor_pp = 11.0f;
    float motor_kv = 141.4f;  /* RPM/V (from iFligh GM3506 datasheet) */
    float max_flux = motor_pp * motor_kv * 2.0f * 3.14159265f / 60.0f;
    
    printf("=== XMC4700 HIL INITIALIZATION ===\r\n");
    printf("Max Flux: %.4f Wb\r\n", max_flux);
    
    /* Initialize HIL system */
    hil_init(&hil, 59.4f, max_flux);  /* Vdc = 59.4V */
    
    /* Initialize FOC algorithm */
    foc_init(&foc);
    
    printf("HIL initialized: Vdc=59.4V, max_flux=%.4f Wb\r\n", max_flux);
    printf("Waiting for speed reference...\r\n");
}

/* ========== TIMER INTERRUPT (20 kHz) ========== */

/*
 * CCU8 PWM Underflow ISR @ 20 kHz
 * Executes entire FOC control loop:
 * 1. Motor model (100 sub-steps)
 * 2. FOC algorithm
 * 3. Inverter model
 * 4. PWM update
 */
void CCU8_0_IRQHandler(void) {
    /* Execute FOC step */
    hil_step_foc(&hil);
    
    /* Update hardware PWM (only P1.11 available on eval board) */
    /* When full 3-phase available, uncomment duty_b and duty_c */
    
    PWMSP_CCU8_SetDutyCycle(&PWM_CCU8, (uint16_t)hil.duty_a);
    // PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_CH2, (uint16_t)hil.duty_b);  // TODO: P2.x
    // PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_CH3, (uint16_t)hil.duty_c);  // TODO: P2.x
    
    /* Debug: output actual PWM to verify */
    GPIO_SetOutputPin(&GPIO_TEST_PIN, (hil.duty_a > 3000) ? 1 : 0);
    
    foc_step_count++;
}

/* ========== SYSTICK / LOW-RATE TIMER (100 Hz) ========== */

/*
 * SysTick Handler @ 1 kHz (divided by 10 for 100 Hz telemetry)
 * Handles:
 * - Time tracking
 * - Scenario logic (speed/load changes)
 * - UART telemetry
 * - Parameter updates
 */
void SysTick_Handler(void) {
    static uint32_t tick_div = 0;
    
    system_time_ms += 1;  /* 1 ms per tick (1 kHz) */
    tick_div++;
    
    if (tick_div >= 10) {  /* 100 Hz telemetry */
        tick_div = 0;
        telemetry_counter++;
        
        /* ===== SCENARIO LOGIC ===== */
        
        /* Scenario A: Step speed from 0 → 300 RPM at t=1s */
        if (system_time_ms >= 1000 && system_time_ms < 6000) {
            hil_set_speed_ref(&hil, 300.0f);
        } else if (system_time_ms >= 6000 && system_time_ms < 10000) {
            hil_set_speed_ref(&hil, 100.0f);
        } else {
            hil_set_speed_ref(&hil, 0.0f);
        }
        
        /* Add load disturbance at t=3s */
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
        
        /* ===== UART COMMAND HANDLING ===== */
        
        if (uart_rx_idx > 0) {
            char cmd = uart_rx_buf[0];
            
            if (cmd == 's' && uart_rx_idx >= 3) {
                /* Format: "s300\r" -> set speed to 300 RPM */
                int speed = atoi(&uart_rx_buf[1]);
                hil_set_speed_ref(&hil, (float)speed);
                printf("Speed ref set to %d RPM\r\n", speed);
            }
            else if (cmd == 'l' && uart_rx_idx >= 3) {
                /* Format: "l15\r" -> set load to 15 mNm */
                int load_mn = atoi(&uart_rx_buf[1]);
                hil_set_load_torque(&hil, load_mn / 1000.0f);
                printf("Load set to %d mNm\r\n", load_mn);
            }
            else if (cmd == 'r') {
                /* Reset system */
                system_time_ms = 0;
                printf("System reset\r\n");
            }
            else if (cmd == 'h') {
                printf("Commands: s<RPM>, l<mNm>, r, h\r\n");
            }
            
            uart_rx_idx = 0;
        }
    }
}

/* ========== UART RX CALLBACK ========== */

/*
 * Called when UART character received
 * Simple line-buffering (awaiting \r)
 */
void uart_rx_callback(uint8_t ch) {
    if (ch == '\r' || ch == '\n') {
        uart_rx_buf[uart_rx_idx] = '\0';
        uart_rx_idx = 0;  /* Signal that buffer is ready (handled in SysTick) */
    } else if (uart_rx_idx < sizeof(uart_rx_buf) - 1) {
        uart_rx_buf[uart_rx_idx++] = ch;
    }
}

/* ========== MAIN ========== */

int main(void) {
    /* Initialize DAVE */
    DAVE_STATUS_t dave_status = DAVE_Init();
    if (dave_status == DAVE_STATUS_SUCCESS) {
        printf("DAVE initialized successfully\r\n");
    }
    
    /* Initialize HIL system */
    hil_system_init();
    
    /* Start PWM (CCU8) */
    PWMSP_CCU8_Start(&PWM_CCU8);
    printf("PWM started (20 kHz)\r\n");
    
    /* Start SysTick for 1 kHz tick */
    /* (DAVE should handle this automatically) */
    
    /* Main idle loop */
    /* All work happens in ISRs */
    printf("\n>>> HIL running - use UART commands: s<RPM>, l<mNm>, r, h\r\n");
    
    while (1) {
        /* Keep alive */
        __WFI();  /* Wait for interrupt */
    }
    
    return 0;
}

/* ========== OPTIONAL: TRACE/DEBUG OUTPUT ========== */

/*
 * High-bandwidth telemetry (call from SysTick at 1 kHz)
 * Outputs CSV format for post-processing
 */
void telemetry_trace_1khz(void) {
    static FILE *trace_file = NULL;
    
    if (trace_file == NULL) {
        trace_file = fopen("trace.csv", "w");
        fprintf(trace_file, "time_ms,speed_rpm,id,iq,te,theta\n");
    }
    
    float speed_rpm = hil_get_speed_rpm(&hil);
    float id = hil_get_current_d(&hil);
    float iq = hil_get_current_q(&hil);
    float te = hil_get_torque(&hil);
    float theta = hil_get_rotor_angle(&hil);
    
    fprintf(trace_file, "%u,%.2f,%.4f,%.4f,%.5f,%.4f\n",
            system_time_ms, speed_rpm, id, iq, te, theta);
    fflush(trace_file);
}

/* ========== HELPER: Printf via UART ========== */

/*
 * Redirect printf to UART (add to your project)
 */
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        UART_Transmit(&UART_0, ptr[i]);
    }
    return len;
}

/*
 * Inline utility: Convert string to int (simple)
 */
int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}
