# Control Design Decisions — PMSM FOC for GM3506/XMC4700

**Purpose:** Document the major architectural and algorithmic choices behind this FOC implementation. These decisions represent trade-offs between performance, complexity, and implementation constraints.

---

## 1. Why Cascaded Control (Not Flat Gain)?

**Decision:** Implement three nested loops: Speed (2 kHz) → Torque → Current (20 kHz), rather than a single flat transfer function.

**Reasoning:**

**Frequency Separation Principle:**
- Speed loop operates at 200 Hz bandwidth (mechanical time scale: motor inertia, load friction)
- Current loop operates at 2000 Hz bandwidth (electrical time scale: L/R response)
- 10× separation ensures inner loop responds before outer loop changes reference
- Each loop sees the inner loop as a "fast actuator" → inner loop can be designed independently

**Physical Motivation:**
- Motor mechanical response is slow (J = 3.8e-6 kg·m², dominated by inertia)
- Motor electrical response is fast (L = 3 mH, R = 8.4 Ω, time constant τ = L/R ≈ 0.36 ms)
- Cascading exploits this natural separation

**Implementation Benefit:**
- Modular: tune current loop first (fast, steady), then speed loop (slow, reference tracking)
- Robust: inner loop rejection of voltage disturbances before speed controller sees them
- Decoupled: speed tuning doesn't require re-tuning current gains

**Source:** [session_31-03-2026.md](session_31-03-2026.md) (architecture foundation)

---

## 2. Why 10× Frequency Separation?

**Decision:** Design speed PI (200 Hz) with current PI (2000 Hz) → exactly 10:1 bandwidth ratio.

**Reasoning:**

**Control Theory Foundation:**
- Inner loop settles in ~5 cycles at 2000 Hz = 2.5 ms response time
- Outer loop sees quasi-steady response; can apply linear PI
- Rule of thumb: 5-10× separation for stable cascade (we chose 10 for margin)

**Gain Formulas Driven by This Ratio:**

Speed loop (bandwidth 200 Hz):
$$K_{p,speed} = J \times 2\pi \times 200 = 3.8 \times 10^{-6} \times 2\pi \times 200 = 0.00477$$

Current loop (bandwidth 2000 Hz):
$$K_p = L_{dq} \times 2\pi \times 2000 = 0.003 \times 2\pi \times 2000 = 37.7$$

The 10:1 ratio emerges naturally from requiring both loops to settle cleanly without oscillation.

