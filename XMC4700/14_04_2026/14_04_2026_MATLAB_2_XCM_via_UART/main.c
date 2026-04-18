#include <DAVE.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 64
char rx_buffer[BUF_SIZE];
uint8_t rx_idx = 0;
volatile bool message_ready = false;
uint8_t temp_byte;

// Interrupt Handler: Triggered on every byte received
void UART_RX_Handler(void) {
    char c = (char)temp_byte;
    if (c == '\n' || c == '\r') {
        if (rx_idx > 0) {
            rx_buffer[rx_idx] = '\0'; // Terminate string
            message_ready = true;
        }
    } else if (rx_idx < BUF_SIZE - 1) {
        rx_buffer[rx_idx++] = c;
    }
    // Re-enable interrupt to catch the next byte
    UART_Receive(&UART_0, &temp_byte, 1);
}

int main(void) {
    DAVE_Init();

    // Start the first listen
    UART_Receive(&UART_0, &temp_byte, 1);

    while(1) {
        if (message_ready) {
            float v1 = 0, v2 = 0, v3 = 0;

            // Logic: Convert the received string back into 3 floats
            int parsed = sscanf(rx_buffer, "%f,%f,%f", &v1, &v2, &v3);

            char tx_out[128];
            if (parsed == 3) {
                // Return success message to MATLAB
                sprintf(tx_out, "Parsed: %.3f, %.3f, %.3f\n", v1, v2, v3);
            } else {
                // Return error message if parsing failed
                sprintf(tx_out, "Error: XMC saw %d values in [%s]\n", parsed, rx_buffer);
            }

            UART_Transmit(&UART_0, (uint8_t*)tx_out, strlen(tx_out));
            while(UART_0.runtime->tx_busy); // Wait for transmission to finish

            // Reset for next message
            rx_idx = 0;
            message_ready = false;
        }
    }
    return 1;
}
