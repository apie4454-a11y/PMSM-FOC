# PMSM Field Oriented Control (FOC) — GM3506 / XMC4700

**Project Status:** Motor parameter derivation + model architecture corrected + physics-based gains calculated + code refactoring complete ✅  
**Current Session:** [session_09-04-2026.md](session_09-04-2026.md) (Code refactoring + Ki gain analysis + physics-based motor parameters)  
**Last Updated:** 09-04-2026 ✅ **FIRMWARE READY**

---

## Quick Summary

**Goal:** Design and implement triple-loop cascaded FOC for iPower GM3506 gimbal motor on XMC4700 microcontroller.

**Motor Specs (Locked):**
| Parameter | Value | Notes |
|-----------|-------|-------|
| Configuration | 24N/22P (P=11) | Gimbal-class high-Kv |
| Speed Constant | 141.4 RPM/V | @ 16V no-load |
| Flux Linkage | 0.00611 Wb | Derived from Kv |
| Phase Resistance | 5.6Ω | d-q equivalent: 8.4Ω (2/3 transform) |
| Phase Inductance | 2mH | d-q equivalent: 3mH |
| Rated Current | 1.0 A | Thermal limit |
| Operating Point | 2262 RPM @ 1A | MTPA control (id=0, iq=1A) |

**Voltage Budget (Locked):**
| Requirement | Calculation | Result |
|------------|-----------|--------|
| Operating Voltage | Vq=24.33V, Vd=-7.82V | Vmag = 25.54V |
| SVPWM (alt) | 25.54 × √3 × 1.15 | Vdc = 51V (15% higher efficiency) |
| Sine-Triangle (chosen) | 25.54 × 2 × 1.15 | **Vdc = 59V** ✅ |
| Controller Limit (Sine-Triangle) | Vdc/2 | **Vmax = 29.5V** |
| Voltage Circle | √(Vd² + Vq²) ≤ Vmax | ✅ Satisfied with 3.96V margin |

**Control Strategy (FINAL - Day 3 Model Validated, Day 9 Physics-Based Gains, Day 10 Dead Time Validated):**
- **Cascade:** Speed (2 kHz, 200 Hz BW) → Torque → Current (20 kHz, 2000 Hz BW)
- **Motor Parameters:** J=3.8e-6 kg·m², B=4.5e-5 N·m·s/rad (calculated from iFligh datasheet, [motor_parameters_derivation.md](motor_parameters_derivation.md))
- **Speed PI Gains:** Kp_speed = J×2π×200, Ki_speed = **B×2π×200 = 0.0565** (physics-based, no empirical multipliers)
- **Current PI Gains:** Kp_id=37.7, Ki_id=105,558 | Kp_iq=37.7, Ki_iq=105,558 (formula-based)
- **PWM Dead Time:** SR FlipFlop reconstruction (rising edge delayed, falling edge original), Tdead ≤ 100 ns (real hardware)
- **Anti-windup:** Mandatory (back-calculation or clamping)
- **Saturation:** Voltage circle enforcement before PWM modulation
- **Model:** Corrected causality (motor execution timing) + removed spurious output filters + dead time insertion validated
- **Variable Naming:** Explicit hierarchy: `current.Kp_id/Ki_id/Kp_iq/Ki_iq` and `speed.Kp_speed/Ki_speed`

---

## Session Files (Reference Archive)

| Session | Focus | Status |
|---------|-------|--------|
| [session_31-03-2026.md](session_31-03-2026.md) | Project foundation: FOC architecture, motor specs, voltage budget, control design | Reference |
| [session_01-04-2026.md](session_01-04-2026.md) | Simulation validation (flawed model): FOC, PI gains, MTPA | Superseded |
| [session_02-04-2026.md](session_02-04-2026.md) | Speed controller debugging: 150 Hz oscillation, PI gain issue | Pending re-validation |
| [session_09-04-2026.md](session_09-04-2026.md) | Code refactoring: variable naming, current/speed loop gains | Complete |
| [session_10-04-2026.md](session_10-04-2026.md) | Dead time insertion (PWM safety), performance validation | Validated |
| [session_11-04-2026.md](session_11-04-2026.md) | 3D flange design: custom 3D-printable shaft coupling | Complete |
| [XMC4700/12_04_2026/12_04_2026_Session_CCU4_Capture_Breakthrough.md](XMC4700/12_04_2026/12_04_2026_Session_CCU4_Capture_Breakthrough.md) | Encoder capture breakthrough: CCU4 config, SEGGER RTT debug | Awaiting HW test |
| [XMC4700/13_04_2026/13_04_2026_Session_MATLAB_XMC_Integration_Planning.md](XMC4700/13_04_2026/13_04_2026_Session_MATLAB_XMC_Integration_Planning.md) | MATLAB-XMC integration planning: FOC via UART, architecture | Planning |
| [XMC4700/14_04_2026/14_04_26_HIL_to_RapidPrototyping.md](XMC4700/14_04_2026/14_04_26_HIL_to_RapidPrototyping.md) | HIL to rapid prototyping: abandoned UART, XMC-only control | Complete |
| [RIPPLE_MITIGATION_01-04-2026.md](RIPPLE_MITIGATION_01-04-2026.md) | Ripple mitigation: investigation history, 6 attempted solutions | Reference |
| [motor_parameters_derivation.md](motor_parameters_derivation.md) | Motor parameters: physics-based J & B calculation | Reference |


## Key Decisions & Trade-offs

**1. Why Sine-Triangle instead of SVPWM?**
- Sine-Triangle simpler to implement on XMC4700
- 59V supply adequate for the application
- ✅ Selected for simplicity over cost savings

