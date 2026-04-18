# DAVE PWM_CCU8 Integration Guide (Dynamic HIL Mode)

**Date:** 17-04-2026  
**Topic:** Integrating HIL firmware with dynamic PWM duty cycles

---

## Overview

You're converting from **static 50% duty** (15_04_2026) to **dynamic duty cycles** (16_04_2026) driven by FOC algorithm.

### What Stays THE SAME
✅ PWM_CCU8 frequency: **20 kHz**  
✅ Period register: **7200 counts**  
✅ Dead time: **97.2 ns (14 counts)**  
✅ Output enable: **P1.11 (CH1 Inverted)**  
✅ All complementary output settings  

### What Changes
🔄 Duty cycle source: **Static (3600) → Dynamic (hil.duty_a)**  
🔄 Main loop: **Polling → 20 kHz ISR-driven**  
🔄 Update mechanism: **Manual → Automatic in ISR**  

---

## DAVE Configuration (No Changes Needed!)

Your 15_04_2026 DAVE project configuration is **perfect as-is**:

```
General Settings (UNCHANGED):
  Frequency: 20000 Hz
  Prescaler: 1
  Period: 7200 counts
  Counting Mode: Center-Aligned

Signal Settings (UNCHANGED):
  Dead Time Ch1: 14 counts (97.2 ns)
  Dead Time Ch2: 14 counts
  Ch1 Direct Passive: Before Compare Match
  Ch1 Inverted Passive: After Compare Match
  
Pin Settings (UNCHANGED):
  P1.5 (CH1 Direct): Enabled
  P1.11 (CH1 Inverted): Enabled ← PulseView connects here
  P2.x (CH2 outputs): For future 3-phase
```

**Action:** Copy the 15_04_2026 DAVE project → create 16_04_2026 project  
No DAVE app changes needed!

---

## Firmware Integration

### Step 1: Copy DAVE Project Template
```
Copy folder: 15_04_2026/15_04_26_PWM_CCU8 → 16_04_2026/16_04_26_HIL_PWM_CCU8
```

### Step 2: Add HIL Files
Into `16_04_26_HIL_PWM_CCU8/` add:
```
src/
  ├── main.c ← NEW (provided above)
  ├── motor_model.c
  ├── motor_model.h
  ├── inverter_model.c
  ├── inverter_model.h
  ├── transforms.h
  ├── foc_algorithm_xmc.c
  ├── foc_algorithm_xmc.h
  ├── main_hil.c
  └── main_hil.h
```

### Step 3: ISR Configuration

The new `main.c` uses **CCU8_0_IRQHandler** for 20 kHz execution:

```c
void CCU8_0_IRQHandler(void) {
    hil_step_foc(&hil);                          // FOC + Motor
    PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, 
                           (uint16_t)hil.duty_a);  // Update PWM
}
```

**DAVE will automatically route this ISR when PWM_CCU8_0 is configured.**

### Step 4: Compile & Flash

```bash
Right-click project → Build Project
Connect XMC debugger → Flash
```

---

## Data Flow: Static vs. Dynamic

### OLD (15_04_2026): Static 50% Duty
```
main.c polling loop
    ↓
Fixed duty_cycle_value = 3600
    ↓
PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, 3600)
    ↓
Hardware PWM P1.11 (constant 50%)
    ↓
[PulseView sees constant 50% square wave]
```

### NEW (16_04_2026): Dynamic Duty from FOC
```
20 kHz CCU8_0 ISR (triggered by PWM period match)
    ↓
hil_step_foc(&hil)
    ├─ FOC calculates v_d_ref, v_q_ref
    ├─ Inverter transforms to abc voltages
    ├─ PWM mapper → hil.duty_a [0..6000]
    ├─ INTERNAL: duty_a → inverter model → motor model
    │   [Motor feedback closes loop internally]
    └─ Returns new duty_a
    ↓
PWMSP_CCU8_SetDutyCycle(&PWM_CCU8_0, hil.duty_a)
    ↓
Hardware PWM P1.11 (varies 0–6000 per FOC)
    ↓
[PulseView sees dynamic PWM - wider at high speed]
    ↓
DUAL LOOPBACK:
  1. Hardware: P1.11 → oscilloscope (visibility)
  2. Internal: duty_a → inverter model → motor model → FOC
```

---

## Pin Assignments

### P1.11 (CH1 Inverted) - OBSERVE
```
GPIO: P1.11 (XMC4700 Eval Board)
Connection: → Oscilloscope Channel A (PulseView)
Signal: 20 kHz PWM, duty 0–100% (from FOC)
Expected: Smooth varying square wave (not just 50%)
```

### P1.5 (CH1 Direct) - AVAILABLE
```
GPIO: P1.5
Connection: (future complementary output or monitor)
Signal: Complement of P1.11 with 97.2 ns dead time
```

### P2.5, P2.6 (CH2 outputs) - FUTURE
```
Future 3-phase PWM (when board headers available)
For now: Can be left unconnected
```

---

## Motor Feedback Loopback (Internal)

**How the duty cycles feedback into the motor model:**

```
hil_step_foc() execution:
    
    ↓ [Calculate duty_a, duty_b, duty_c]
    
    hil.duty_a = computed value  [0..6000]
    
    ↓ [Store in hil structure]
    
    ↓ [Later in hil_step_foc, same duty values used:]
    
    inverter_pwm_to_voltage(&hil.inverter, 
                            hil.duty_a, 
                            hil.duty_b, 
                            hil.duty_c);
    
    ↓ [Converts duty → abc voltages]
    
    ↓ [Clarke/Park transforms → dq voltages]
    
    ↓ [motor_step(&hil.motor, vd, vq)]
    
    ↓ [Motor integrates: i_d, i_q, ω, θ]
    
    ↓ [Feedback extracted: id_foc, iq_foc, speed_rpm]
    
    ↓ [Back to FOC for next cycle]
```

