# XMC4700 PWM Configuration for PMSM FOC — DAVE IDE Setup (CCU8)

**Purpose:** Configure 20 kHz 3-phase sine-triangle PWM with 100 ns dead time using **CCU8 (Capture/Compare Unit 8)** — hardware dead time generator for motor control.  
**Target Hardware:** XMC4700 Relax Lite Kit (Cortex-M4, 120 MHz)  
**FOC Specifications:** Speed loop 200 Hz BW, Current loop 2 kHz BW, Vdc = 59V (sine-triangle modulation)  
**Timer Module:** CCU8 (native 3-phase motor control support, built-in dead time hardware)

---

## Part 1: DAVE PWM App Configuration

### Step 1.1: Initial Setup in DAVE

1. **Create/Open Project**
   - File → New → XMC Microcontroller Project
   - Board: Infineon XMC4700 Relax Lite Kit
   - Project name: `PMSM_FOC_XMC4700`

2. **Add PWM App for CCU8 (PWMSP_CCU8)**
   - Drag and drop **PWMSP_CCU8** app from Library → Timers section (or **PWMSP** with CCU8 module selected)
   - Configure **3 instances** (one per phase/leg: U, V, W)
   - DAVE will auto-route to CCU80 slices 0, 1, 2 (one phase per slice)

---

### Step 1.2: Clock & Timer Configuration (All Three PWM Instances)

| Setting | Value | Rationale |
|---------|-------|-----------|
| **Module** | CCU8 (Capture/Compare Unit 8) | **Motor-optimized:** Hardware dead time, better slicing |
| **Slice Assignment** | Slice 0, 1, 2 on CCU80 (or auto) | One per phase: U, V, W |
| **Input Clock** | Prescaler = 1 (120 MHz → 120 MHz) | Maximum resolution |
| **PWM Period** | 6000 counts | See calculation below |
| **PWM Frequency** | 20 kHz | Matches simulation & control loop |
| **Shadow Registers** | ✓ Enabled | Synchronous duty update across all slices |

**Frequency Calculation:**  
```
PWM_Frequency = 20 kHz = 50 µs period
Clock = 120 MHz
PWM_Period_counts = (120 MHz) / (20 kHz) = 6000 counts
```

---

### Step 1.3: Dead Time Configuration (CCU8 Hardware Dead Time Generator)

| Setting | Value | Rationale |
|---------|-------|-----------|
| **Dead Time Enable** | ✓ Enabled | **CCU8 native dead time hardware** (no logic workarounds needed) |
| **Dead Time Value** | 12 counts | 12 × (1/120 MHz) = 100 ns (shoot-through protection) |
| **Dead Time Mode** | Asymmetric (Rising Delay) | Rising edge delayed by 100 ns, falling edge original |
| **Apply to All Slices** | ✓ Yes | Dead time same for U, V, W phases |

**Dead Time Reasoning:**
- Hardware requirement: 50–200 ns minimum (gate driver + MOSFET propagation delay)
- **We use 100 ns** (middle of range, proven in simulation via dead time validation in session_10-04-2026.md)
- **Clock for dead time:** Same 120 MHz clock = 8.33 ns per count
- **100 ns = 12 counts** (100 / 8.33 ≈ 12)
- **CCU8 Advantage:** Hardware dead time generator automatically applies to complementary outputs (high-side, low-side) — no manual SR FlipFlop logic needed

---

### Step 1.4: Output Compare Mode (All Three Instances)

