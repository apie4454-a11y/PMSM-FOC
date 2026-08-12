# Mathematical Foundation — PMSM FOC & EKF

**Purpose:** Core mathematical framework underlying the FOC controller and EKF observer. All equations derive from first principles; all parameters sourced from motor datasheet and physics-based calculations.

---

## 1. Motor Model

### Electrical Equations (dq Frame)

For a permanent magnet synchronous motor, the voltage equations in the rotating d-q frame are:

$$v_d = R \times i_d + L \frac{di_d}{dt} - \omega_e L i_q$$

$$v_q = R \times i_q + L \frac{di_q}{dt} + \omega_e L i_d + \omega_e \lambda_m$$

Where:
- $v_d, v_q$ = d-q axis voltages [V]
- $i_d, i_q$ = d-q axis currents [A]
- $R$ = phase resistance [Ω]
- $L$ = d-q axis inductance [H]
- $\omega_e$ = electrical angular velocity [rad/s]
- $\lambda_m$ = permanent magnet flux linkage [Wb]

### Mechanical Equation

$$\tau_e - \tau_load = J \frac{d\omega_m}{dt} + B \omega_m$$

Where:
- $\tau_e = 1.5 \times p_p \times \lambda_m \times i_q$ = electromagnetic torque
- $p_p$ = number of pole pairs
- $J$ = moment of inertia [kg·m²]
- $B$ = viscous damping [N·m·s/rad]
- $\omega_m$ = mechanical speed [rad/s]

### Motor Parameters (iPower GM3506)

| Parameter | Symbol | Value | Unit | Source |
|-----------|--------|-------|------|--------|
| Phase resistance | $R$ | 5.6 | Ω | Datasheet |
| Phase inductance | $L$ | 2.0 | mH | Datasheet |
| Back-EMF constant | $K_v$ | 141.4 | RPM/V | Datasheet (2262 RPM @ 16V) |
| Flux linkage | $\lambda_m$ | 0.00611 | Wb | $\lambda_m = 1/(K_v \times 2\pi/60 \times p_p)$ |
| Pole pairs | $p_p$ | 11 | — | 22P configuration |
| Torque constant | $K_t$ | 0.0867 | N·m/A | $K_t = 1.5 \times p_p \times \lambda_m$ |
| Moment of inertia | $J$ | 3.8×10⁻⁶ | kg·m² | Physics-based (rotor geometry) |
| Damping coefficient | $B$ | 4.5×10⁻⁵ | N·m·s/rad | Physics-based (no-load power loss) |

**Derivation Reference:** [Motor_Parameters.m](Motor_Parameters.m) and [motor_parameters_derivation.md](motor_parameters_derivation.md)

---

## 2. PI Gain Design (Physics-Based Cascade)

### Design Principle: Bandwidth-Driven Pole Placement

For a cascaded FOC system, the PI gains are derived to achieve **target bandwidth** at each control level, exploiting the natural 10:1 frequency separation between electrical (fast) and mechanical (slow) dynamics.

### Current Loop (Inner, 20 kHz, 2000 Hz Bandwidth)

**Design requirement:** First-order response with bandwidth $f_{bw} = 2000$ Hz.

From the RL circuit model $L \frac{di}{dt} = v - iR$, a proportional gain raises the natural frequency:

$$K_p = L \times 2\pi \times f_{bw}$$

$$K_p = 0.002 \times 2\pi \times 2000 = 25.1 \, \text{[V/A]}$$

Integral term removes steady-state error:

$$K_i = R \times 2\pi \times f_{bw}$$

$$K_i = 5.6 \times 2\pi \times 2000 = 70,372 \, \text{[V/(A·s)]}$$

**Rationale:** $K_p$ cancels $L$; $K_i$ cancels $R$. Result: closed-loop pole at $2\pi f_{bw}$ with zero overshoot.

### Speed Loop (Outer, 2 kHz update, 200 Hz Bandwidth)

**Design requirement:** Bandwidth $f_{bw} = 200$ Hz (10× slower than current loop).

From the mechanical equation $J \frac{d\omega}{dt} = \tau_e - B\omega$:

