# XMC4700 PWM Bring-Up: Common Issues & Solutions (CCU8)

**Quick Reference:** Use this to diagnose PWM/FOC issues during firmware development.  
**Timer Module:** CCU8 (hardware dead time generator for 3-phase motor control)

---

## Issue 1: PWM Outputs Not Appearing (No Square Wave)

### Symptoms
- Oscilloscope shows 0V on P5.8–P5.13 (PWM pins)
- Motor doesn't spin
- No sound from inverter gate drivers

### Root Causes & Solutions

| Cause | Check | Fix |
|-------|-------|-----|
| PWM not started | `CCU80->GCTRL & 0x001` | Call `CCU80->GCTRL \|= 0x001` in init |
| Clock not enabled | DAVE PWM app global settings | Enable CCU8 in DAVE → Global Config |
| Pin not configured as PWM | DAVE port routing | Verify P1.0–P1.9 assigned to PWMSP_CCU8 outputs |
| PWM period = 0 | Check `CCU80_CC8x->PER` register | Set to 6000 (120 MHz / 20 kHz) |
| GPIO override | Port P1 control | Disable GPIO override on PWM pins |

**Verification:**
```c
// In debugger, inspect:
CCU80->GCTRL           // Should have bit 0 = 1 (enabled)
CCU80_CC80->PER        // Should = 6000 (50 µs period)
CCU80_CC80->CR1S       // Should change 0→5999 (duty update)
```

---

## Issue 2: Duty Cycle Not Updating (PWM Frozen)

### Symptoms
- PWM square wave present but duty cycle stuck (e.g., always 50%)
- Manually setting duty via code has no effect
- Speed control not responding

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| Shadow register not reloading | Is `CR1S` write having effect? | Trigger PWM reload: low-to-high transition on period match |
| Duty cycle out of range | Is duty_u/v/w in [0.0, 1.0]? | Clamp values: `min=0, max=1` |
| Write to wrong register | Check write location | Use `CCU8x_CCy->CR1S` (shadow register for CCU8) |
| PWM period too short | `PER < 1000`? | Increase to 6000 (20 kHz = 50 µs) |
| Anti-windup saturation | Check PI integrator state | Verify `pi.output_limit` allows 0–1 duty range |

**Verification:**
```c
// Read shadow register before write:
uint16_t cr1s_before = CCU80_CC80->CR1S;

// Write new duty:
CCU80_CC80->CR1S = (uint16_t)(0.5 * 6000);  // 50% duty

// Poll until updated:
while (CCU80_CC80->CR1S == cr1s_before) {
    // Wait for reload... should update within 50 µs
}
```

---

## Issue 3: Dead Time Not Working (Shoot-Through Detected)

### Symptoms
- Power supply overcurrent alarm trips
- Black smoke from MOSFETs
- Gate driver signals show high-high-high on same leg for microseconds
- High-frequency ringing on motor phase voltages

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| Dead time = 0 | Check CCU8 dead time register (DTR) | Set to 12 counts (100 ns @ 120 MHz) |
| Dead time not applied | DAVE config: dead time enabled for all slices? | ✅ Enable dead time in PWMSP_CCU8 app for slices 0, 1, 2 |
| Dead time too short | Is 100 ns not enough? | Increase to 200 ns (24 counts) |
| Dead time hardware disabled | Check CCU8 control register bits | Verify DEAD_TIME_ENABLE bit set in CCU8 DTR register |
| Gate driver propagation delay not accounted for | Measured gate-to-gate delay < 100 ns? | Add gate driver spec sheet dead time to PWM setting |

**Verification:**
```
Use logic analyzer on high/low gate signals (CCU8 auto-generates dead time gap):
┌─────────┬──────────┬──────────┐
│  CCU8   │  Ideal   │  Actual  │
│ Setting │ (100ns)  │ Hardware │
├─────────┼──────────┼──────────┤
│ Q high  │    ↓     │   (gap)  │
│ Q' low  │    ↓     │     ↓    │
└─────────┴──────────┴──────────┘

The gap should be ≥ 100 ns (CCU8 hardware-generated, no logic needed).
```

**Measurement on Oscilloscope:**
```
Trigger: U_HIGH (P1.0) falling edge
Time cursor A: On U_HIGH falling edge
Time cursor B: On U_LOW (P1.1) rising edge
Δt should be ≥ 100 ns (confirms CCU8 dead time working)
```

---

## Issue 4: Phase Misalignment (Voltages Not 120° Apart)

