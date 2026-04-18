# XMC4700 Hardware-in-Loop (HIL) Simulation with Real Encoder Feedback - 17_04_2026

## ⚠️ CRITICAL: Speed Controller Decimation Fix (17-04-2026)

**IMPORTANT:** The FOC algorithm includes a speed controller decimation counter that **MUST be present** for correct operation. See **"Speed Loop Decimation (2 kHz from 20 kHz)"** section in [foc_algorithm_xmc.c](foc_algorithm_xmc.c).

**Background:** The speed PI controller was designed for 2 kHz execution (Ts=0.0005), but `foc_step()` is called at 20 kHz from the ISR. Without decimation, the PI integrator gains are 10× too large → causes oscillations. This was identified and fixed in SIL testing ([see session_17-04-2026.md](session_17-04-2026.md) "SIL Oscillation Investigation & Decimation Fix").

**What Changed:** Added `speed_decimator` counter to execute speed PI only every 10th call. Hold `iq_ref` and `te_ref` during skip cycles.

**Do NOT disable or remove the decimation logic.** If oscillations appear during hardware testing, verify the decimator is active, not that PI gains need tuning.

---

## Quick Start

This folder contains a complete embedded C implementation of FOC + Motor + Inverter models for XMC4700 closed-loop testing. **Latest Update (17-04-2026):** Real encoder feedback integration on P1.1 (CCU4 capture) now operational.

### What's Inside

```
├── motor_model.h/.c          ← dq motor dynamics (5e-7 s timestep)
├── inverter_model.h/.c       ← 3-phase inverter (PWM → abc voltages)
├── transforms.h              ← Clarke/Park transformations
├── foc_algorithm_xmc.h/.c    ← FOC control (C port from MATLAB)
├── main_hil.h/.c             ← Top-level integration + encoder feedback
├── session_17-04-2026.md     ← Detailed documentation (includes encoder section)
├── DAVE_INTEGRATION_GUIDE.md ← Step-by-step DAVE project setup
└── IMPLEMENTATION_COMPLETE.md ← Milestone checklist
```

### Key Features

✅ **Multi-rate control:**
- FOC current loop: 20 kHz (CCU8_0)
- Speed loop: 2 kHz (decimated)
- Motor model: 5e-7 s (100 sub-steps per FOC cycle)

✅ **Real encoder feedback (NEW):**
- P1.1 CCU4 capture: PWM period/duty measurement
- Float math: (duty_ticks / period_ticks) × 360° → angle [deg]
- sprintf formatting: Proper float→ASCII conversion
- Integration ready: Can replace simulated feedback with real rotor angle

✅ **Closed-loop simulation (internal motor model):**
- FOC algorithm outputs v_d, v_q
- Inverter converts to PWM duty cycles (0–6000 counts)
- Motor model consumes dq voltages → produces id, iq, ω, θ
- Feedback closes loop

✅ **Scenario-based testing:**
- User-settable load torque (T_load parameter)
- User-settable speed reference (via UART or main.c)
- Real-time telemetry (speed, current, torque, angle, encoder angle)

### Usage Pattern

```c
// 1. Initialize
HIL_State hil;
hil_init(&hil, 59.4f, max_flux);

// 2. Call at 20 kHz (from timer ISR)
void TimerISR(void) {
    hil_step_foc(&hil);
    PWMSP_CCU8_SetDutyCycle(&PWM, (uint16_t)hil.duty_a);  // Output to real PWM pin
}

// 3. Read encoder (from CCU4 capture interrupt)
float encoder_angle_deg = get_encoder_angle();  // P1.1 CCU4 capture
// Later: Replace motor_rpm with encoder_rpm for closed-loop

// 4. Update parameters (low rate)
hil_set_speed_ref(&hil, 200.0f);     // RPM
hil_set_load_torque(&hil, 0.01f);    // N·m

// 4. Read telemetry (anytime)
float rpm = hil_get_speed_rpm(&hil);
float te = hil_get_torque(&hil);
```

### Integration Checklist

- [ ] Copy all .h and .c files to XMC DAVE project
- [ ] Add `#include "main_hil.h"` to main.c
- [ ] Call `hil_init()` in DAVE_Init() section
- [ ] Hook `hil_step_foc()` to 20 kHz PWM ISR
- [ ] Update speed/load reference from UART or main loop
- [ ] Compile and test telemetry output
- [ ] Compare traces with MATLAB SIL validation file
- [ ] Once 3-phase PWM available, enable duty_b, duty_c outputs

### Expected Behavior

**Step Speed from 0 → 200 RPM:**
```
t=0s:     ω=0,    id≈0,  iq≈0,   Te≈0
t=0.1s:   ω=50,   id≈0,  iq≈0.3, Te≈0.1  (ramp up)
t=0.5s:   ω=150,  id≈0,  iq≈0.3, Te≈0.1  (settling)
t=1.0s:   ω≈200,  id≈0,  iq≈0.25 Te≈0.08 (steady-state)
```

Small oscillations near setpoint acceptable (same as MATLAB SIL).

### Load Disturbance Test

```c
// At t = 2s, apply 0.01 N·m load
hil_set_load_torque(&hil, 0.01f);

// Expected: speed dips ~10 RPM, then recovers over ~200ms
```

### File Interdependencies

```
main_hil.c
  ├── #include motor_model.h
  ├── #include inverter_model.h
  ├── #include transforms.h
  ├── extern foc_algorithm_step()
  └── calls hil_step_foc() @ 20 kHz

foc_algorithm_xmc.c
  └── PI controller math (self-contained)

transforms.h
  └── All inline (Clarke/Park math)
```

### Tuning Notes

If motor behaves differently than MATLAB:
1. Check motor parameters (R, L, J, B) match Motor_Parameters.m
2. Verify PI gains are loaded correctly in `foc_init()`
3. Confirm timesteps (FOC=5e-5, motor_steps=100×5e-7)
4. Check inverter Vdc matches configuration (59.4V)

### Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Rotor angle wraps oddly | θ_ele wrapping issue | Check `motor_step()` 2π wrap logic |
| Speed doesn't reach setpoint | Low PI gain or load too high | Increase speed Kp or reduce T_load |
| Oscillations > ±5% | Integral windup or unstable gain | Reduce Ki or add derivative term |
| PWM always 0 or max | Duty cycle clamping | Check inverter voltage saturation |

---

**For detailed implementation notes, see:** [session_17-04-2026.md](session_17-04-2026.md)
