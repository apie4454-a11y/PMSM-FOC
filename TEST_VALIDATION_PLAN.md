# FOC Motor Control Validation Test Plan
**Project:** iPower GM3506 Gimbal Motor with Field-Oriented Control (FOC)  
**Date Created:** April 1, 2026  
**Status:** In Progress  
**Test Environment:** MATLAB/Simulink + XMC4700 Hardware  

---

## Executive Summary

This document defines comprehensive validation tests to verify:
1. Simulation model accuracy (white-box physics)
2. PI controller gains correctness (Kp=37.7, Ki=105,558)
3. PWM modulation technique selection (SVPWM vs. Sine-Triangle efficiency)
4. Cascade control stability under load transients
5. Hardware-simulation correlation

---

## Test Phase 1: Nominal Operation

### Test 1.1: Speed Reference Tracking (No Load)
**Objective:** Validate speed controller follows reference without overshoot

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Reference | 500 → 1500 → 2262 | RPM |
| Load Torque | 0 | Nm |
| Vdc | 51 | V (SVPWM) |
| PWM Technique | SVPWM | - |

**Expected Results:**
- Speed settles within 100 ms of step command
- Overshoot: < 5%
- Steady-state error: < 1% (1 RPM @ 100 RPM ref)
- id = 0 A (MTPA maintained)
- iq traces smoothly with ±0.2A ripple