### Symptoms
- Motor vibrates or produces noise
- Uneven current distribution (I_a ≠ I_b ≠ I_c)
- Speed ripple (±10 RPM or more)
- FOC stability poor

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| PWM slices not synchronized | Are U, V, W counters starting at same clock cycle? | Enable CCU8 synchronization (DAVE auto-syncs slices 0,1,2 in 3-phase mode) |
| Duty calculation wrong | Verify modulation formula | Use `duty = 0.5 + 0.5*(v_mag*sin(phase+offset))/Vmax` |
| Park transform angle wrong | Is `theta_e` signed correctly? | Verify encoder rotation direction + pole pairs |
| Phase offset calculation wrong | Expected 16.67 µs (120° out of 50 µs period) | Measure actual delay: should be 16.67 µs (120° of 50 µs) |

**Verification (Oscilloscope):**
```
Trigger: U_HIGH (P1.0) rising edge
Cursor: Measure time to V_HIGH (P1.4) and W_HIGH (P1.8) rising edges
Expected:
  V rising = 16.67 µs after U  (120° = 50 µs/3)
  W rising = 33.33 µs after U  (240° = 2×50 µs/3)
```

---

## Issue 5: Current Measurements Noisy (±0.3 A ripple)

### Symptoms
- ADC readings fluctuate wildly (i_a drifts 0.3–0.7 A)
- PI controller output oscillates
- Speed loop unstable
- Telemetry shows spikes in current

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| ADC not synchronized to PWM | Trigger from PWM edge or free-run? | **Enable PWM sync**: ADC samples at PWM period match |
| Sampling during PWM switching | ADC reads during gate driver transition | Delay ADC by 5–10 µs after PWM edge (PWM mid-period sampling) |
| Shunt resistor value error | Multimeter check: is shunt actually 0.1 Ω? | Verify physical part or pre-measure resistance |
| Op-amp gain wrong | Amplifier spec says 20× or 50×? | Recalculate ADC_SCALE_IA with correct gain |
| ADC filter not applied | Is low-pass still disabled? | Enable ADC_LowPassFilter(...) in ADC_ReadCurrents() |
| Noisy power supply to op-amp | Measure Vcc ripple on scope | Add 100 nF bypass cap near op-amp supply pins |

**Quick Fix (Software):**
```c
// Increase filter coefficient to reduce noise
#define ADC_FILTER_ALPHA 0.5f  // Stronger filtering (was 0.8)
// Trade-off: slower response (now ~100 µs lag)
```

**Measurement:**
```
Oscilloscope on ADC input (P14.6):
- Should see PWM-frequency ripple (20 kHz) superimposed on DC
- Peak-to-peak ripple: ±20 mV (0.1 A equivalent)
- If > ±100 mV: problem detected
```

---

## Issue 6: Motor Speed Not Following Reference (Won't Accelerate)

### Symptoms
- Speed stuck at 0 RPM even with speed_ref = 2262 RPM
- Motor hums but doesn't spin
- Current loop working (PWM modulation visible) but no torque

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| Encoder not connected | Logic analyzer on encoder lines | Verify encoder power, CAN, or SSI signal present |
| Encoder angle wrong | Read `theta_e` in debugger: always same value? | Confirm mechanical rotation changes `theta_e` |
| Park transform inverted | Is motor spinning backwards? | Check encoder direction: CW should increase angle |
| Speed reference not set | Is `speed_ref` still 0? | Call `FOC_SetSpeedRef(2262)` before ISR starts |
| PWM duty never exceeds 10% | Check telemetry: `duty_u, duty_v, duty_w` | Inspect voltage circle saturation: is `v_mag` getting clipped? |
| Motor torque constant wrong | Verify `MOTOR_TORQUE_CONST` formula | Use λm = 0.00611 Wb, P = 11 |

**Verification (Debugger):**
```c
// Add breakpoint in FOC_CurrentControl_20kHz():
printf("theta_e=%f, i_a=%f, i_b=%f, v_q=%f, duty_u=%f\n",
       theta_e, i_dq.d, i_dq.q, v_dq.q, duty_u);
// Expected (at steady-state 1A):
// v_q ≈ 24 V (from Kp*iq_err + Ki*integral)
// duty_u ≈ 0.41 (24V / (29.5V max))
```

---

## Issue 7: Inconsistent Results Between Simulink & Hardware