**Stability Implication:**
- If ratio < 5: inner loop too slow, outer loop can overshoot
- If ratio > 20: inner loop too fast, actuator saturation (PWM can't react fast enough)
- 10:1 is the "sweet spot" for this motor + XMC4700

**Source:** [Motor_Parameters.m](Motor_Parameters.m) (gain calculation), [session_09-04-2026.md](session_09-04-2026.md) (refactored gain naming)

---

## 3. Why Pole-Zero Cancellation (Physics-Based Gains)?

**Decision:** Derive all PI gains from first principles (bandwidth + motor parameters), NOT empirical tuning.

**Reasoning:**

**Avoid "Tuning Roulette":**
- Empirical tuning (try gains, measure oscillation, adjust) has no guarantees
- Led to the 150 Hz oscillation problem in early sessions (Session 02 → 09)
- Filters were added to hide the problem; root cause was unsystematic design

**Physics-Based Approach:**
Each PI controller is designed to cancel the motor's natural pole and place a new pole at desired bandwidth.

**Current Loop (Simplified RL Circuit):**
Motor electrical dynamics: $\frac{di}{dt} = \frac{1}{L}(v - Ri - e_{back-emf})$

Natural pole: $p = -\frac{R}{L} = -2800$ rad/s

Desired pole: $p_{new} = -2\pi \times 2000 = -12,566$ rad/s

PI controller cancels R/L, places new pole:
- $K_p = L \times 2\pi \times 2000 = 37.7$
- $K_i = R \times 2\pi \times 2000 = 105,595$

**Speed Loop (Mechanical Dynamics):**
Motor mechanical: $\frac{d\omega}{dt} = \frac{1}{J}(T_e - B\omega)$

Natural pole: $p = -\frac{B}{J} = -11.8$ rad/s (very slow!)

Desired pole: $p_{new} = -2\pi \times 200 = -1257$ rad/s

PI gains:
- $K_p = J \times 2\pi \times 200 = 0.00477$
- $K_i = B \times 2\pi \times 200 = 0.0565$

**No Empirical Multipliers:**
Unlike typical tuning ("Kp = 0.5 × formula value"), we use formula values directly because derivation includes all physics.

**Validation:**
Load transients in SIL show <100 ms settling with no overshoot — proof that pole-zero cancellation is optimal.

**Source:** [PROJECT_STATUS.md](PROJECT_STATUS.md) (PI gain formulas section), [Motor_Parameters.m](Motor_Parameters.m), [motor_parameters_derivation.md](motor_parameters_derivation.md)

---

## 4. Why Extended Kalman Filter (EKF) for State Estimation?

**Decision:** Implement observer-based state estimation (angle + speed from currents alone) rather than relying on encoder feedback.

**Reasoning:**

**Eliminates Sensor Dependency:**
- Encoder adds hardware complexity (CCU4 capture, wiring, noise)
- Current sensors (shunt resistors) are already present for control
- EKF fuses voltage inputs + current measurements → estimates θ, ω without separate angle sensor

**Observability Principle:**
Back-EMF voltage depends on ω and θ:
$$v = Ri + L\frac{di}{dt} + \lambda_m \omega \sin(\theta) \quad \text{(d-axis)}$$

The sin(θ) and ω terms in current dynamics make state (θ, ω) observable from voltage and current alone.

**Frame Choice: α-β (Stationary) Not d-q (Rotating):**
- **Problem with d-q frame:** Would need to transform measured currents back to physical reality using θ → circular dependency (need θ to estimate θ!)
- **Solution: α-β frame:** Currents measured directly in fixed frame; back-EMF terms sin(θ), cos(θ) appear naturally
- Result: Clean observability without circular dependencies

**Robustness to Load Transients:**
SIL validation (session_10-08-2026) shows EKF handles sudden load drops (0.026 → 0.02 Nm at t=1.3s) without divergence. Encoder feedback would require separate filtering for same robustness.

**Future Fault Tolerance:**
If encoder fails, EKF estimates remain valid. Enables sensorless mode fallback.

**Source:** [session_10-08-2026.md](session_10-08-2026.md) (complete EKF derivation, validation, observability proof)

---

## 5. Why Sine-Triangle PWM (Not SVPWM)?

**Decision:** Use Sine-Triangle modulation @ 20 kHz instead of Space Vector PWM.

**Reasoning:**

**Implementation Simplicity:**
- Sine-Triangle: three sine wave references compared to triangular carrier → simple logic
- SVPWM: requires coordinate rotation, sector switching, switching time calculation → complex state machine
- XMC4700 constraints: No dedicated PWM block; manual generation → simpler is better

**Sufficient Voltage Margin:**
- SVPWM efficiency: 44.3V theoretical minimum (100% DC bus utilization)
- Sine-Triangle requirement: 25.54V × 2 × 1.15 = **59V** (115% headroom)
- Trade-off: 59V power supply costs ~$3 more than 44.3V; huge gain in robustness
- Accommodates load steps, PI integrator wind-up, measurement noise without saturating PWM

**Real-Time Feasibility:**
- 20 kHz ISR @ 144 MHz: ~7200 cycles available
- Sine-Triangle: ~200 cycles
- SVPWM: ~500 cycles
- Safety margin preserved for other ISR tasks (encoder read, telemetry)

**Source:** [PROJECT_STATUS.md](PROJECT_STATUS.md) (voltage budget trade-offs section), [session_15-04-2026.md](session_15-04-2026.md) (PWM implementation)

---

## 6. Why 100 RPM EKF Trigger (Not 0 RPM)?

**Decision:** Activate EKF only when motor speed |rpm| ≥ 100 RPM; below this, estimates are unreliable.

**Reasoning:**

**Observability Threshold:**
EKF observability depends on back-EMF strength:
$$\text{Back-EMF} = \lambda_m \times \omega = 0.00611 \times \omega$$

At low speeds (< 100 RPM ≈ 10.5 rad/s):
$$\text{Back-EMF} < 0.00611 \times 10.5 = 0.064 \text{ V}$$

Typical current sensor noise: ~±0.05 A on 0.1Ω shunt = ±5 mV noise floor

**Problem:** Back-EMF signal < noise floor → filter can't distinguish state changes from measurement noise → covariance grows → estimates diverge

**Solution:** Wait for motor to reach operating point (100 RPM) where back-EMF >> noise, then activate EKF

**Practical Benefit:**
- Avoids filter startup transients at zero speed
- Simplifies initialization (no special singularity handling)
- In practice, motor reaches 100 RPM in ~0.6 sec under light load (acceptable delay)

**SIL Validation:**
Test scenario (session_10-08-2026) shows clean convergence after t=0.66s (100 RPM). Before that, estimates would be unreliable anyway.

**Source:** [session_10-08-2026.md](session_10-08-2026.md) (trigger condition explanation, 100 RPM justification)

---

## Summary: Design Philosophy

These six decisions reflect a **systematic approach** rather than ad-hoc tuning:

1. **Cascaded control** exploits natural frequency separation (mechanics vs. electronics)
2. **10× ratio** ensures stable nested loops without oscillation
3. **Physics-based gains** (pole-zero cancellation) guarantee performance without empirical roulette
4. **EKF observer** eliminates sensor dependencies while proving observability from first principles
5. **Sine-Triangle PWM** balances implementation simplicity with adequate voltage margin
6. **100 RPM trigger** respects physical observability limits

Each decision is justified by explaining the underlying constraint, trade-off, or control theory principle.

---

**Last Updated:** 10-08-2026
