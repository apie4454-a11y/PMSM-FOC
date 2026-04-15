# AS5048A Encoder Integration Guide for XMC4700 via DAVE IDE

**Date:** 12-04-2026  
**Purpose:** Step-by-step DAVE configuration for AS5048A encoder (PWM and SPI modes)  
**Status:** Hands-on commissioning reference

---

## Overview

AS5048A magnetic rotary encoder outputs rotor angle in multiple formats. We'll cover two:

1. **PWM Mode** (3 wires, simpler, START HERE)
2. **SSI/SPI Mode** (5 wires, higher resolution, advanced)

---

## Part 1: PWM Mode (Recommended for Initial Commissioning)

### Hardware Setup

**Wiring:**
```
AS5048A Red   → XMC4700 VCC (3.3V)
AS5048A Black → XMC4700 GND
AS5048A White → XMC4700 P1.1 (GPIO input)
```

**Pin Notes:**
- P1.1 is on port 1, pin 1 (verify on XMC4700 Relax Lite board)
- Can also use P0.0, P0.1, etc. (any GPIO capable of CCU4 input capture)

---

### DAVE IDE Configuration (PWM Capture)

#### Step 1: Create New Project

1. **Open DAVE IDE** (Installed with DAVE package)
2. **File → New → DAVE C Project**
   - Device: XMC4700 (select your variant, usually XMC4700-F144)
   - Project name: `PMSM_Motor_Control` (or your choice)
   - Finish

#### Step 2: Add CCU4 App (Slice 3 for Encoder)

1. **Right-click on project → Add APP**
2. **Search:** "CCU4"
3. **Select:** `CCU4` (Capture/Compare Unit 4 - Timer)
4. **Click Add**

#### Step 3: Configure CCU4 for PWM Capture


In the CCU4 app properties (double-click the added app), use these **final, validated settings** (see also the Session file for screenshots and troubleshooting):

**Basic Settings:**
- **Module:** CCU40 (not CCU41, CCU42, CCU43)
- **Slice:** 3 (Slice 3, or as available)

**Timer Mode:**
- **Timer:** Multi-Channel (or Single-Channel)
- **Mode:** Capture

**Capture Configuration:**
- **Capture Edge:** Rise-to-Rise (captures full period; both edge and function enable must be set)
- **Overwrite:** Enabled (prevents lockup, always gives latest data)
- **Interrupts:** Disabled (use polling for initial debug)

**Input Pin:**
- **Input:** P1.1 (or your chosen GPIO)
- **Alternate Function:** CCU4_IN3G_P1_1 (verify in pin list)
- **Pin Mode:** Tristate (high impedance)
- **Pull-down:** Enabled (prevents ghost data when encoder is disconnected)

**Event Routing:**
- **Event 0:** Explicitly route to external input (critical for capture to trigger)

**Clock:**
- **Input Clock:** 144 MHz (or your system clock)
- **Prescaler:** Direct mode (100 ns resolution recommended)

**Save and Generate Code** (Ctrl+G or toolbar icon)

**Troubleshooting:**
- If capture register is stuck at 0, double-check Event 0 routing and all interdependent settings (see Session file Phase 4 for details).
- DAVE APPs can fail silently if config is incomplete—always verify with known-good internal PWM before connecting encoder.

---

### C Code: Reading Encoder via PWM


**For the final, working code for PWM capture and encoder reading, see:**
**12_04_2026_Session_CCU4_Capture_Breakthrough.md** (Phase 5.1, “Corrected Encoder Capture Configuration” section).

This ensures you always use the latest, validated code pattern.

**Expected Output (as you rotate shaft by hand):**
```
=== AS5048A PWM Encoder Test ===
Rotate motor shaft by hand and watch angle update.

Counts: 0000 → Angle:   0.0°
Counts: 1024 → Angle:  90.0°
Counts: 2048 → Angle: 180.0°
Counts: 4095 → Angle: 359.9°
Counts: 0000 → Angle:   0.0°  (full revolution, back to start)
```

---

### Troubleshooting PWM Mode

**Problem: Encoder counts stuck at 0**
- Check P1.1 connection to XMC (continuity test)
- Verify VCC and GND connections to encoder
- In DAVE, confirm CCU4_IN3G_P1_1 is selected as input pin
- Verify CCU4 app is added and code generated

**Problem: Counts not changing smoothly**
- AS5048A might not be outputting PWM (check operating voltage)
- PWM frequency might be wrong (scope check: should be ~10 kHz PWM)
- Prescaler in DAVE might be too high → adjust and regenerate

**Problem: Values jump/glitch**
- Electrical noise on P1.1 line → add 100nF capacitor near P1.1 to GND
- Check GND connection quality

---

## Part 2: SSI/SPI Mode (Advanced, For Later)

### Hardware Setup

**Wiring (5 wires via SPI):**
```
AS5048A VCC (Red)    → XMC4700 3.3V
AS5048A GND (Black)  → XMC4700 GND
AS5048A CLK (Yellow) → XMC4700 P5.8 (SPI Clock)
AS5048A MOSI (Green) → XMC4700 P5.9 (tied low, not used for read)
AS5048A MISO (Blue)  → XMC4700 P5.10 (encoder data output)
AS5048A CS (Brown)   → XMC4700 P1.0 (chip select, GPIO)
```

**Pin Notes:**
- These are I2C/SPI pins; verify on XMC4700 board
- P5.8/P5.9/P5.10 form USIC SPI interface
- P1.0 is GPIO (manual CS control)

---

### DAVE IDE Configuration (SPI)

#### Step 1: Add USIC App

1. **Right-click project → Add APP**
2. **Search:** "USIC"
3. **Select:** `USIC (Serial Interface)` 
4. **Click Add**

