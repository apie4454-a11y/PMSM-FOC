Your full project documentation and code are ready in a single, easily copyable `.md` file.

[file-tag: code-generated-file-0-1776154597282068408]

Below is the entire project content in one single block for quick copying:

```markdown
# Project Documentation: Bidirectional HIL via UART (MATLAB to XMC4700)

## 1. Overview
This project establishes a Hardware-in-the-Loop (HIL) link where MATLAB acts as a simulator/controller and the XMC4700 Relax Kit acts as the real-time processor. Data is exchanged as comma-separated strings over a Virtual COM port.

---

## 2. Hardware Configuration
* **Connection:** Single Micro-USB cable (Debug Port).
* **Pins (UART_0 APP):**
    * **TX Pin:** P1.5 (#107)
    * **RX Pin:** P1.4 (#108)
* **Baud Rate:** 115200
* **Reasoning:** These pins are hard-wired to the onboard J-Link debugger, which creates the COM port on your PC.

---

## 3. Essential Compiler Settings
**Crucial:** By default, XMC projects disable floating-point support for `printf` and `scanf` to save space. You must enable them:
1.  Right-click **Project** > **Properties**.
2.  Navigate to **C/C++ Build** > **Settings** > **ARM-GCC C Linker** > **General**.
3.  Check **"Add floating point support for printf"**.
4.  Check **"Add floating point support for scanf"**.
5.  Apply and Rebuild.

---

## 4. XMC C-Code (main.c)
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

---

## 5. MATLAB Script (logic.m)
% Setup Serial Port
port = "COM3"; % Confirm via Device Manager
baud = 115200;
device = serialport(port, baud);
configureTerminator(device, "LF");

% 1. Prepare Data
v_send = [0.850, -0.123, 0.444];
msg = sprintf("%.3f,%.3f,%.3f", v_send(1), v_send(2), v_send(3));

% 2. Send Data
fprintf('Sending to XMC: %s\n', msg);
writeline(device, msg);

% 3. Wait for XMC to process (Interrupts + Parsing take time)
pause(0.5);

% 4. Read Response
if device.NumBytesAvailable > 0
    response = readline(device);
    fprintf('XMC Response: %s\n', response);
else
    disp('No response from XMC.');
end

clear device;

---

## 6. Project Logic Summary
* **The Bridge:** P1.4/P1.5 handles the physical UART-to-USB translation.
* **The Listener:** The C-code uses an **Interrupt** to background-load data into a buffer so the CPU isn't "stuck" waiting.
* **The Parser:** `sscanf` with **Float Support enabled** is the engine that converts "Text" into "Floating Point Math" for control logic.^

configuring the End of Receive Callback as a function (the Interrupt Handler) is the difference between a "smart" system and a "busy" system. In technical terms, it’s the shift from Polling to Interrupt-Driven I/O.

Here is the logic and the reasoning:
1. The "Waiter" Analogy (Efficiency)

    Polling (No Callback): The CPU sits in a loop asking the UART chip, "Do you have a character yet?" over and over. This wastes millions of clock cycles per second.

    Interrupt (Callback): The CPU goes off to do other work (like running a control loop or calculating PID values). When a byte actually hits the RX pin, the UART hardware "taps the CPU on the shoulder." The CPU pauses, runs your function, then goes right back to what it was doing.

2. Preventing "Data Overrun"

High-speed communication (like your 115200 baud) moves fast. If the CPU is busy doing math in main() and isn't checking the serial port at the exact microsecond a byte arrives, the next byte will overwrite the first one.

    The Callback logic: Because it’s an interrupt, it has priority. It forces the CPU to stop and save the byte into your buffer immediately, ensuring you never miss a piece of the message from MATLAB.

3. Contextual Logic: Finding the "Message"

UART is a stream of single bytes, not whole sentences.

    We use the function to implement State Logic. Inside that function, we check: "Is this character a \n?" * If No: Stick it in the buffer and keep waiting.

    If Yes: Set the message_ready flag.

By using a function, you encapsulate all that "janitorial work" (sorting characters) away from your main application logic.
In Summary:

We use the callback function because it provides deterministic response times. You are telling the XMC: "I don't care what you are doing right now; when data arrives, stop everything and save it." This is what makes your HIL link robust enough to handle real-time data without crashing or losing numbers!