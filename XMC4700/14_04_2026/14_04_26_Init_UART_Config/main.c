#include <DAVE.h>

int main(void) {
  DAVE_Init();

  uint8_t rx_byte;
  // We use strlen-style manual lengths to avoid sending the hidden \0 null terminator
  uint8_t msg[] = "Link Active\r\n";

  // Transmit initial message
  UART_Transmit(&UART_0, msg, 13);
  while(UART_0.runtime->tx_busy);

  while(1) {
    // 1. Start a fresh receive
    UART_Receive(&UART_0, &rx_byte, 1);

    // 2. Wait for the byte to arrive
    while(UART_0.runtime->rx_busy);

    // 3. Simple Echo: Send only the character back
    // This removes the "Received: " string to narrow down the error
    UART_Transmit(&UART_0, &rx_byte, 1);
    while(UART_0.runtime->tx_busy);
  }
}
