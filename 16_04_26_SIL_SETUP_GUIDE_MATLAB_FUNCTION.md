# SIL Setup Guide - MATLAB Function Block

## Function Signature

```matlab
[vd_ref, vq_ref, id_out, iq_out, te_out, speed_error] = ...
    foc_algorithm_sil(id_motor, iq_motor, motor_rpm, omega_ele, speed_ref, max_flux)
```

## Inputs (6)

| Input | Units | Source |
|-------|-------|--------|
| `id_motor` | A | Motor_Model output |
| `iq_motor` | A | Motor_Model output |
| `motor_rpm` | RPM | Motor_Model output |
| `omega_ele` | rad | Motor_Model output |
| `speed_ref` | RPM | Signal Editor / Test input |
| `max_flux` | Wb | Motor_Model output |

## Outputs (6)

| Output | Units | Destination |
|--------|-------|-------------|
| `vd_ref` | V | PWM Pulses block |
| `vq_ref` | V | PWM Pulses block |
| `id_out` | A | Scope (debug) |
| `iq_out` | A | Scope (debug) |
| `te_out` | Nm | Scope (debug) |
| `speed_error` | RPM | Scope (debug) |

---

## Quick Start

### Step 1: Create SIL Model Copy

```matlab
cd 'c:\Users\rizwansadik\OneDrive\Documents\Simulations\MATLAB\Machines\PMSM'
copyfile('simulation_basic.slx', 'simulation_basic_sil.slx')
open simulation_basic_sil.slx
```

### Step 2: Delete Old Controllers

In Simulink (SIL model):
- Delete `speed_controller` subsystem
- Delete `current_controller` subsystem
- Keep: Motor_Model, 3Phase_VSI_Maths_Model, PWM Pulses block

### Step 3: Add MATLAB Function Block

1. **Simulink Library** → Simulink → User-Defined Functions → **MATLAB Function**
2. Drag into model
3. Replace default code:
```matlab
function [vd_ref, vq_ref, id_out, iq_out, te_out, speed_error] = fcn(id_motor, iq_motor, motor_rpm, omega_ele, speed_ref, max_flux)
    [vd_ref, vq_ref, id_out, iq_out, te_out, speed_error] = foc_algorithm_sil_16_04_26(id_motor, iq_motor, motor_rpm, omega_ele, speed_ref, max_flux);
end
```

### Step 4: Configure Block Inputs/Outputs

**Right-click MATLAB Function block → Edit**

**Inputs (6 scalar doubles):**
- `id_motor`
- `iq_motor`
- `motor_rpm`
- `omega_ele`
- `speed_ref`
- `max_flux`

**Outputs (6 scalar doubles):**
- `vd_ref`
- `vq_ref`
- `id_out`
- `iq_out`
- `te_out`
- `speed_error`

### Step 5: Connect Signals

**Inputs:**
```
Motor_Model → id_motor (output 1)
Motor_Model → iq_motor (output 2)
Motor_Model → motor_rpm (output 3)
Motor_Model → omega_ele (output 4)
Signal Editor → speed_ref
Motor_Model → max_flux (output 5)
```

**Main Outputs (to PWM):**
```
vd_ref → PWM Pulses block (vd_ref input)
vq_ref → PWM Pulses block (vq_ref input)
```

**Debug Outputs (optional):**
```
id_out, iq_out, te_out, speed_error → Mux → Scope
```

### Step 6: Run SIL Test

```matlab
sim('simulation_basic_sil');
```

### Step 7: Validate SIL vs MIL

```matlab
% Run both models
sim('simulation_basic');    % MIL
sim('simulation_basic_sil'); % SIL

% Compare results
% Should match within < 1% RMS error on speed tracking
```

---

## Algorithm Inside foc_algorithm_sil.m

**Speed Loop:**
```
rpm_error = speed_ref - motor_rpm [RPM]
→ convert to rad/s
→ PI(Kp=0.0004732, Ki=0.056549)
→ Te_ref, saturate [-0.05, 0.1] Nm
```

**d-Axis Loop (id_ref = 0):**
```
id_error = 0 - id_motor
→ PI(Kp=37.699, Ki=105560)
→ vd_star [V]
vd_decouple = omega_ele × L × iq_motor
vd_ref = vd_star - vd_decouple
→ saturate [-29.5, +29.5] V
```

**q-Axis Loop:**
```
iq_ref = Te_ref / (1.5 × pp × max_flux)
iq_error = iq_ref - iq_motor
→ PI(Kp=37.699, Ki=105560)
→ vq_star [V]
vq_decouple = omega_ele × L × id_motor + omega_ele × max_flux
vq_ref = vq_star + vq_decouple
→ saturate [-29.5, +29.5] V
```

---

## Troubleshooting

**"Function 'foc_algorithm_sil' not found"**
- Ensure `foc_algorithm_sil.m` is in MATLAB current directory
- Try: `which foc_algorithm_sil`

**SIL results don't match MIL**
- Verify all 6 inputs connected correctly
- Check signal order (id, iq, motor_rpm, omega_ele, speed_ref, max_flux)
- Compare Motor_Parameters.m values with constants in function (Kp, Ki gains)

**PI integrators not resetting**
- Clear workspace: `clear all`
- Close/reopen model
- Persistent state will reset

---

## Next: Convert to C for XMC4700

Once SIL passes (< 1% error):
1. `foc_algorithm_sil.c` has the C implementation (already provided)
2. Adapt register names for CCU8 (from motor_constants.md)
3. Deploy to XMC4700 firmware

