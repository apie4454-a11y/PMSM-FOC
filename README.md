# PMSM Field Oriented Control (FOC) — GM3506 / XMC4700

**Project Status:** Motor parameter derivation + simulation validation complete ✅  
**Current Session:** [session_01-04-2026.md](session_01-04-2026.md) (Day 2: Simulation validation, 16-test plan)  
**Last Updated:** 01-04-2026

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
| SVPWM (chosen) | 25.54 × √3 × 1.15 | **Vdc = 51V** ✅ |
| Sine-Triangle (alt) | 25.54 × 2 × 1.15 | Vdc = 59V (inefficient) |
| Controller Limit (SVPWM) | Vdc/√3 | **Vmax = 29.4V** |
| Voltage Circle | √(Vd² + Vq²) ≤ Vmax | ✅ Satisfied with 3.9V margin |

**Control Strategy:**
- **Cascade:** Speed (200Hz) → Torque → Current (2000Hz)
- **PI Gains:** Kp=37.7 (current), Ki=105,558 (current)
- **Anti-windup:** Mandatory (back-calculation or clamping)
- **Saturation:** Voltage circle enforcement before PWM modulation

---

## Session Files (Date-Based Archive)

All technical details organized by session date:

👉 **[session_31-03-2026.md](session_31-03-2026.md)** — **START HERE**
- Motor parameters + datasheet analysis
- Voltage budget derivation (step-by-step)
- PWM modulation techniques (SVPWM vs. Sine-Triangle)
- Controller saturation limits
- Cascade control architecture & PI tuning
- 10 Lessons Learned (Lessons 1-10)
- Resume bullet points (15 items)
- Simulation setup guide

---

## Key Decisions & Trade-offs

**1. Why SVPWM instead of Sine-Triangle PWM?**
- SVPWM achieves 15% higher voltage utilization
- 51V SVPWM = 59V Sine-Triangle (same performance, 8V less cost)
- ✅ Selected for cost efficiency

**2. Why Vdc = 51V (not 44.3V minimum)?**
- 44.3V is worst-case steady-state requirement
- Added 15% transient headroom → 51V
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
- Motor parameter extraction from datasheet
- Voltage budget analysis (Vdc=51V)
- PWM modulation comparison
- Control architecture design
- PI gain calculation
- Hardware constraint identification (gimbal shaft unsuitable for direct load)

**🔄 In Progress:**
- Simulation validation (MATLAB/Simulink) — see [.session-todo.md](.session-todo.md)
- User paper derivations on motor dynamics equations

**⏳ Upcoming:**
- Compare SVPWM vs. Sine-Triangle simulation results
- Firmware implementation (XMC4700 C-code)
- Hardware quick-test (encoder validation)
- Bearing block integration (3-phase deployment)
- Flux-weakening expansion (future)

---

## Working Files

| File | Purpose |
|------|---------|
| **session_01-04-2026.md** | Day 2: Simulation validation, 16-test plan (CURRENT) |
| **session_31-03-2026.md** | Day 1: Motor derivation, voltage budget, 10 Lessons Learned |
| **TEST_VALIDATION_PLAN.md** | Comprehensive 16-scenario test suite with graph requirements |
| **.session-todo.md** | Simulation validation todo (3 PWM scenarios) |
| **/memories/repo/motor_constants.md** | Locked motor electrical specs reference |

---

## Quick Reference: Key Formulas

**Voltage Budget:**
$$V_{dc} = V_{required} \times \sqrt{3} \times 1.15 \quad \text{(SVPWM)}$$

**Electrical Frequency:**
$$\omega_e = \frac{n_{RPM} \times P \times \pi}{30}$$

**PI Gains (Current Loop, 2000Hz):**
$$K_p = L_{dq} \times 2\pi \times 2000 = 37.7$$
$$K_i = R_{dq} \times 2\pi \times 2000 = 105,595$$

**Voltage Circle Saturation:**
$$\sqrt{V_d^2 + V_q^2} \leq \frac{V_{dc}}{\sqrt{3}}$$

---

## How to Use This Repository

1. **First time?** → Read [session_31-03-2026.md](session_31-03-2026.md) (comprehensive)
2. **Quick reference?** → Check this README (summary + links)
3. **Simulation setup?** → See section "Simulation Setup & Instructions" in session file
4. **Firmware questions?** → Search session file for "Firmware", "C-code", or "XMC4700"
5. **Next steps?** → Check [.session-todo.md](.session-todo.md) for pending tasks

---

## Contact / Notes

**Known Issues:**
- GM3506 gimbal motor has hollow shaft (unsuitable for direct load bearing) → requires bearing block + coupling

**Hardware Path:**
- Phase 1: Simulation validation (this week)
- Phase 2: Quick-test with <30g rod (week 2)
- Phase 3: Bearing block integration (week 3)

**Architecture & Parameters Final Status:**
✅ Motor constants — LOCKED  
✅ Voltage budget — LOCKED  
✅ Transform convention — LOCKED (2/3 amplitude-invariant)  
✅ Control strategy — LOCKED (MTPA, cascade)  
✅ PI gains — LOCKED (verified with correct L_dq)  
✅ PWM modulation — SELECTED (SVPWM @ 51V)  
✅ Saturation limits — DERIVED (Vmax=29.4V)