#### Step 2: Configure USIC as SPI Master

In USIC properties:

**Basic:**
- **Interface:** SPI
- **Mode:** Master

**Clock:**
- **Baudrate:** 1000000 (1 MHz) — AS5048A SPI max ~2 MHz
- **Clock Polarity:** 0 (CPOL=0)
- **Clock Phase:** 0 (CPHA=0)

**Data Format:**
- **Word Length:** 16 bits
- **Shift Direction:** MSB first
- **Frame Length:** 2 (send 2 bytes, read 2 bytes)

**Pins:**
- **MOSI (TX):** P5.9 (AS5048A MOSI)
- **MISO (RX):** P5.10 (AS5048A MISO)
- **CLK:** P5.8 (AS5048A CLK)

**Save and Generate Code**

#### Step 3: Add GPIO App for CS

1. **Add APP → GPIO**
2. **Configure:**
   - **Pin:** P1.0
   - **Direction:** Output
   - **Initial State:** High
3. **Save and Generate**

---

### C Code: Reading Encoder via SPI

Create `encoder_spi.c`:

```c
#include "DAVE.h"
#include <stdio.h>

// AS5048A SPI read command
#define AS5048A_READ_ANGLE 0x3FFC

void encoder_spi_init(void) {
    DAVE_Init();  // Initializes USIC + GPIO
    printf("Encoder SPI initialized\n");
}

uint16_t encoder_read_spi(void) {
    uint8_t tx_data[2] = {(AS5048A_READ_ANGLE >> 8) & 0xFF, 
                          AS5048A_READ_ANGLE & 0xFF};
    uint8_t rx_data[2] = {0, 0};
    
    // Pull CS low
    XMC_GPIO_SetOutputLow(XMC_GPIO_PORT1, 0);  // P1.0
    
    // Send SPI command, receive angle
    USIC_SpiMaster_Transfer(&USIC_0, tx_data, rx_data, 2);
    
    // Pull CS high
    XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 0);  // P1.0
    
    // Extract 14-bit angle from received data
    uint16_t angle_raw = ((rx_data[0] << 8) | rx_data[1]) & 0x3FFF;
    
    return angle_raw;
}

float encoder_get_angle_spi(void) {
    uint16_t counts = encoder_read_spi();
    float angle = (counts / 16384.0) * 360.0;  // 14-bit (16384 counts)
    return angle;
}

int main(void) {
    encoder_spi_init();
    
    printf("=== AS5048A SPI Encoder Test ===\n\n");
    
    while(1) {
        uint16_t raw = encoder_read_spi();
        float angle = encoder_get_angle_spi();
        
        printf("Counts: %5d → Angle: %7.2f°\n", raw, angle);
        
        // Delay ~100ms
        for(int i = 0; i < 10000000; i++);
    }
    
    return 0;
}
```

**Expected Output:**
```
=== AS5048A SPI Encoder Test ===

Counts: 00000 → Angle:    0.00°
Counts: 04096 → Angle:   90.00°
Counts: 08192 → Angle:  180.00°
Counts: 16383 → Angle:  359.98°
```

---

### Troubleshooting SPI Mode

**Problem: SPI returns 0xFFFF (all 1s)**
- CS not toggling properly → check P1.0 GPIO (scope it)
- MISO not connected → verify P5.10 connection
- Clock not running → verify P5.8 on scope

**Problem: CRC error**
- AS5048A returns CRC error in bits — normal if you ignore CRC validation
- Use mask `0x3FFF` to extract only angle bits

**Problem: Transfers timeout**
- USIC baudrate too high → reduce to 500 kHz, try again
- SPI pins not configured correctly in DAVE → regenerate

---

## Comparison: PWM vs SPI

| Feature | PWM | SPI |
|---------|-----|-----|
| **Wires** | 3 (simple) | 5 (complex) |
| **Resolution** | 14-bit (0-4095) | 14-bit (0-16383) |
| **Setup Time** | ~15 min | ~30 min |
| **Noise Immunity** | Medium | High |
| **Speed** | ~10 kHz refresh | ~1 kHz read rate |
| **For Commissioning** | ✅ Use first | Use after PWM works |

---

## Integration with Motor Spin-Up

Once encoder is confirmed working:

```c
// In your main motor control loop
float theta_e = (encoder_get_angle_degrees() - THETA_E_ZERO_REF) * 11.0;

// Use theta_e for commutation (see commissioning_strategy.md)
commutate(theta_e);
```

---

## Next Steps

1. **Choose PWM mode** (this guide, Part 1)
2. Build XMC4700 project with CCU4 app
3. Load code, rotate motor by hand
4. Verify encoder counts 0→4096 smoothly
5. Proceed to Step 2 of hardware commissioning strategy

---

## Reference Files

- DAVE generated files: `DAVE/GENERATED/` (don't edit)
- Your code: `src/encoder_pwm.c` or `src/encoder_spi.c`
- Linker: auto-managed by DAVE
- Startup: auto-generated

---

## Common DAVE Mistakes to Avoid

1. **Forgot to Add App:** APP not dragged into project → no compiler path
   - Solution: Drag CCU4 app onto project canvas, save/generate

2. **Wrong Pin Selected:** P1.1 vs P1_1 confusion
   - Solution: Use DAVE GUI pin selector, don't type manually

3. **Generate Code Not Run:** Changed config but didn't hit Ctrl+G
   - Solution: Always right-click project → Generate Code after config changes

4. **Callback Not Wired:** ISR defined but not called
   - Solution: DAVE auto-registers callbacks; verify auto-call in generated code

---

## Status

✅ **PWM encoder capture guide complete**
✅ **SPI encoder guide included (reference)**

Ready to implement in DAVE and commission encoder.