**Result:** Complete closed loop without external hardware!

---

## Oscilloscope (PulseView) Setup

### Capture Configuration
```
Trigger: CH A (P1.11)
  - Level: 50% (mid-supply)
  - Edge: Rising
  - Mode: Auto

Channel A (P1.11):
  - Name: "PWM_CH1_INV"
  - Samples: 1 μs/div (to see full 50 μs = 20 kHz period)
  - Voltage: 0–3.3V (logic level)

Time Base:
  - 1–5 μs/div (to capture complete PWM cycles with duty variation)
```

### Expected Waveforms

**Before FOC enabled (initialization):**
```
___
| |___| |___ [Constant waveform]
  ↑   ↑   ↑
  ← 50 μs → (20 kHz period)
  50% duty throughout
```

**After FOC step enabled (dynamic):**
```
____        ___
|  |___|___|   |__  [Duty varies with speed ref]
```

**At high speed (e.g., 300 RPM):**
```
______          __
|    |___|___|_|  |___ [Duty wider, following FOC control]
```

**When motor speed overshoots:**
```
_       _______
| |___| |      |_ [Duty narrows as speed decreases]
```

---

## Telemetry Output (UART)

The new `main.c` outputs telemetry at 100 Hz via UART:

```
=== XMC4700 HIL STARTUP ===
Motor: pp=11, Kv=141.4, max_flux=0.1628 Wb
HIL initialized: Vdc=59.4V
PWM started: 20 kHz, P1.11 output enabled
Initial PWM duty: 50%

>>> HIL running - scenarios will execute automatically
>>> Connect P1.11 to PulseView for PWM visualization
>>> Motor internally simulated and fed back to FOC

[    0 ms] RPM:  0.00  Id: 0.0000  Iq: 0.0000  Te: 0.00000  θ: 0.0000
...
[ 1000 ms] RPM:150.00  Id: 0.0001  Iq: 0.0300  Te: 0.01000  θ: 1.5708
[ 2000 ms] RPM:280.00  Id:-0.0002  Iq: 0.0250  Te: 0.00833  θ: 3.1416
```

---

## Testing Checklist

### Pre-Compile
- [ ] All .h and .c files copied to src/
- [ ] main.c includes `#include "main_hil.h"`
- [ ] DAVE project uses 20 kHz PWM config
- [ ] P1.11 enabled in DAVE PWM_CCU8 settings

### Post-Compile
- [ ] No compilation errors
- [ ] main.c builds cleanly with HIL files
- [ ] Flash to XMC4700

### Hardware Test
- [ ] Connect UART → PC (9600 baud)
- [ ] See startup messages in serial monitor
- [ ] Connect P1.11 → PulseView
- [ ] Observe 20 kHz PWM signal (initially ~50% duty)
- [ ] Watch duty cycle change dynamically over time
- [ ] Telemetry shows speed increasing 0 → 300 RPM at t=1s
- [ ] Load applied at t=3s causes speed dip in telemetry

### Scenario Validation
- [ ] **Test 1 (t=0–1s):** Speed = 0, PWM duty ~3000 (50%)
- [ ] **Test 2 (t=1–5s):** Speed ramps to 300 RPM, PWM duty varies
- [ ] **Test 3 (t=3–4s):** Load applied, speed dips briefly, recovers
- [ ] **Test 4 (t=5–10s):** Speed steps to 100 RPM, PWM duty adjusts
- [ ] **Test 5 (t>10s):** Speed returns to 0, PWM duty returns to ~3000

---

## Comparison: Old vs. New

| Aspect | 15_04_2026 (Static) | 16_04_2026 (Dynamic HIL) |
|--------|-------------------|-------------------------|
| **Duty source** | Hardcoded 3600 | FOC algorithm (hil.duty_a) |
| **Execution** | Polling main loop | 20 kHz ISR |
| **Motor model** | None | Simulated dq motor |
| **Feedback** | None | Closed-loop internal |
| **PWM visibility** | Constant 50% | Dynamic 0–100% |
| **Test capability** | Visual only | Full control algorithm validation |
| **Next phase** | Hardware motor | Real motor + encoder |

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| PWM doesn't change | duty_a not calculated | Check hil_step_foc() runs in ISR |
| PWM stuck at 50% | ISR not triggered | Verify DAVE PWM_CCU8 config + ISR routing |
| Telemetry not updating | UART not initialized | Check DAVE UART_0 enabled |
| Motor doesn't accelerate | Load torque too high or FOC gains wrong | Reduce T_load or check PI gains in foc_algorithm_xmc.c |
| Oscillations wild | Speed PI Ki too high | Reduce SPEED_KI in foc_algorithm_xmc.c |

---

## Next Phase: Real Motor Connection

Once this HIL validates correctly, add:
1. **Encoder feedback** (CCU4 capture) → θ_ele input
2. **Current sensors** (ADC) → id, iq feedback
3. **3-phase PWM outputs** (P2.x) → all three phases
4. **Physical motor** connected to inverter

The C firmware, FOC algorithm, and transforms will **reuse without changes** — just swap the motor model for real motor interface.

---

**Ready to compile and test! 🚀**