| Setting | Value |
|---------|-------|
| **Compare Mode** | Edge-Aligned PWM (symmetric modulation) |
| **Output Polarity** | ✓ Normal (Q output drives high-side, Q' drives low-side) |
| **Passive Level** | 0 (Low when PWM off) |

---

### Step 1.5: Synchronization Between All Three PWM Instances (CCU8 Native)

1. **Slice Synchronization (CCU8 Built-In):**
   - DAVE automatically synchronizes CCU80 slices 0, 1, 2 when configured in 3-phase mode
   - Enable **Synchronous Start/Reload Mode**: All slices share same counter clock
   - This ensures U, V, W phases have **identical counter positions** at all times
   - Result: Perfect 120° phase spacing without manual tricks

2. **Start Trigger in Firmware:**
   - Use DAVE-generated initialization: `PWMSP_CCU8_Init()`
   - Call once at startup: `PWMSP_CCU8_Start()`
   - All three slices start and remain synchronized throughout operation

---

## Part 2: Control Loop Integration

### Step 2.1: PWM Update Timing

**Critical:** PWM duty cycle must update **at the same frequency as the control loop**.

| Loop | Frequency | Period | Alignment |
|------|-----------|--------|-----------|
| Current Control (dq) | 20 kHz | 50 µs | Each PWM period |
| Speed Control (outer) | 2 kHz | 500 µs | Every 10 PWM periods |
| Simulator | 20 kHz | 50 µs | Matches hardware |

**Implementation in XMC4700 Code:**

```c
// In main.c or FreeRTOS task (20 kHz interrupt-driven)
void PWM_20kHz_ISR(void) {
    // 1. Read feedback currents (I_alpha, I_beta)
    float I_alpha = ADC_Read_Phase_A();
    float I_beta = ADC_Read_Phase_B();
    
    // 2. Transform to dq frame (Clarke + Park)
    dq_currents = clarke_transform(I_alpha, I_beta);
    dq_currents = park_transform(dq_currents, theta_e);
    
    // 3. Run current control loops
    v_d = current_PI_d(dq_currents.d - iq_ref);
    v_q = current_PI_q(dq_currents.q - iq_ref);
    
    // 4. Enforce voltage circle (saturation)
    v_mag = sqrt(v_d*v_d + v_q*v_q);
    if (v_mag > VMAX) {
        v_d = v_d * VMAX / v_mag;
        v_q = v_q * VMAX / v_mag;
    }
    
    // 5. Inverse Park transform (back to αβ frame)
    v_alpha_beta = inverse_park_transform(v_d, v_q, theta_e);
    
    // 6. Sine-Triangle PWM modulation (or SVPWM)
    duty_u = sine_triangle_modulation(v_alpha_beta.alpha, v_alpha_beta.beta, 0);
    duty_v = sine_triangle_modulation(v_alpha_beta.alpha, v_alpha_beta.beta, 120°);
    duty_w = sine_triangle_modulation(v_alpha_beta.alpha, v_alpha_beta.beta, 240°);
    
    // 7. Update PWM duty cycles (shadow registers)
    PWM_SET_DUTY_U(duty_u);
    PWM_SET_DUTY_V(duty_v);
    PWM_SET_DUTY_W(duty_w);
}

// In separate 2 kHz speed loop (every 10th PWM cycle)
void SPEED_2kHz_ISR(void) {
    // Read speed estimate (from encoder or observer)
    float omega_e = ENCODER_Read_Speed();
    
    // Speed error
    float speed_error = speed_ref - omega_e;
    
    // Speed PI control
    iq_ref = speed_PI(speed_error);
    
    // Anti-windup
    if (iq_ref > 1.0) iq_ref = 1.0;
    if (iq_ref < 0) iq_ref = 0;
    
    // d-axis current always = 0 (MTPA for non-salient motor)
    id_ref = 0;
}
```

---

### Step 2.2: PWM Duty Cycle Update Mechanism

**Two approaches:**

#### **Option A: PWM Compare Register Direct (Fastest)**
```c
#define PWM_PERIOD_COUNTS 6000
#define PWMU_CMP (CCU80_CC80)  // CCU8 Slice 0, Compare register

void PWM_SET_DUTY_U(float normalized_duty) {
    // normalized_duty: 0.0 to 1.0
    uint16_t compare_val = (uint16_t)(normalized_duty * PWM_PERIOD_COUNTS);
    
    // Write to shadow register (takes effect at PWM period match/reload)
    PWMU_CMP->CR1S = compare_val;  // Note: CCU8 uses CR1S for shadow write
}
```

#### **Option B: DAVE PWM App API (Simpler, auto-handles dead time)**
```c
#include <xmc_pwm.h>
// DAVE auto-generated header for CCU8
#include "pwmsp_ccu8.h"

void PWM_SET_DUTY_U(float normalized_duty) {
    PWMSP_SetCompareValue(pwm_handle_u, (uint16_t)(normalized_duty * 6000));
    // CCU8 hardware dead time automatically applied to both high-side and low-side
}
```

**Recommendation:** Use Option B (DAVE API) — it abstracts CCU8 complexity and auto-applies dead time. Option A is rarely needed.

---

## Part 3: Modulation Algorithms

### Step 3.1: Sine-Triangle PWM (Recommended for FOC)

```c
float sine_triangle_modulated_duty(
    float v_alpha,      // Voltage ref in α direction
    float v_beta,       // Voltage ref in β direction
    float phase_offset  // 0°, 120°, or 240° for U, V, W
) {
    // Phase voltage reference magnitude
    float v_mag = sqrt(v_alpha*v_alpha + v_beta*v_beta);
    
    // Park transform to get phase angle
    float phase = atan2(v_beta, v_alpha);
    
    // Get sinusoidal reference for this phase
    float phase_shifted = phase + phase_offset;
    float sine_ref = sin(phase_shifted);
    
    // Normalize to PWM range [0, 1]
    // Vmax = Vdc/2 (sine-triangle max), so divide by Vmax
    #define VMAX 29.5  // Half of 59V Vdc with margin
    float duty = 0.5 + (0.5 * v_mag * sine_ref / VMAX);
    
    // Clamp to [0, 1]
    if (duty > 1.0) duty = 1.0;
    if (duty < 0.0) duty = 0.0;
    
    return duty;
}
```

### Step 3.2: Space Vector PWM (SVPWM) Alternative

SVPWM provides 15% more voltage utilization but is more complex. Implementation:

```c
float svpwm_duty(
    float v_alpha,
    float v_beta,
    float phase_offset  // 0°, 120°, 240°
) {
    // Rotating frame: find which of 6 sectors we're in
    float phase = atan2(v_beta, v_alpha);
    int sector = (int)(phase / 60.0);  // 0-5
    
    // Calculate active vector times T1, T2
    // (See SVPWM derivation: SVM lookup tables)
    float v_mag = sqrt(v_alpha*v_alpha + v_beta*v_beta);
    float T1 = ..., T2 = ...;  // Sector-specific calculations
    
    // Distribute T1, T2 among phase legs
    // Apply phase_offset to rotate for U, V, W
    float duty = 0.5 + ...;  // Sector-dependent
    
    return duty;
}
```

**For now: Use Sine-Triangle PWM** (simpler, 87% voltage utilization sufficient).

---

## Part 4: Hardware Connections

### Step 4.1: GPIO Pin Mapping (XMC4700 Relax Lite)

| PWM Output | XMC4700 Pin | Physical Board | Gate Driver Connection |
|------------|------------|-----|-----|
| **PWM_U_HIGH** | P1.0 (CCU80.OUT0) | J3 Pin 8 | Gate driver In1 (U-phase high-side) |
| **PWM_U_LOW** | P1.1 (CCU80.OUT0 inverted) | J3 Pin 9 | Gate driver In1' (U-phase low-side) |
| **PWM_V_HIGH** | P1.4 (CCU80.OUT1) | J3 Pin 10 | Gate driver In2 (V-phase high-side) |
| **PWM_V_LOW** | P1.5 (CCU80.OUT1 inverted) | J3 Pin 11 | Gate driver In2' (V-phase low-side) |
| **PWM_W_HIGH** | P1.8 (CCU80.OUT2) | J3 Pin 12 | Gate driver In3 (W-phase high-side) |
| **PWM_W_LOW** | P1.9 (CCU80.OUT2 inverted) | J3 Pin 13 | Gate driver In3' (W-phase low-side) |

**In DAVE:**
- Assign P1.0, P1.1, P1.4, P1.5, P1.8, P1.9 to PWMSP_CCU8 app outputs (auto-routed if using library defaults)
- Enable **complementary outputs** (high/low pairs) for each slice
- Verify dead time applied to all complementary pairs in DAVE configuration

### Step 4.2: Current Feedback Connections

| Phase | Shunt Sense Pin | ADC Channel | A/D Module |
|-------|-------|-----|-----|
| **Phase A** | P14.6 (0.5V per 1A) | VADC_GROUP0, CH6 | 200 kHz sampling |
| **Phase B** | P14.7 (0.5V per 1A) | VADC_GROUP0, CH7 | 200 kHz sampling |
| **Encoder** | P1.15 (Timer Inp) | CCU40 Slice 3 | Up/Down counter |

---

## Part 5: Validation Checklist

### Hardware Bring-Up

- [ ] **Oscilloscope probe on PWM_U_HIGH (P1.0)**: Should see square wave, 20 kHz, 0–3.3V
- [ ] **Dead time visible?** Measure time between U_HIGH falling edge and U_LOW rising edge → should be ~100 ns (CCU8 hardware-generated)
- [ ] **Phase alignment:** V & W PWM should be 120° out of phase with U (each shifted by 16.67 µs out of 50 µs period)
- [ ] **Dead time gap:** All three phases should show small gap (~100 ns) between high-side OFF and low-side ON

### Functional Tests

- [ ] **No-load spin-up:** Apply step speed command 0 → 2262 RPM, motor should ramp smoothly
- [ ] **Settling time:** Speed should settle to ±1 RPM within ~7 seconds
- [ ] **Current limiting:** Phase current should not exceed 1.0 A (soft limit enforced)
- [ ] **Thermal:** Motor temp < 80°C after 10 min continuous run at 1 A

### Software Validation

- [ ] **PWM frequency accurate?** Measure carrier with logic analyzer: Should be exactly 20 kHz ±0.1%
- [ ] **Control loop latency:** Time from current sampling to PWM update < 2.5 µs (1/8 of PWM period)
- [ ] **No glitches:** PWM outputs should never have both U_HIGH and U_LOW ON simultaneously (CCU8 dead time hardware prevents this)

---

## Part 6: Code Template (Minimal Example)

### File: `pwm_driver.h`

```c
#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdint.h>

typedef struct {
    float duty_u;
    float duty_v;
    float duty_w;
} PWM_Output;

void PWM_Init(void);
void PWM_UpdateDuty(float duty_u, float duty_v, float duty_w);
void PWM_Start(void);
void PWM_Stop(void);

#endif
```

### File: `pwm_driver.c`

```c
#include "pwm_driver.h"
#include "xmc_pwm.h"

#define PWM_PERIOD 6000

void PWM_Init(void) {
    // DAVE-generated initialization (auto-generated from project)
    // CCU8 configuration with hardware dead time
    PWMSP_Init(&pwm_config);  // Single call for all 3 CCU8 slices
}

void PWM_UpdateDuty(float duty_u, float duty_v, float duty_w) {
    // Clamp duties to [0, 1]
    if (duty_u > 1.0f) duty_u = 1.0f;
    if (duty_u < 0.0f) duty_u = 0.0f;
    if (duty_v > 1.0f) duty_v = 1.0f;
    if (duty_v < 0.0f) duty_v = 0.0f;
    if (duty_w > 1.0f) duty_w = 1.0f;
    if (duty_w < 0.0f) duty_w = 0.0f;
    
    // Update PWM compare shadow registers (CCU8)
    // Dead time automatically applied by hardware
    CCU80_CC80->CR1S = (uint16_t)(duty_u * PWM_PERIOD);  // U (Slice 0)
    CCU80_CC81->CR1S = (uint16_t)(duty_v * PWM_PERIOD);  // V (Slice 1)
    CCU80_CC82->CR1S = (uint16_t)(duty_w * PWM_PERIOD);  // W (Slice 2)
}

void PWM_Start(void) {
    CCU80->GCTRL |= 0x001;  // Start all CCU8 slices
}

void PWM_Stop(void) {
    CCU80->GCTRL &= ~0x001;  // Stop all CCU8 slices
}
```

---

## Part 7: Quick Troubleshooting (CCU8-Specific)

| Issue | Cause | Solution |
|-------|-------|----------|
| PWM duty cycle not updating | Shadow register not reloading | Ensure shadow (CR1S) write happens; reload occurs at counter match |
| Shoot-through detected (current spike) | Dead time setting wrong or not applied | Verify CCU8 dead time register = 12 counts; confirm applied to all slices |
| Uneven phase currents | Slices not synchronized | Verify CCU80 slices 0,1,2 sync'd in DAVE config (native support) |
| Motor won't start | PWM frequency or dead time issue | Verify 20 kHz on scope; check dead time gap ~100 ns between high/low |
| High current ripple | ADC not sync'd to PWM or control timing | Ensure ADC trigger from PWM period match; verify 20 kHz ISR is called |

---

## References

- **XMC4700 Reference Manual:** CCU8 timer section (dead time generation, slices 0–7)
- **DAVE Documentation:** PWMSP_CCU8 app configuration guide
- **FOC Simulation:** session_10-04-2026.md (dead time validation, applies to both CCU4/CCU8)
- **Motor Parameters:** motor_constants.md (Vdc budget, current limits)
- **Datasheet:** Infineon XMC4700 User Manual, Chapter 24 (CCU8)

---

**Status:** ✅ Ready for XMC4700 firmware implementation (CCU8)  
**Next Step:** Import DAVE PWM configs, integrate with ADC sampling & current PI loops
