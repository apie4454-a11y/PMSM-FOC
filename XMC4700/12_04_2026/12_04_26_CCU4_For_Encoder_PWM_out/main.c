#include "DAVE.h"
#include "SEGGER_RTT.h"
#include <stdio.h>  // Added for sprintf

int main(void)
{
  DAVE_STATUS_t status;
  uint32_t period_ticks = 0;
  uint32_t duty_ticks = 0;
  char buffer[80]; // Buffer moved out of loop for efficiency

  status = DAVE_Init();

  if (status == DAVE_STATUS_SUCCESS)
  {
    XMC_GPIO_SetMode(P1_1, XMC_GPIO_MODE_INPUT_PULL_DOWN);
    PWM_CCU4_Start(&PWM_CCU4_0);
    CAPTURE_Start(&CAPTURE_0);

    SEGGER_RTT_WriteString(0, "System Initialized. Reading Encoder...\r\n");

    while(1U)
    {
      /* Read captured values */
      if (CAPTURE_GetCapturedTime(&CAPTURE_0, &period_ticks) == CAPTURE_STATUS_SUCCESS)
      {
        if (CAPTURE_GetDutyCycle(&CAPTURE_0, &duty_ticks) == CAPTURE_STATUS_SUCCESS)
        {
          /* Safety check for division by zero */
          if (period_ticks > 0)
          {
            float angle_degrees = (duty_ticks / (float)period_ticks) * 360.0f;

            /* Format string using the standard library (requires float-support enabled) */
            sprintf(buffer, "Period: %lu | Duty: %lu | Angle: %.1f deg\r\n",
                    (unsigned long)period_ticks,
                    (unsigned long)duty_ticks,
                    (double)angle_degrees);

            SEGGER_RTT_WriteString(0, buffer);
          }
        }

        /* Clear event flag to allow next capture */
        XMC_CCU4_SLICE_ClearEvent(CAPTURE_0.ccu4_slice_ptr, XMC_CCU4_SLICE_IRQ_ID_EVENT0);
      }

      /* Delay to prevent RTT overflow - ~100-200ms depending on clock */
      for(volatile uint32_t i = 0; i < 1000000; i++);
    }
  }

  while(1U);
}