**2. Why Vdc = 59V (not 44.3V minimum)?**
- 44.3V is worst-case steady-state requirement
- Added 33% transient headroom → 59V (Sine-Triangle modulation)
- Accommodates load steps, PI integrator dynamics, measurement noise

**3. Why MTPA (id=0, iq=1A)?**
- Non-salient motor: id produces zero reluctance torque
- All 1A current should go to iq (torque production)
- Maximizes torque density below flux-weakening threshold

**4. Why 2/3 amplitude-invariant transform?**
- Preserves voltage magnitude: √(Vd²+Vq²) = Vpeak ✓
- Requires R_dq = 8.4Ω (not 5.6Ω phase resistance)
- Mandatory for correct PI gain derivation

---

## Milestones & Next Steps

**✅ Completed:**
- Motor parameter extraction from datasheet ✓
- Physics-based motor parameter calculation (J, B) ✓
- Code refactoring: Variable nomenclature standardization ✓
- Voltage budget analysis (Vdc=59V, Sine-Triangle) ✓
- PWM modulation comparison & selection ✓
- Control architecture design ✓
- Formula-based PI gain calculation ✓
- Phase 1.1: Speed reference tracking with corrected J/B ✓
- Phase 1.2: Dead time insertion validation ✓

**🔄 In Progress:**
- Phase 1.3-1.5: Full TEST_VALIDATION_PLAN execution (remaining tests)
- Graph capture for validation report (10-14 graphs from simulation)

**⏳ Upcoming:**
- **Phase 2:** Hardware firmware implementation on XMC4700 (gains validated, dead time architecture ready)
- Phase 3: Bearing block integration + 3-phase hardware test
- Phase 4: Production firmware deployment
- Future: Flux-weakening expansion

---

## Working Files

| File | Purpose |
|------|---------|
| **Motor_Parameters.m** | ✅ Refactored: current.Kp_id/Ki_id/Kp_iq/Ki_iq + speed.Kp_speed/Ki_speed (physics-based J/B) |
| **motor_parameters_derivation.md** | Physics-based J & B calculation from iFligh datasheet (hollow cylinder + power analysis) |
| **session_10-04-2026.md** | ✅ **Latest:** Dead time insertion, SR FlipFlop validation, performance tested |
| **session_09-04-2026.md** | Code refactoring, physics-based motor parameters, firmware ready |
| **session_31-03-2026.md** | Foundation: Motor specs, voltage budget, control architecture (reference) |
| **session_02-04-2026.md** | Day 3 investigation (⚠️ findings pending re-validation with corrected J/B) |
| **RIPPLE_MITIGATION_01-04-2026.md** | Complete investigation history (reference archive) |

---

## Quick Reference: Key Formulas

**Voltage Budget:**
$$V_{dc} = V_{required} \times 2 \times 1.15 \quad \text{(Sine-Triangle)}$$

**Electrical Frequency:**
$$\omega_e = \frac{n_{RPM} \times P \times \pi}{30}$$

**PI Gains (Current Loop, 2000Hz):**
$$K_p = L_{dq} \times 2\pi \times 2000 = 37.7$$
$$K_i = R_{dq} \times 2\pi \times 2000 = 105,595$$

**Voltage Circle Saturation:**
$$\sqrt{V_d^2 + V_q^2} \leq \frac{V_{dc}}{2} = 29.5V \quad \text{(Sine-Triangle)}$$

---

## How to Use This Repository

- **Firmware implementation ready?** Start here: [session_09-04-2026.md](session_09-04-2026.md) (physics-based gains, code refactored)
- **Want motor parameter derivation?** See [motor_parameters_derivation.md](motor_parameters_derivation.md) (hollow cylinder formula + power analysis)
- **Need architecture foundation?** Read [session_31-03-2026.md](session_31-03-2026.md) (motor specs, voltage budget, control design)
- **Full investigation history?** See [RIPPLE_MITIGATION_01-04-2026.md](RIPPLE_MITIGATION_01-04-2026.md) (all 6 attempted solutions)
- **Ready for validation?** Execute [TEST_VALIDATION_PLAN.md](TEST_VALIDATION_PLAN.md) (16 tests)

---

## Important: Day 3 Findings Pending Re-Validation ⚠️

Day 3 identified a 150 Hz oscillation and proposed Kp=0.0065 (76% de-rating) as the solution. **However**, this solution was developed with incorrect motor parameters (J was 5.8× too high, B was 45× too low).

**Status:** The Kp=0.0065 solution **needs re-validation with corrected physics-based J/B parameters**. The 150 Hz oscillation itself may not exist with accurate motor parameters, or may require very different tuning.

**Next Step:** Run Phase 1.1 validation test using current-loop control with correct J/B before implementing hardware. The true root cause may differ from the Day 3 diagnosis.

---

## Known Issues & Hardware Notes

**Motor (GM3506):**
- Has hollow shaft (unsuitable for direct load bearing) → requires bearing block + coupling
- Gimbal-class high-Kv motor (141.4 RPM/V)
- Rated for 1A continuous, 2262 RPM @ MTPA

**Simulation → Hardware Path:**
- Phase 1: Simulation validation (with corrected J/B parameters) — IN PROGRESS
- Phase 2: XMC4700 firmware implementation (gains TBD post-Phase 1.1 validation)
- Phase 3: Bearing block integration + 3-phase hardware test
- Phase 4: Production firmware deployment

**Architecture & Parameters Final Status:**
✅ Motor constants — LOCKED  
✅ Voltage budget — LOCKED  
✅ Transform convention — LOCKED (2/3 amplitude-invariant)  
✅ Control strategy — LOCKED (MTPA, cascade)  
✅ PI gains — LOCKED (verified with correct L_dq)  
✅ PWM modulation — SELECTED (Sine-Triangle @ 59V)  
✅ Saturation limits — DERIVED (Vmax=29.5V)