### Symptoms
- Sim shows smooth speed ramp, hardware oscillates
- Settling time matches sim, but ripple ≠ sim
- Hardware drifts or has offset (speed 10 RPM off)

### Root Causes

| Cause | Check | Fix |
|-------|-------|-----|
| Solver mismatch | Sim uses fixed-step ode4; hardware solves in real-time | Verify **both** using 5µs timestep. Simulink timestep should match 1/(4×PWM_frequency) |
| Control gains different | Did you copy Kp/Ki from sim or recalculate? | **Use exact same math:** Kp=L×2π×BW, Ki=R×2π×BW |
| ADC quantization not modeled | Sim uses float, hardware 12-bit ADC | Add quantization to Simulink: Quantizer block set to 12-bit |
| Dead time not inserted in sim | Sim has dead time block, but is timing right? | Set Transport Delay = 100 ns (or 2 µs conservative) |
| Variable sample rate | Is hardware really running 20 kHz consistently? | Use scope to confirm PWM period = 50 µs ±1% |

---

## Quick Diagnostic Checklist

Use this before filing a support ticket:

- [ ] PWM present on all 6 pins (U, V, W high + low)?
- [ ] Duty cycles updating (not stuck at constant %)?
- [ ] Dead time visible (~100 ns gap between high/low on same leg)?
- [ ] Phase alignment correct (V 120° after U, W 240°)?
- [ ] Current measurements within ±0.1 A of expected?
- [ ] Encoder angle increases when shaft rotates CW?
- [ ] Speed reference properly initialized?
- [ ] Motor shaft rotates freely (no load lock)?
- [ ] Gate driver powered (+12V, GND good)?
- [ ] MOSFETs not hot (temperature < 80°C)?

---

## Recommended Test Sequence

1. **PWM Alive?** → Enable PWM, check for square waves
2. **Duty Updating?** → Manually set duty to 0.25, 0.5, 0.75 → verify waveform widths
3. **Dead Time OK?** → Inject 1 A step current, verify no shoot-through alarm
4. **Phase Alignment?** → Run 2 A test, measure phase currents with clamp meter (should be equal ±0.1 A)
5. **Encoder Working?** → Manually rotate shaft, verify speed changes smoothly
6. **FOC Stable?** → Ramp speed 0→1000 RPM, verify smooth tracking, no overshoot
7. **Full Power?** → Increase to target speed (2262 RPM), verify thermal limits respected

---

## Test Modes (Firmware Helper Functions)

```c
// Useful for bring-up (add to firmware)

void TEST_PWM_DutySweep(float sweep_period_sec) {
    // Ramp duty 0→1→0 for verification
    static float duty_cmd = 0.0f;
    static int dir = 1;
    
    duty_cmd += (dir * 0.001f);
    if (duty_cmd > 1.0f) { duty_cmd = 1.0f; dir = -1; }
    if (duty_cmd < 0.0f) { duty_cmd = 0.0f; dir = 1; }
    
    PWM_UpdateDuty(duty_cmd, duty_cmd, duty_cmd);
}

void TEST_SinusoidalVoltage(float frequency_hz, float amplitude) {
    // Three-phase sinusoid for smooth current verification
    static float time = 0;
    time += 1.0f / 20000.0f;  // 20 kHz update rate
    
    float phase = 2.0f * 3.14159f * frequency_hz * time;
    float v_u = amplitude * sinf(phase);
    float v_v = amplitude * sinf(phase - 2.0f*3.14159f/3.0f);
    float v_w = amplitude * sinf(phase - 4.0f*3.14159f/3.0f);
    
    // Convert voltages to duty and apply
    // (Modultion index = voltage / Vmax)
}
```

---

## References

- **Oscilloscope Capture Guide:** Trigger on PWM U_HIGH; use 10.4 MHz sampling (5 µs timebase, 2 divisions = 10 µs visible dead time region)
- **Logic Analyzer:** Capture at ≥ 10 MHz to resolve 100 ns dead time gaps
- **Multimeter (AC mode):** Measure PWM fundamental frequency on gate signals (should read ~20 kHz)
- **Thermal Imaging:** Check MOSFET junction temperature vs. heatsink baseline (ΔT ideal < 20°C)

**Debug Telemetry Variables to Log:**
- Current loop: `i_d, i_q, v_d, v_q, v_mag`
- Speed loop: `speed_ref, speed_meas, speed_error, iq_ref`
- PWM: `duty_u, duty_v, duty_w`
- Hardware: `theta_e, omega_e, adc_raw_ia, adc_raw_ib`