$$K_p = J \times 2\pi \times f_{bw}$$

$$K_p = 3.8 \times 10^{-6} \times 2\pi \times 200 = 0.00477 \, [\text{A/(rad/s)}]$$

$$K_i = B \times 2\pi \times f_{bw}$$

$$K_i = 4.5 \times 10^{-5} \times 2\pi \times 200 = 0.0565 \, [\text{A/(rad/s)²}]$$

**Rationale:** Same pole-placement principle. Speed controller sees current loop as fast actuator; can assume proportional response.

### Cascade Stability

The 10:1 frequency separation ensures:
- Inner loop (current) settles in ~5 cycles at 20 kHz = 2.5 ms
- Outer loop (speed) at 2000 Hz sees quasi-steady response from inner loop
- Decoupled design: tune current loop independently, then speed loop

**See also:** [CONTROL_DESIGN_DECISIONS.md](CONTROL_DESIGN_DECISIONS.md#2-why-10-frequency-separation)

---

## 3. Extended Kalman Filter (EKF) Observer

### Motivation

Standard FOC requires real-time rotor angle feedback (encoder) for dq transformation. EKF eliminates this dependency by **estimating angle and speed from current measurements alone**, exploiting the back-EMF terms as observability signal.

### System State

$$x = \begin{bmatrix} i_\alpha \\ i_\beta \\ \omega_e \\ \theta_e \end{bmatrix}$$

Where:
- $i_\alpha, i_\beta$ = stationary (fixed) frame currents [A]
- $\omega_e$ = electrical angular velocity [rad/s]
- $\theta_e$ = electrical rotor angle [rad]

**Frame choice:** α-β (stationary), NOT d-q (rotating). Critical reason: α-β frame is independent of estimated state $\theta_e$, avoiding circular dependencies in the measurement model.

### Process Model (Continuous Time)

$$\dot{i}_\alpha = -\frac{R}{L} i_\alpha + \frac{\lambda_m}{L} \omega_e \sin(\theta_e) + \frac{1}{L} v_\alpha$$

$$\dot{i}_\beta = -\frac{R}{L} i_\beta - \frac{\lambda_m}{L} \omega_e \cos(\theta_e) + \frac{1}{L} v_\beta$$

$$\dot{\omega}_e = 0 \quad \text{(assumed constant over } T_s \text{)}$$

$$\dot{\theta}_e = \omega_e$$

**Key insight:** Back-EMF terms $\lambda_m \omega_e \sin(\theta_e)$ and $\lambda_m \omega_e \cos(\theta_e)$ couple $\omega_e$ and $\theta_e$ into the current dynamics. These terms **provide observability** — the filter can infer $\omega_e, \theta_e$ from $i_\alpha, i_\beta$ variations.

### Measurement Model

$$z = \begin{bmatrix} i_\alpha^{meas} \\ i_\beta^{meas} \end{bmatrix} = H \times x$$

$$H = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \end{bmatrix}$$

**Measurement:** Only currents are measured; angle and speed are purely estimated via the Kalman filter.

### Jacobian (Linearization for EKF)

The continuous-time Jacobian of the process model is:

$$A = \begin{bmatrix} 
-\frac{R}{L} & 0 & \frac{\lambda_m}{L}\sin(\theta_e) & \frac{\lambda_m}{L}\omega_e\cos(\theta_e) \\
0 & -\frac{R}{L} & -\frac{\lambda_m}{L}\cos(\theta_e) & \frac{\lambda_m}{L}\omega_e\sin(\theta_e) \\
0 & 0 & 0 & 0 \\
0 & 0 & 1 & 0
\end{bmatrix}$$

Discrete state transition matrix via Euler integration:
$$F = I + A \times T_s$$

### Discretization (Euler Method)

Sampling time: $T_s = 50$ μs (20 kHz ISR)

**Prediction step:**
$$x_{k+1}^- = x_k + f_c(x_k, u_k) \times T_s$$

**Covariance prediction:**
$$P_{k+1}^- = F \times P_k \times F^T + Q$$

**Kalman gain:**
$$K = \frac{P_k^- H^T}{H P_k^- H^T + R}$$

**Correction step:**
$$x_k = x_k^- + K (z_k - H x_k^-)$$
$$P_k = (I - K H) P_k^-$$

### Noise Covariances (Tuned)

**Process noise (Q):** Uncertainty in state dynamics
$$Q = \text{diag}(1.0, 1.0, 800.0, 100.0) \times T_s$$

- High $Q_{\omega}$ (800.0) and $Q_\theta$ (100.0): Allow fast speed/angle tracking during transients
- Low $Q_i$ (1.0): Currents are well-modeled by RL circuit
- Scaled by $T_s$ to maintain consistency across different sample rates

**Measurement noise (R):** Sensor uncertainty
$$R = \text{diag}(1 \times 10^{-4}, 1 \times 10^{-4})$$

- Low values: Simulink measurements in SIL are clean
- In real hardware, increase $R$ if noise floor is higher

### Activation Condition

$$\text{EKF active} \iff |\text{RPM}| \geq 100 \text{ RPM}$$

**Reason:** Below 100 RPM, back-EMF signal $\lambda_m \omega_e$ is too weak relative to measurement noise. Filter cannot distinguish $\theta$ effects and will diverge. Threshold ensures SNR sufficiency for observability.

### Coordinate Transform (Output Stage)

Once $i_\alpha, i_\beta, \theta_e$ are estimated, compute d-q currents via **Park transformation**:

$$i_d^{est} = i_\alpha^{est} \cos(\theta_e^{est}) + i_\beta^{est} \sin(\theta_e^{est})$$

$$i_q^{est} = -i_\alpha^{est} \sin(\theta_e^{est}) + i_\beta^{est} \cos(\theta_e^{est})$$

These estimated currents feed directly into the FOC current controllers (replacing encoder-based measurements).

---

## 4. Observability Proof Sketch

### Why α-β Frame Avoids Circular Dependency

If we used a **d-q rotating frame** for the EKF:
- Process model would depend on **measured $\theta$** to transform $i_a, i_b \to i_d, i_q$
- But $\theta$ is what the filter is trying to estimate
- **Circular dependency:** Measurement model would depend on estimated state $\theta$
- **Result:** System not observable; filter diverges

**Solution:** Use **stationary α-β frame**:
- Measurements ($i_\alpha, i_\beta$) are directly in the frame of the state vector
- Process model contains back-EMF coupling terms: $\lambda_m \omega_e \sin(\theta_e)$, $\lambda_m \omega_e \cos(\theta_e)$
- These nonlinear terms link $\theta_e, \omega_e$ to measured currents
- **Result:** Observable system; EKF converges

### Observability Criteria

The system $\dot{x} = f(x, u)$, $z = Hx$ is locally observable if the **observability rank** equals the state dimension (4).

For the PMSM EKF:
- Rank test: The set $\{H, \nabla H \times f, \nabla^2 H \times f, ...\}$ spans $\mathbb{R}^4$
- Physical interpretation: Back-EMF terms create coupling between electrical states ($i_\alpha, i_\beta$) and mechanical states ($\omega_e, \theta_e$)
- Validation: SIL testing shows convergence within 0.2 s post-activation ✅

---

## 5. Simulink Implementation

The complete FOC system is simulated in **Simulink** with:
- Continuous-time motor model (ideal for validation)
- Discrete-time controllers (20 kHz current loop, 2 kHz speed loop)
- EKF observer (discretized, running at 20 kHz)
- SIL validation: Motor ramp (0→1700 RPM), load transient at t=1.3s, steady-state tracking

**Model:** [simulation_basic_sil.slx](simulation_basic_sil.slx)  
**Validation results:** [session_10-08-2026.md](session_10-08-2026.md)

---

## References

- [Motor_Parameters.m](Motor_Parameters.m) — Electrical & mechanical parameters
- [CONTROL_DESIGN_DECISIONS.md](CONTROL_DESIGN_DECISIONS.md) — Design rationale for each decision
- [motor_parameters_derivation.md](motor_parameters_derivation.md) — Detailed physics-based derivation of J, B
- [session_10-08-2026.md](session_10-08-2026.md) — EKF validation plots and results

---

**Last Updated:** 10-08-2026
