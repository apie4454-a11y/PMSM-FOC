# PMSM Field Oriented Control (FOC) — GM3506 / XMC4700

**Status:** ✅ **EKF Observer Validated** — Open-loop FOC running + RTT streaming + Motor model integrated + **EKF state estimation from currents alone (no encoder needed)**  
**Last Updated:** 10-08-2026

## What Is This?

A complete **Field Oriented Control (FOC) implementation** for a high-speed gimbal motor (GM3506) running on an ARM Cortex-M4 microcontroller (XMC4700). The project spans **simulation, firmware, embedded systems, and hardware validation** — from MATLAB Simulink models through real-time embedded C code to actual motor control on silicon.

---

## 🎯 Key Achievements

✅ **Triple-loop cascaded control system** — Speed → Torque → Current (2 kHz, 20 kHz, 20 kHz respectively)  
✅ **Physics-based motor parameters** — Derived J & B from iFligh datasheet; validated against motor datasheet constants  
✅ **Formula-driven PI gains** — No empirical tuning; all gains derived from control theory (bandwidth, inertia, resistance)  
✅ **Stability margins verified** — Phase Margin 77.7° (current loops), 95.2° (speed loop); Bode/Nyquist/Nichols analysis complete  
✅ **Coordinate transforms validated on hardware** — dq↔abc math proven correct via UART handshake with real motor  
✅ **Real encoder feedback** — XMC4700 CCU4 capture module integrated; rotor angle feedback working at 1 MHz  
✅ **PWM dead time** — Hardware SR FlipFlop reconstruction validated (97.2 ns dead time, < 100 ns spec)  
✅ **SIL-to-C port** — MATLAB algorithm extracted to embedded C; timing architecture verified  
✅ **Autonomous RTT streaming** — Real-time data logging without UART handshake dependency  
✅ **Generic CSV plotter tools** — MATLAB & Python plotters for cross-project reuse  
✅ **Extended Kalman Filter (EKF)** — Motor angle & speed estimation from current measurements alone (no encoder dependency, observability proven)  
✅ **TLE9879 embedded FOC discovered** — Power shield contains ARM M3 microcontroller with pre-programmed FOC firmware (SPI configurable)  
✅ **Systematic debugging** — 3 major root causes found via methodical analysis (not random guessing)

---

## 🛠️ Tech Stack

| Layer | Technology | Status |
|-------|-----------|--------|
| **Microcontroller** | XMC4700 (ARM Cortex-M4 @ 144 MHz) | ✅ Working |
| **Real-Time Control** | C (20 kHz ISR, 1 MHz motor model) | ✅ Embedded |
| **Motor Algorithm** | FOC (PI current loop, speed regulator) | ✅ Validated |
| **Simulation** | MATLAB/Simulink (MIL/SIL) | ✅ Reference |
| **Stability Analysis** | Bode/Nyquist/Nichols (MATLAB Control System Toolbox) | ✅ Verified |
| **PWM Modulation** | Sine-Triangle (20 kHz, 59V supply) | ✅ Implemented |
| **Debugging** | SEGGER RTT (real-time telemetry), UART (handshake) | ✅ Proven |
| **Motor** | iPower GM3506 (24N/22P, 141.4 RPM/V) | ✅ Controlled |

---

## 📋 Current Project State

| Component | Status | Notes |
|-----------|--------|-------|
| Motor parameter extraction | ✅ Complete | Physics-based (J, B) + datasheet (R, L, Kv) |
| FOC control architecture | ✅ Complete | Cascade design, PI gain formulas, saturation strategy |
| MATLAB/Simulink model | ✅ Complete | MIL validation, SIL extraction |
| XMC4700 firmware | ✅ Complete | FOC + motor model + PWM in C, 1 MHz loop |
| Encoder integration | ✅ Complete | CCU4 capture, real rotor feedback |
| Hardware validation | ✅ Complete | 3 bugs fixed, dq→abc math proven, UART confirmed |
| Closed-loop speed control | 🔄 In Progress | Motor tracking under load (Phase 3 refinement) |
| Current sensors | ⏳ Upcoming | 3-phase current measurement for true closed-loop |

---

## 🏆 Three Debugging Victories (Why This Matters)

Each victory demonstrates **systematic root-cause analysis** across abstraction levels — simulation → algorithm → hardware timing:

1. **Filter Phase Lag (Simulation)** — 150 Hz oscillation wasn't a tuning problem; filters were adding unwanted lag. Removed them; problem disappeared.
2. **Block Causality (Model)** — Motor feedback delayed one sample due to wrong block order. Reordered; artificial oscillation was purely a Simulink structure issue.
3. **Execution Rate Mismatch (Firmware)** — Speed PI running at 20 kHz instead of 2 kHz design assumption (10× integrator error). Added decimation; response improved 4–5× in settling time.

**Lesson:** Before blaming tuning, verify architecture. Each layer (simulation, algorithm, timing) must be correct first.

---

## 🚀 How to Get Started

**1. Understand the Design Philosophy**
- [CONTROL_DESIGN_DECISIONS.md](CONTROL_DESIGN_DECISIONS.md) — Why cascaded FOC? Why EKF? Why these gains?
- [MATHEMATICAL_FOUNDATION.md](MATHEMATICAL_FOUNDATION.md) — Motor equations, PI gain derivation, EKF system matrices

**2. See How It's Implemented**
- [IMPLEMENTATION_ARCHITECTURE.md](IMPLEMENTATION_ARCHITECTURE.md) — Simulink blocks → C code mapping, anti-windup, voltage saturation

**3. Review the Motor Design**
- [motor_parameters_derivation.md](motor_parameters_derivation.md) — Physics-based parameter calculation

**4. Understand the Control Architecture**
- [PROJECT_STATUS.md](PROJECT_STATUS.md) — Detailed specs, voltage budget, control strategy (living document)

**5. See the Full Implementation Journey**
- [session_19-04-2026.md](session_19-04-2026.md) — Latest hardware validation results
- [session_10-08-2026.md](session_10-08-2026.md) — EKF observer implementation & validation
- Session files in root — Day-by-day development log

**6. Verify Stability (Before Hardware)**
- [session_11_08_2026.md](session_11_08_2026.md) — Bode/Nyquist/Nichols plots, phase & gain margins for all 3 loops

**7. Build & Test**
- Firmware: `XMC4700/25_04_2026/` (embedded FOC + closed-loop control)
- Simulation: `simulation_basic_sil.slx` (MATLAB/Simulink SIL model)
- Python plotter: `plot_csv.py` — Visualize test results

---

## 📊 What's Working Now

- Motor speed control (open-loop and closed-loop tested)
- Sine-triangle PWM modulation @ 20 kHz
- Real-time rotor angle feedback (encoder)
- UART telemetry and command interface
- Load transient response (PI controller settling < 100 ms)

---

## 📚 Detailed Documentation

For deeper dives, see [PROJECT_STATUS.md](PROJECT_STATUS.md) which contains:
- Complete motor specs and voltage budget analysis
- Full control strategy documentation
- Phase-by-phase milestone tracking
- Working file inventory
- Known issues and architecture decisions


