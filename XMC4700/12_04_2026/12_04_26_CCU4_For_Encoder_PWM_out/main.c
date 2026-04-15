#include "DAVE.h"
#include "SEGGER_RTT.h"

int main(void)
{
  DAVE_STATUS_t status;
  uint32_t period_ticks = 0;
  uint32_t duty_ticks = 0;

  status = DAVE_Init();

  if (status == DAVE_STATUS_SUCCESS)
  {
    /* 1. Prevent floating noise on P1.1 */
    XMC_GPIO_SetMode(P1_1, XMC_GPIO_MODE_INPUT_PULL_DOWN);

    /* 2. Start the PWM on P1.0 */
    PWM_CCU4_Start(&PWM_CCU4_0);

    /* 3. Start Capture App */
    CAPTURE_Start(&CAPTURE_0);

    SEGGER_RTT_WriteString(0, "System Initialized. Connect encoder to P1.1\r\n");

    while(1U)
    {
      /* 4. Read period and duty */
      if (CAPTURE_GetCapturedTime(&CAPTURE_0, &period_ticks) == CAPTURE_STATUS_SUCCESS)
      {
        if (CAPTURE_GetDutyCycle(&CAPTURE_0, &duty_ticks) == CAPTURE_STATUS_SUCCESS)
        {
          /* 5. Calculate angle from duty cycle */
          float angle_degrees = (duty_ticks / (float)period_ticks) * 360.0f;
          SEGGER_RTT_printf(0, "Period: %u | Duty: %u | Angle: %.1f°\r\n",
                           (unsigned int)period_ticks,
                           (unsigned int)duty_ticks,
                           angle_degrees);
        }

        /* 6. Clear Event Flag */
        XMC_CCU4_SLICE_ClearEvent(CAPTURE_0.ccu4_slice_ptr, XMC_CCU4_SLICE_IRQ_ID_EVENT0);
      }

      /* Throttle for RTT readability */
      for(volatile uint32_t i = 0; i < 800000; i++);
    }
  }

  while(1U);
}