**Pass Criteria:**
- [ ] Speed ramp smooth without oscillation
- [ ] id stays within ±0.05A (noise margin from zero)
- [ ] iq matches Te_required calculated from speed error
- [ ] No integrator windup (PI output doesn't saturate)

**Graphs to Capture & Save:**
1. **rpm_actual vs. rpm_reference** (full transient)
2. **iq_reference vs. iq_actual** (current tracking)
3. **id** (should stay at 0 ±0.05A)
4. **Vd and Vq** (voltage demands)
5. **Error signal: rpm_error = rpm_ref - rpm_actual** (convergence rate)

**Results:**
- Simulation Date: __________
- Speed Rise Time (10%-90%): __________
- Overshoot %: __________
- Final Steady-State Error: __________
- Graphs saved as: Test_1_1_speed_tracking_[date].png, Test_1_1_currents_[date].png
- Notes: _____________________________________________________________

---

### Test 1.2: Current Ripple Characterization
**Objective:** Quantify switching ripple vs. control oscillation

**Test Setup:**
- Speed: 2262 RPM (steady state)
- Load: 0 Nm
- Sampling: 100 kHz (10× PWM frequency)
- Duration: 0.5 s

**Expected Results:**
- Ripple frequency: 20 kHz (matches PWM)
- iq ripple amplitude: ±0.3A (within 30% of 1A nominal)
- Ripple does NOT grow over time (stable)

**Pass Criteria:**
- [ ] FFT peak at exactly 20 kHz
- [ ] Ripple amplitude constant (±3% over duration)
- [ ] No subsynchronous oscillations (<5 kHz)
- [ ] No 50/100 Hz harmonics (AC line noise)

**Graphs to Capture & Save:**
1. **iq zoomed in (0.3-0.35 s window, show 3-4 PWM cycles)**
2. **FFT(iq) from DC to 100 kHz** (show 20 kHz peak)
3. **id zoomed in (should show <±0.1A ripple)**
4. **Three-phase currents Ia, Ib, Ic zoomed** (120° phase spacing)
5. **Voltage magnitude √(Vd² + Vq²) over time**

**Results:**
- Simulation Date: __________
- Peak Ripple Frequency: __________ Hz
- Ripple Amplitude: ±__________ A
- FFT Peak SNR: __________
- Graphs saved as: Test_1_2_ripple_zoomed_[date].png, Test_1_2_fft_[date].png
- Notes: _____________________________________________________________

---

### Test 1.3: Oscillation Validation @ Kp=0.0065 (Root Cause Verification)
**Objective:** Validate that Kp reduction to 0.0065 eliminates 150 Hz oscillation discovered in Day 3 investigation

**Background:** Days 1-3 systematic testing revealed discrete PI controller instability at 2 kHz sampling with theoretical Kp=0.0267. Root cause: 440× motor-to-sample-rate mismatch creates phase lag, destabilizing high gains. Solution: Reduce Kp to 0.0065 (76% de-rating).

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Reference | 2262 | RPM |
| Load Profile | 0 Nm constant (no-load test) | Nm |
| Sampling Rate (Speed Loop) | 2 | kHz |
| Kp Value (Tuned) | 0.0065 | - |
| Expected Oscillation Frequency (BEFORE fix) | 150 | Hz |
| Expected Oscillation Frequency (AFTER fix) | None | - |

**Expected Results:**
- Zero sustained 150 Hz oscillation in iq and speed
- Ripple amplitude < 0.3 A (switching ripple only, no control oscillation)
- FFT shows NO peak at 150 Hz (clean baseline)
- Smooth, damped response to speed commands
- Settling time: ~150 ms (as documented in Day 3)

**Pass Criteria:**
- [ ] Peak-to-peak iq ripple: < 0.5 A (no 150 Hz oscillation visible)
- [ ] FFT(iq): NO discrete peak in 100-200 Hz band
- [ ] FFT(speed): NO discrete peak in 50-150 Hz band
- [ ] Oscillation elimination confirmed vs. Day 3 problem case
- [ ] Settling time < 200 ms (typical FOC performance)

**Graphs to Capture & Save:**
1. **iq time-domain plot (0.5-1.5 s window, show smooth settling)**
2. **FFT(iq) from DC to 300 Hz** (show absence of 150 Hz peak)
3. **Speed error over time (should converge smoothly, no oscillation)**
4. **Overlaid comparison: Kp=0.0267 (oscillatory) vs. Kp=0.0065 (stable)** (side-by-side)
5. **iq zoomed during transient (2-3 PWM cycles, show clean waveform)**
6. **Speed ripple FFT (should show only 20 kHz switching, no 150 Hz)

**Results:**
- Simulation Date: __________
- Peak Ripple (iq): __________ A
- FFT Peak Search (100-200 Hz): __________ (should be "No peak" or "<0.1A SNR")
- Oscillation Status: ELIMINATED ✓ / PERSISTS ✗
- Settling Time: __________ ms
- Comparison to Problem Case: __________ % improvement
- Graphs saved as: Test_1_3_oscillation_validation_[date].png, Test_1_3_fft_comparison_[date].png
- Notes: _____________________________________________________________

---

## Test Phase 2: Load Transient Response

### Test 2.1: Gradual Load Ramp (0 → 0.0671 Nm)
**Objective:** Validate motor can sustain maximum rated torque without current overshoot

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Reference | 2262 | RPM |
| Load Profile | 0 → 0.0671 Nm (ramp, 2 s) | Nm |
| Vdc | 51 | V |
| Motor Capability | τ = 1.5 × pp × λm × iq = 0.0671 Nm @ 1A | Nm |

**Expected Results:**
- Speed droop: < 5% (2262 RPM → >2149 RPM)
- iq ramps 0 → 1.0 A (maintains torque balance)
- id remains at 0 A (MTPA holds)
- Voltage circle respected: √(Vd² + Vq²) ≤ 29.4V

**Pass Criteria:**
- [ ] Speed never drops below 2100 RPM
- [ ] iq settles to 1.0 A ±0.05 A
- [ ] No overshoot in iq (no current spikes)
- [ ] Voltage circle constraint satisfied

**Graphs to Capture & Save:**
1. **rpm_actual vs. load_torque_applied** (on dual y-axis)
2. **iq_actual vs. Te_required** (torque production tracking)
3. **id (should stay ~0A)**
4. **Vd and Vq envelope** (respect voltage circle?)
5. **Mechanical power = τ × ω** (should increase with load)
6. **Speed error = rpm_ref - rpm_actual**

**Results:**
- Simulation Date: __________
- Max Speed Droop %: __________
- Final iq Steady-State: __________ A
- Peak Vd: __________ V, Peak Vq: __________ V
- Voltage Circle Check: √(Vd² + Vq²) = __________ V ✓/<29.4V
- Graphs saved as: Test_2_1_load_ramp_[date].png, Test_2_1_voltages_[date].png
- Notes: _____________________________________________________________

---

### Test 2.2: Sudden Load Step (0 → 0.0934 Nm)
**Objective:** Validate graceful degradation when load exceeds motor capability

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Reference | 2262 | RPM |
| Load Profile | Step: 0 @ t=0s → 0.0934 Nm @ t=2s | Nm |
| Motor Max Torque (at 1A) | 0.0671 | Nm |
| Expected Speed Floor | ~1850 | RPM (est. equilibrium) |

**Expected Results:**
- Speed drops sharply when load applied (50-100 RPM/s deceleration)
- Speed stabilizes at lower RPM where τ_motor = τ_load
- iq saturates at 1.0 A (current limit holds)
- PI integrator does NOT runaway (anti-windup working)
- No oscillation or chatter

**Pass Criteria:**
- [ ] Speed deceleration rate: 50-150 RPM/s
- [ ] Settling time to new equilibrium: < 500 ms
- [ ] Final iq = 1.0 A ±0.05 A with no overshoot
- [ ] No velocity oscillation (damped single transient)
- [ ] Motor current never exceeds 1.1 A (10% headroom)

**Graphs to Capture & Save:**
1. **rpm_actual around load step (1.5-2.5 s zoomed)**
2. **iq_actual when load applied (shows saturation at 1A)**
3. **rpm_error (0 before step, then negative, settles to new value)**
4. **Acceleration = dω/dt (shows sharp deceleration)**
5. **PI controller integrator output** (should NOT runaway)
6. **Load torque step function**

**Results:**
- Simulation Date: __________
- Speed @ Load Application: __________
- Min Speed Reached: __________ RPM
- Deceleration Rate: __________ RPM/s
- Final Equilibrium Speed: __________ RPM
- Time to Settle: __________ ms
- Peak Current: __________ A
- Graphs saved as: Test_2_2_load_step_[date].png, Test_2_2_error_recovery_[date].png
- Notes: _____________________________________________________________

---

### Test 2.5: PI Gain Margin Analysis (Kp Stability Boundary)
**Objective:** Determine how close Kp=0.0065 is to instability boundary; validate safety factor for manufacturing/temperature tolerances

**Rationale:** Gain margin (distance to instability) is critical for robustness. Too close to boundary = sensitive to component variations. Industry target: 20-45° phase margin (equivalent to 2-3x gain safety factor).

| Parameter | Value | Unit |
|-----------|-------|------|
| Nominal Kp | 0.0065 | - |
| Test Range | 0.0050 to 0.0100 | Kp sweep |
| Speed Reference | 2262 | RPM |
| Load | 0 Nm | Nm |
| Sampling Rate | 2 | kHz |

**Expected Results:**
- Kp=0.0065: **STABLE** (nominal working point)
- Kp=0.0080: **BORDERLINE/UNSTABLE** (visible ringing or oscillation)
- Kp=0.0050: **OVERDAMPED** (slow response, safe)
- Gain margin: ~20% (means Kp can increase 20% before instability)

**Pass Criteria:**
- [ ] Kp=0.0065 completely stable (no growing oscillation)
- [ ] Gain margin ≥ 15% (Kp can reach ≥0.0075 before instability)
- [ ] Phase response smooth, not resonant
- [ ] Settling time degrades gracefully as Kp reduced below 0.0065

**Test Procedure:**
1. Run 5 simulations: Kp = [0.0050, 0.0060, 0.0065, 0.0075, 0.0090]
2. Apply speed step 0→2262 RPM @ t=0.1s
3. Log: speed response, iq, error signal for 1 second
4. Analyze: settling time, overshoot, oscillation onset

**Graphs to Capture & Save:**
1. **Speed responses (all 5 Kp values overlaid)** (show when instability starts)
2. **Pole-zero map (discrete Z-plane)** (show pole locations for each Kp)
3. **Phase margin vs. Kp** (plot showing 20° phase margin line)
4. **iq ripple amplitude vs. Kp** (should increase near instability)
5. **Settling time vs. Kp curve** (trade-off visualization)
6. **Bode plot: Open-loop G(z) for nominal Kp=0.0065** (show phase margin graphically)
7. **Step response at Kp limits** (0.0050 vs 0.0065 vs 0.0090)

**Results:**
- Simulation Date: __________
- Kp=0.0050: Stable ✓ / Unstable ✗, Settling time: __________ ms
- Kp=0.0060: Stable ✓ / Unstable ✗, Settling time: __________ ms
- Kp=0.0065: Stable ✓ / Unstable ✗, Settling time: __________ ms
- Kp=0.0075: Stable ✓ / Borderline ⚠ / Unstable ✗
- Kp=0.0090: Stable ✓ / Unstable ✗
- Gain Margin (% increase before instability): __________ %
- Phase Margin @ Kp=0.0065: __________ °
- Safety Factor Validation: ADEQUATE ✓ / MARGINAL ⚠ / INSUFFICIENT ✗
- Graphs saved as: Test_2_5_gain_margin_[date].png, Test_2_5_pole_zero_[date].png, Test_2_5_bode_[date].png
- Notes: _____________________________________________________________

---

## Test Phase 3: PWM Modulation Comparison

### Test 3.1: SVPWM @ 51V (Optimal)
**Objective:** Baseline performance with space vector PWM

| Parameter | Value | Unit |
|-----------|-------|------|
| Vdc | 51 | V |
| PWM Technique | SVPWM | - |
| Vmax Available | 51/√3 = 29.4 | V |
| Speed Target | 2262 | RPM |
| Load | 0 Nm | Nm |

**Expected Results:**
- Speed reaches 2262 RPM with 3.9V voltage headroom remaining
- Vq peaks at ~24V with margin
- Smooth acceleration, no saturation
- Efficiency: Baseline

**Pass Criteria:**
- [ ] Speed error < 2% (2218-2306 RPM range)
- [ ] Peak Vq < 26V (leaving 3.4V safety margin)
- [ ] Voltage circle never violated
- [ ] Clean current tracking in both phases

**Graphs to Capture & Save:**
1. **rpm_actual vs. time (should reach 2262 RPM)**
2. **iq_reference vs. iq_actual**
3. **Vq over time (peak value)**
4. **Vd and Vq on same plot (verify circle constraint)**
5. **Voltage utilization: √(Vd² + Vq²) vs. Vmax = 29.4V**
6. **Three-phase current Ia, Ib, Ic (validate 120° balance)**

**Results:**
- Simulation Date: __________
- Final Speed Achieved: __________ RPM
- Peak Vq: __________ V
- Voltage Margin Remaining: __________ V (29.4 - peak)
- Status: PASS ✓ / FAIL ✗
- Graphs saved as: Test_3_1_svpwm_51v_[date].png, Test_3_1_voltage_circle_[date].png
- Notes: _____________________________________________________________

---

### Test 3.2: Sine-Triangle @ 51V (Insufficient)
**Objective:** Demonstrate why traditional sine-triangle PWM fails at this operating point

| Parameter | Value | Unit |
|-----------|-------|------|
| Vdc | 51 | V |
| PWM Technique | Sine-Triangle | - |
| Vmax Available | 51/2 = 25.5 | V |
| Speed Target | 2262 | RPM |
| Load | 0 Nm | Nm |

**Expected Results (BAD):**
- Speed SATURATES before reaching 2262 RPM (likely ~2000 RPM)
- Vq clips at 25.5V (no headroom)
- PI output saturates (integrator windup visible)
- Difference from SVPWM: 262 RPM loss = 15% efficiency loss

**Pass Criteria (Demonstrating Failure):**
- [ ] Speed plateaus < 2262 RPM (proves saturation)
- [ ] Vq reaches 25.5V and stays there (clipping)
- [ ] Speed error > 5% (>100 RPM below target)
- [ ] Illustrates why SVPWM is superior

**Graphs to Capture & Save:**
1. **rpm_actual vs. time (should SATURATE below 2262 RPM)**
2. **Vq over time (should hit 25.5V and stay there—clipping)**
3. **Vd and Vq saturated envelope**
4. **iq_reference vs. iq_actual (should look "stuck" trying to deliver torque)**
5. **Comparison plot: SVPWM@51V vs. SineTriangle@51V speed curves side-by-side**
6. **PI output saturation indicator**

**Results:**
- Simulation Date: __________
- Max Speed Achieved: __________ RPM (should be < 2200)
- Speed Deficit vs. Target: __________ RPM
- Peak Vq: __________ V (should be clipped at 25.5V)
- Voltage Efficiency Loss: __________ %
- Status: EXPECTED FAILURE ✓ / UNEXPECTED PASS ✗
- Graphs saved as: Test_3_2_sine_51v_[date].png, Test_3_2_comparison_svpwm_vs_sine_[date].png
- Notes: _____________________________________________________________

---

### Test 3.3: Sine-Triangle @ 59V (Alternative, Higher Cost)
**Objective:** Prove SVPWM @ 51V achieves same performance as Sine-Triangle @ 59V (8V cheaper)

| Parameter | Value | Unit |
|-----------|-------|------|
| Vdc | 59 | V |
| PWM Technique | Sine-Triangle | - |
| Vmax Available | 59/2 = 29.5 | V |
| Speed Target | 2262 | RPM |
| Load | 0 Nm | Nm |

**Expected Results:**
- Speed reaches 2262 RPM (similar to SVPWM @ 51V)
- Performance match validates efficiency gain calculation
- Cost argument: 59V vs. 51V = ~$5-8 additional battery cost per unit

**Pass Criteria:**
- [ ] Final speed within 2% of SVPWM @ 51V result
- [ ] Peak Vq in similar range (~24-25V)
- [ ] Confirms SVPWM @ 51V = Sine-Triangle @ 59V equivalence

**Graphs to Capture & Save:**
1. **rpm_actual vs. time (should reach 2262 RPM, like SVPWM@51V)**
2. **Vq over time (peak ~24-25V, not clipped)**
3. **Three-phase current (120° balanced)**
4. **Overlay comparison: SVPWM@51V, SineTriangle@51V, SineTriangle@59V all together**
5. **Efficiency ratio visualization** (show why 59V ST = 51V SVPWM)

**Results:**
- Simulation Date: __________
- Final Speed Achieved: __________ RPM
- Speed Match vs. SVPWM@51V: ±__________ RPM (target: <50 RPM diff)
- Peak Vq: __________ V
- Efficiency Gain Validated: YES ✓ / NO ✗
- Cost Justification (59V vs 51V): +$__________ per motor
- Graphs saved as: Test_3_3_sine_59v_[date].png, Test_3_3_all_three_scenarios_[date].png
- Notes: _____________________________________________________________

---

### Test 2.3: Multiple Speed Steps (Speed Range Validation)
**Objective:** Verify controller works across full operating range (100-2262 RPM)

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Profile | Step: 0 → 500 → 1500 → 2262 → 1000 → 500 RPM | RPM |
| Step Timing | 0.5s each | s |
| Load | 0 Nm (no-load test) | Nm |
| Vdc | 51 | V |

**Expected Results:**
- Controller adapts PI gains dynamically (bandwidth scales with speed)
- Rise time similar at each speed (not degrading)
- No instability at low OR high speeds
- id remains at 0 across all speeds
- iq scales appropriately for each speed reference

**Pass Criteria:**
- [ ] All speed steps settle within 100ms
- [ ] No overshoot at any step (< 5% at each)
- [ ] id stays at 0 ±0.05A at all speeds
- [ ] Ripple consistent (not degrading with speed)
- [ ] Back-EMF compensation working (smooth response)

**Graphs to Capture & Save:**
1. **rpm_actual vs. rpm_reference** (full multi-step profile)
2. **iq_actual over each speed region (should scale linearly)**
3. **id (verify MTPA at all speeds)**
4. **Vq envelope** (increases with speed due to back-EMF)
5. **Back-EMF term: ωe × λm × iq** (should increase linearly with speed)
6. **Speed error at each step** (convergence rate)

**Results:**
- Simulation Date: __________
- Rise Time @ 500 RPM: __________ ms
- Rise Time @ 1500 RPM: __________ ms
- Rise Time @ 2262 RPM: __________ ms
- Max Overshoot (all speeds): __________ %
- Minimum id value: __________ A
- Maximum id value: __________ A
- Graphs saved as: Test_2_3_multistep_[date].png, Test_2_3_speed_range_[date].png
- Notes: _____________________________________________________________

---

### Test 2.4: Emergency Deceleration (Negative Speed Demand)
**Objective:** Validate braking/reverse capability and current limiting during rapid deceleration

| Parameter | Value | Unit |
|-----------|-------|------|
| Initial Speed | 2262 | RPM (steady state) |
| Speed Command | -2262 (full reverse) | RPM |
| Load | 0 | Nm |
| Expected Deceleration | Max (motor torque in reverse) | RPM/s |

**Expected Results:**
- No integrator windup during deceleration
- Current reverses polarity (negative iq for reverse torque)
- Deceleration rate: up to -8000 RPM/s (acceleration limit in reverse)
- Motor shouldn't stall or oscillate

**Pass Criteria:**
- [ ] Speed reaches negative direction
- [ ] Deceleration smooth (no chatter)
- [ ] iq goes negative (reverse current, reverse torque)
- [ ] Peak reverse current ≤ 1A (iq magnitude)

**Graphs to Capture & Save:**
1. **rpm_actual (shows deceleration to negative region)**
2. **iq_actual (crosses zero, goes negative)**
3. **id (should stay at zero through transition)**
4. **Acceleration profile: dω/dt** (shows rate of change)
5. **Current direction reversal**

**Results:**
- Simulation Date: __________
- Peak Deceleration Rate: __________ RPM/s
- Time to Reverse Polarity: __________ ms
- Min iq Value: __________ A (negative, should be ≥-1.0A)
- Max Reverse Speed Reached: __________ RPM
- Graphs saved as: Test_2_4_emergency_braking_[date].png
- Notes: _____________________________________________________________

---

### Test 3.4: Back-EMF Verification (Feedforward Term)
**Objective:** Validate that ωe × λm term is computed correctly and reduces steady-state voltage demand

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed | 2262 | RPM (constant) |
| Load | 0 Nm | Nm |

**Theory:**
- At steady state, no load: Vq_required = ωe × λm = 2605.9 rad/s × 0.00611 Wb ≈ 15.9V
- The PI controller should output only 15.9V (just enough to overcome back-EMF)
- Without back-EMF compensation: would need full 24V+, wasting voltage

**Expected Results:**
- Measured Vq at steady state ≈ 15.9V - 8.4Ω × iq_ripple
- Reduction vs. without feedforward: ≈ 8V savings
- PLL (phase-locking loop) tracks actual electrical angle θe
- No phase lag in estimated angle

**Pass Criteria:**
- [ ] Steady-state Vq ≈ 15.9V ±2V
- [ ] Back-EMF component visible in Vq decomposition
- [ ] Electrical frequency ωe tracks motor speed (2605.9 rad/s @ 2262 RPM)

**Graphs to Capture & Save:**
1. **Vq decomposed: Vq_pi_output vs. omega_e × lambda × iq**
2. **Back-EMF term: ωe × λm × iq alone** (should be ~15.9V constant @ 2262 RPM)
3. **Estimated electrical frequency ωe vs. actual**
4. **Cumulative electrical angle θe vs. rotor angle (mechanical)**

**Results:**
- Simulation Date: __________
- Steady-State Vq: __________ V
- Expected Back-EMF: 15.9 V
- PI Output Contribution: __________ V
- Back-EMF Estimate Accuracy: ±__________ % 
- Graphs saved as: Test_3_4_back_emf_[date].png
- Notes: _____________________________________________________________

---

### Test 3.5: Three-Phase Current Balance
**Objective:** Verify Clarke-Park transform is correct (currents should be 120° apart in a-b-c frame)

**Validation Metrics:**
- **Ia + Ib + Ic ≈ 0** (KCL: sum of three-phase currents = 0)
- **Phase spacing: 120° ±5° between phases**
- **Amplitude balance: Ia, Ib, Ic similar magnitude**

**Pass Criteria:**
- [ ] RMS(Ia + Ib + Ic) < 0.05A (sum near zero)
- [ ] Phase angle Ib @ 120° ±5° from Ia
- [ ] Phase angle Ic @ 240° ±5° from Ia
- [ ] Peak magnitudes within ±10% of each other

**Graphs to Capture & Save:**
1. **Three-phase currents Ia, Ib, Ic on same time-plot (show 120° spacing)**
2. **Phasor diagram: Ia vs Ib vs Ic** (should form equilateral triangle)
3. **Sum of three phases: Ia + Ib + Ic** (should be near zero, hovering around 0A)
4. **Phase angle vs. time** (mechanical angle × P = electrical angle)

**Results:**
- Simulation Date: __________
- Peak Ia: __________ A
- Peak Ib: __________ A
- Peak Ic: __________ A
- RMS(Ia + Ib + Ic): __________ A (should be < 0.05A)
- Phase angle Ib relative to Ia: __________ ° (should be ≈120°)
- Phase angle Ic relative to Ia: __________ ° (should be ≈240°)
- Graphs saved as: Test_3_5_phase_balance_[date].png, Test_3_5_phasor_[date].png
- Notes: _____________________________________________________________

---

### Test 3.6: RC Filter Effectiveness (Ripple Mitigation Comparison)
**Objective:** Validate RC filter design (τ=250µs) delivers superior ripple suppression vs. alternatives tested in Days 1-3

**Background:** Investigation tested 3 filtering approaches:
1. Moving average (10-tap): Failed—added 250µs delay, made oscillation worse
2. Mean block (averaging): Partial help only, didn't solve root cause
3. RC filter (τ=250µs): Success—smooth ripple without excess delay

This test quantifies the improvement and confirms final design choice.

| Parameter | Value | Unit |
|-----------|-------|------|
| Test Cases | Unfiltered, Moving Avg, RC Filter | - |
| Cutoff Frequency (RC) | 1/(2π×250µs) ≈ 637 | Hz |
| Speed Reference | 2262 | RPM |
| Load Profile | 0 Nm constant | Nm |
| Measurement: iq ripple | 20 kHz PWM + control artifacts | - |

**Expected Results:**
- **Unfiltered:** iq ripple ±0.4-0.5 A (all switching + control noise)
- **Moving Average 10-tap:** Ripple reduced, BUT settling slowed, oscillation exacerbated
- **RC Filter (τ=250µs):** Ripple ±0.2-0.3 A (PWM only, clean control response)

**Pass Criteria:**
- [ ] RC filter achieves ≥50% ripple reduction vs. unfiltered
- [ ] No additional oscillation induced by RC filter
- [ ] Phase lag from RC filter <5° @ 2 kHz control rate (delay <0.7 ms)
- [ ] Settling time similar to unfiltered (RC doesn't slow response)
- [ ] THD (Total Harmonic Distortion) reduced in iq spectrum

**Test Procedure:**
1. Run 3 identical simulations with different iq filtering:
   - Case A: No filter (raw PWM + control)
   - Case B: Moving average 10-tap (from Day 2 failed attempt)
   - Case C: RC filter with τ=250µs (final design)
2. Apply step: speed 0→2262 RPM @ t=0.1s
3. Log iq at 20 kHz sampling (capture all switching ripple)
4. Analyze: peak ripple, RMS noise, frequency content, settling time

**Graphs to Capture & Save:**
1. **iq time-domain (0.2-0.4 s window, zoomed to show 2-3 PWM cycles)**
   - All 3 cases overlaid (Unfiltered vs. Mov-Avg vs. RC)
2. **iq zoomed detail plot (5 PWM cycles ≈ 250µs window)**
   - Individual sub-plots for each filter case
3. **FFT(iq) DC-100 kHz** (show ripple spectrum for each case)
   - Peaks: 20 kHz (switching), harmonics, control artifacts
4. **RMS ripple amplitude vs. filter type** (bar chart: 3 bars)
5. **Settling time overlay** (speed response for all 3, show delay)
6. **Phase lag measurement:** iq_filtered vs. iq_reference (time shift)
7. **Control performance plot:** iq_ref vs. iq_actual for RC case (should track well)

**Results:**
- Simulation Date: __________
- Unfiltered iq Peak Ripple: ±__________ A
- Moving Average iq Peak Ripple: ±__________ A
- RC Filter iq Peak Ripple: ±__________ A
- Ripple Reduction (RC vs. Unfiltered): __________ %
- RC Filter Phase Lag @ 2 kHz: __________ °
- Phase Lag as Time Delay: __________ µs
- Settling Time Change (RC vs. Unfiltered): __________ ms difference
- RC Filter Design Validation: PASS ✓ / MARGINAL ⚠ / FAIL ✗
- Graphs saved as: Test_3_6_filter_comparison_[date].png, Test_3_6_ripple_fft_[date].png, Test_3_6_settling_overlay_[date].png
- Notes: _____________________________________________________________

---

## Test Phase 4: Advanced Scenarios

### Test 4.1: Integrator Windup During Saturation
**Objective:** Verify back-calculation anti-windup prevents PI integrator runaway

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Reference | 2262 | RPM |
| Load | 0.0934 Nm (overload) | Nm |
| Expected Behavior | Motor can only produce 0.0671 Nm, will decelerate | - |

**Expected Results:**
- Pi integrator term Ki × error grows initially
- When speed saturates, error converges to new equilibrium
- Integrator does NOT continue growing (anti-windup active)
- Speed settles at lower RPM, not oscillation

**Pass Criteria:**
- [ ] No velocity oscillation (settles in <500 ms)
- [ ] iq does not exceed 1.1 A (10% overshoot from limit)
- [ ] Speed droop stable (no ringing)

**Graphs to Capture & Save:**
1. **Velocity around load step (1.5-2.5s)**
2. **iq_actual (shows saturation at 1A)**
3. **Pi integrator output (Ki × integral(error))** (should plateau, not grow unbounded)
4. **rpm_error (should converge to constant negative error at new equilibrium)**
5. **Acceleration profile (dω/dt)** (should show deceleration then settling)
6. **Load torque step function (reference)**

**Results:**
- Simulation Date: __________
- Peak iq During Transient: __________ A
- Overshoot %: __________
- Settling Time: __________ ms
- Integrator Windup Detected? YES ✗ / NO ✓
- Final steady-state iq: __________ A (should be 1.0A at new equilibrium)
- Graphs saved as: Test_4_1_antiwindup_[date].png, Test_4_1_integrator_[date].png
- Notes: _____________________________________________________________

---

## Test Phase 5: Voltage Circle Constraint

### Test 5.1: Dq Voltage Saturation Verification
**Objective:** Confirm Vd and Vq respect the voltage circle

**Constraint:** 
$$\sqrt{V_d^2 + V_q^2} \leq V_{max} = 29.4 \text{ V}$$

**Test Procedure:**
- Run full simulation (Phase 2, all transients)
- Log Vd and Vq at 1 kHz sampling
- Calculate radius: $r(t) = \sqrt{V_d(t)^2 + V_q(t)^2}$
- Find peak radius

**Expected Results:**
- Peak radius ≤ 29.4V at all times
- If violated, saturation block is broken

**Pass Criteria:**
- [ ] Peak radius: __________ V ≤ 29.4V

**Graphs to Capture & Save:**
1. **Vd vs. Vq (parametric/phase-plane plot, should stay inside circle)**
2. **Voltage radius r(t) = √(Vd² + Vq²) vs. time** (should never exceed 29.4V)
3. **Vd and Vq on same time-plot**
4. **Circle constraint visualization: actual trajectory overlaid on theoretical circle boundary**
5. **Zoomed view of any near-limit points**

**Results:**
- Simulation Date: __________
- Max Radius Found: __________ V
- Constraint Violated? YES ✗ / NO ✓
- Peak Violation Amount: __________ V over limit (if any)
- Number of violations: __________
- Graphs saved as: Test_5_1_voltage_circle_[date].png, Test_5_1_radius_envelope_[date].png
- Notes: _____________________________________________________________

---

### Test 5.2: Settling Time Characterization (Performance Validation Across Operating Range)
**Objective:** Map settling time across speed and load conditions; validate ~150 ms settling time observed in Day 3 is achievable throughout operating range

**Rationale:** Settling time determines system responsiveness. Must be consistent across 0-2262 RPM and 0-100% load to ensure predictable performance for application (gimbal motor demanding ~150 ms response).

| Parameter | Value | Unit |
|-----------|-------|------|
| Speed Points Tested | 500, 1000, 1500, 2262 | RPM |
| Load Points Tested | 0, 50%, 100% of max (0.0671 Nm) | Nm |
| Test Matrix | 4 speeds × 3 loads = 12 conditions | - |
| Settling Criterion | Speed ±2% of reference | - |
| Nominal Expected Settling | ~150 | ms (from Day 3 validation) |

**Expected Results:**
- Settling time relatively flat across speed range (2% variation max)
- Settling time increases slightly under heavy load (dynamic response slower)
- No speed "hunting" or oscillation near setpoint
- Overshoot consistent: 0-5% across all conditions

**Pass Criteria:**
- [ ] Settling time: 140-160 ms @ all speeds, no load
- [ ] Settling time: 150-180 ms @ all speeds, 100% load (acceptable increase)
- [ ] Overshoot: ≤5% in all conditions
- [ ] No hunting or limit-cycle oscillation
- [ ] Response time degrades gracefully with load

**Test Procedure:**
1. Create 12 test scenarios (speed × load combinations)
2. For each: Apply speed step → hold for 1.5s → log response
3. Measure: settling time (2% band), overshoot %, peak current
4. Generate heatmap and curve fits

**Graphs to Capture & Save:**
1. **Speed response matrix (4×3 grid):** Individual plots for each speed/load combo
2. **Settling time vs. Speed curve** (no load condition)
   - X-axis: Speed (0-2262 RPM)
   - Y-axis: Settling time (ms)
   - Should be flat ±20 ms
3. **Settling time vs. Load curve** (at 2262 RPM)
   - X-axis: Load % (0-100%)
   - Y-axis: Settling time (ms)
   - Should show graceful increase, not sharp jumps
4. **Overshoot vs. Speed** (bar chart, all 12 conditions)
5. **Heatmap: Settling Time (Speed vs. Load)** (color gradient 0-200ms)
6. **Response time envelope plot** (all 12 speed traces overlaid, normalized)
7. **Peak current vs. Load** (verify current limiting at high load)

**Results Summary Table:**

| Speed (RPM) | Load=0% | Load=50% | Load=100% | Notes |
|-------------|---------|----------|-----------|-------|
| 500 | ____ms | ____ms | ____ms | |
| 1000 | ____ms | ____ms | ____ms | |
| 1500 | ____ms | ____ms | ____ms | |
| 2262 | ____ms | ____ms | ____ms | |

**Additional Results:**
- Simulation Date: __________
- Min Settling Time Found: __________ ms (at __________ RPM, __________ load)
- Max Settling Time Found: __________ ms (at __________ RPM, __________ load)
- Settling Time Variance: ±__________ ms
- Max Overshoot Observed: __________ % (at which condition?)
- Response Consistency: GOOD ✓ / ACCEPTABLE ⚠ / POOR ✗
- Graphs saved as: Test_5_2_settling_matrix_[date].png, Test_5_2_settling_curves_[date].png, Test_5_2_heatmap_[date].png
- Notes: _____________________________________________________________

---

## Test Phase 6: Hardware Validation (Future)

### Test 6.1: Speed Tracking @ 12V (Initial Safety Test)
**Objective:** First hardware spin-up, low voltage, no load

| Parameter | Value | Unit |
|-----------|-------|------|
| Vdc | 12 | V (initial safety) |
| Speed Reference | 500 | RPM (conservative start) |
| Load | 0 | Nm |
| Expected Speed | ~500 RPM | RPM |

**Pass Criteria:**
- [ ] Motor spins smoothly (no cogging, stuttering)
- [ ] Encoder reads correctly
- [ ] Speed controller proportional response visible
- [ ] No excessive current draw (< 0.5A at 500 RPM, no load)

**Results:**
- Hardware Date: __________
- Motor Status: Spinning ✓ / No-spin ✗ / Overcurrent ✗
- Speed Achieved: __________ RPM
- Current Draw: __________ A
- Encoder Working? YES ✓ / NO ✗
- Notes: _____________________________________________________________

---

### Test 6.2: Speed Tracking @ 51V (Full Performance)
**Objective:** Rated voltage operation, gradual load ramp

**Pass Criteria:**
- [ ] Reaches 2262 RPM reference
- [ ] Handles 0-0.0671 Nm load ramp
- [ ] Current limit enforced at 1A
- [ ] Hardware-sim correlation within ±5%

**Results:**
- Hardware Date: __________
- Motor Status: Spinning ✓ / No-spin ✗ / Overcurrent ✗
- Speed Achieved: __________ RPM
- Current Draw: __________ A
- Encoder Working? YES ✓ / NO ✗
- Graphs saved as: Test_6_1_hardware_12v_[date].png
- Notes: _____________________________________________________________

---

### Test 6.2: Speed Tracking @ 51V (Full Performance)
**Objective:** Rated voltage operation, gradual load ramp

**Graphs to Capture & Save:**
1. **rpm_actual vs. encoder readout** (raw ADC, no filter)
2. **iq_actual from current sensor vs. simulated model prediction**
3. **Ia, Ib, Ic from shunt resistors** (phase currents, verify 120° balance)
4. **Vq output from XMC4700 PWM** (should match simulation)
5. **Speed error = actual - reference**

**Pass Criteria:**
- [ ] Reaches 2262 RPM reference
- [ ] Handles 0-0.0671 Nm load ramp
- [ ] Current limit enforced at 1A
- [ ] Hardware-sim correlation within ±5%

**Results:**
- Hardware Date: __________
- Final Speed: __________ RPM
- Sim Prediction: 2262 RPM
- Correlation Error: __________ %
- Max Current: __________ A
- Encoder resolution: __________ CPR (counts per revolution)
- Graphs saved as: Test_6_2_hardware_51v_[date].png, Test_6_2_hardware_vs_sim_[date].png
- Notes: _____________________________________________________________

---

## Additional Recommended Tests (For Future Phases)

### Optional Test A: Parameter Sensitivity
**Why:** Understand robustness to datasheet tolerances

- Kp ±10% variation
- Ki ±10% variation  
- L ±15% variation (inductance manufacturing tolerance)
- R ±10% variation (resistance temperature variation)

**Graphs:** Speed response under±10% Kp, ±10% Ki separately

---

### Optional Test B: Voltage Saturation Limits Under Load
**Why:** Understand when system hits hard limits

- Test at rated torque (0.0671 Nm) at varying speeds
- Plot max Vq required vs. speed
- Find speed ceiling @ Vdc = 51V (should be ≈2262 RPM)

---

### Optional Test C: Sensorless Observer (Future Phase, Not Included Here)
**Why:** Eliminate encoder requirement

- Estimated angle θ_est vs. actual θ (from encoder)
- Angle estimation error @ each speed
- Pass/fail criterion: ±5° error at full speed

---

## Summary Table: Graph Files to Archive

| Test ID | Description | Critical Graphs | File Pattern |
|---------|-------------|-----------------|--------------|
| 1.1 | Speed Tracking (No Load) | Speed response, iq tracking, id zero | Test_1_1_speed_tracking_*.png |
| 1.2 | Current Ripple | FFT, zoomed iq, three-phase balance | Test_1_2_ripple_*.png |
| 2.1 | Load Ramp (0→0.0671Nm) | Speed droop, iq ramp, voltages, power | Test_2_1_load_ramp_*.png |
| 2.2 | Load Step (0→0.0934Nm) | Deceleration transient, anti-windup | Test_2_2_load_step_*.png |
| 2.3 | Multi-Speed Steps | Speed range validation, rise time @ speeds | Test_2_3_multistep_*.png |
| 2.4 | Emergency Braking | Reverse direction, negative iq, dmax/dt | Test_2_4_emergency_braking_*.png |
| 3.1 | SVPWM @ 51V | Speed achieved, voltage utilization, margin | Test_3_1_svpwm_51v_*.png |
| 3.2 | Sine-Triangle @ 51V | Speed saturation, clipping, comparison | Test_3_2_sine_51v_*.png |
| 3.3 | Sine-Triangle @ 59V | Equivalence to SVPWM@51V, efficiency ratio | Test_3_3_sine_59v_*.png |
| 3.4 | Back-EMF Verification | Vq decomposition, ωe×λm term, PLL accuracy | Test_3_4_back_emf_*.png |
| 3.5 | Phase Balance | Three-phase currents, 120° spacing, KCL | Test_3_5_phase_balance_*.png |
| 4.1 | Anti-Windup | Integrator saturation, speed recovery | Test_4_1_antiwindup_*.png |
| 5.1 | Voltage Circle | Vd-Vq parametric, radius envelope | Test_5_1_voltage_circle_*.png |
| 6.1 | Hardware 12V Safety | Motor spin-up, encoder, current limit | Test_6_1_hardware_12v_*.png |
| 6.2 | Hardware 51V Rated | Full perf, hardware vs sim correlation | Test_6_2_hardware_51v_*.png |

**Total graphs to capture: ~40-50 PNG files** (ensures full comprehensive validation proof)

---

## Summary & Sign-Off

| Test | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 | Phase 6 | Status |
|------|---------|---------|---------|---------|---------|---------|--------|
| Nominal | ✓ | | | | | | |
| Ripple | ✓ | | | | | | |
| Load Ramp | | ✓ | | | | | |
| Load Step | | ✓ | | | | | |
| SVPWM@51V | | | ✓ | | | | |
| ST@51V | | | ✓ | | | | |
| ST@59V | | | ✓ | | | | |
| Anti-Windup | | | | ✓ | | | |
| V-Circle | | | | | ✓ | | |
| HW 12V | | | | | | ✓ | |
| HW 51V | | | | | | ✓ | |

**Overall Validation Status:** ________________________

**Validated By:** _______________________  
**Date:** _______________________  
**Sign-Off:** Simulation ✓ / Hardware ✓ / Pending

---

## Lessons Learned & Issues

| Issue | Root Cause | Resolution | Status |
|-------|-----------|-----------|--------|
| Vq_req ripple (±1-2V) causing negative spikes in inverter positive half-cycle | In discrete MATLAB simulation, motor speed is read from plant model only @ 2kHz discrete sample times (not continuously). This creates discrete quantized steps in speed output → discrete steps in speed_error → discrete steps propagated through Speed PI output (Vq_req command). SVPWM modulator receives this rippled Vq_req and tries to "follow" it by modulating PWM rapidly, creating switching artifacts (negative spikes in positive half). Root cause: discrete sampling of continuous plant dynamics from motor equation. Not a motor equation sign problem or inertia problem. | Add Discrete Transfer Function filter (Num=0.01, Den=[1 -0.99], Ts=1/20000) between Speed PI output and SVPWM input to smooth Vq_req before modulator sees it. Filter pole at 0.99 provides strong low-pass (~160 Hz cutoff) with acceptable 8° phase lag @ 2kHz control rate, preventing modulator from responding to discrete sampling ripple. | ✓ Resolved |
| | | | |

---

**Document Version:** 1.0  
**Last Updated:** April 1, 2026
