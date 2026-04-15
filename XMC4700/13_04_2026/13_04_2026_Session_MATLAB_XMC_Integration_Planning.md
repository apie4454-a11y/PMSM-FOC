# Session 13_04_2026: MATLAB-XMC Integration Planning for FOC

## Overview
**Objective:** Plan integration of validated MATLAB FOC algorithm with XMC4700 PWM hardware  
**Status:** 🔄 **Planning Phase Complete — Architecture Proposed**  
**Key Concept:** Use XMC as PWM generation engine; MATLAB computes FOC control laws via UART communication

---

## Context from 12_04

On 12_04_2026, the following was achieved:
- ✅ CCU4 Capture module correctly configured to read encoder PWM signal
- ✅ Phase identification complete (motor phases 1-2-3 characterized via DC test)
- ✅ Ready to test encoder-to-PWM conversion for rotor angle feedback

**Current System State:**
- XMC can read encoder PWM duty cycle → extract rotor angle
- XMC has CCU8 module for PWM generation
- MATLAB has validated FOC algorithm (from earlier PMSM simulation)

---

## 13_04 Planning: Bridging MATLAB and XMC

### The Concept

**Goal:** Leverage the strength of each platform:
- **MATLAB:** Proven FOC algorithm (clarke/park transforms, PI controllers, modulation)
- **XMC4700:** Real-time PWM generation, encoder feedback interface

**Proposed Architecture:**

```
┌─────────────────────┐
│   MATLAB (PC)       │
│  - FOC Algorithm    │
│  - Phase Voltage    │
│    Calculation      │
└──────────┬──────────┘
           │ UART
        Phase Voltages
        (V_alpha, V_beta)
           │
           ▼
┌─────────────────────┐
│  XMC4700 (Board)    │
│  - Read Encoder     │
│  - Generate PWM     │
│  - Motor Interface  │
└──────────┬──────────┘
           │ UART
        PWM Feedback
        (Pulse Data)
           │
           ▼
┌─────────────────────┐
│   MATLAB (PC)       │
│  - Verify PWM       │
│  - Log Data         │
│  - Tune Parameters  │
└─────────────────────┘
```

### Communication Protocol (Proposed)

**MATLAB → XMC (Phase Voltages):**
```
UART Frame: [START] [V_d] [V_q] [Theta] [MODE] [CHECKSUM] [END]
- V_d, V_q: Direct/Quadrature axis voltages (2 bytes each)
- Theta: Rotor angle for reference (2 bytes)
- MODE: Control mode (open-loop test vs closed-loop)
- Baud Rate: 115,200 baud
```

**XMC → MATLAB (PWM Feedback):**
```
UART Frame: [START] [PWM_A] [PWM_B] [PWM_C] [ENCODER_RAW] [CHECKSUM] [END]
- PWM_A, PWM_B, PWM_C: Current PWM duty cycles (1 byte each)
- ENCODER_RAW: Raw encoder count (2 bytes)
- Feedback interval: ~1 kHz
```

### Workflow

1. **MATLAB sends voltage command** via UART
   - Example: "Set V_d=10V, V_q=5V"

2. **XMC receives, updates PWM**
   - Parse UART frame
   - Convert voltages to PWM duty cycles (via modulation matrix)
   - Update CCU8 timers for three-phase PWM

3. **XMC reads encoder each PWM cycle**
   - Extract rotor angle from P1.1 capture
   - Prepare feedback frame

4. **XMC sends PWM feedback** via UART
   - MATLAB receives PWM state
   - Can verify correct conversion from voltages
   - Logs for later analysis

5. **MATLAB analyzes and adjusts** next command
   - Check if motor behaves correctly
   - Adjust V_d/V_q for next cycle
   - Close the loop from PC

---

## Why This Approach?

**Strengths:**
- ✅ Leverage existing validated MATLAB FOC code
- ✅ Simple debugging: See exactly what voltages MATLAB commanded vs what XMC executed
- ✅ Flexible: Can adjust algorithm parameters in MATLAB without recompiling XMC
- ✅ Non-invasive: No need to implement full FOC in XMC firmware yet

**Trade-offs:**
- ⚠️ UART bandwidth limited (analyzed in later 14_04 document)
- ⚠️ Communication latency adds delay to control loop
- ⚠️ Motor cannot run "standalone" — always depends on PC

---

## Implementation Plan

### Phase 1: MATLAB UART Sender
- Open serial port to XMC
- Send fixed voltage command (V_d=10V, V_q=0V) repeatedly
- Monitor RTT Viewer to confirm XMC receives

### Phase 2: XMC UART Receiver & PWM Update
- Configure UART interrupt for incoming frames
- Parse voltage command
- Recalculate PWM duty cycles using SVM (Space Vector Modulation)
- Update CCU8 timers

### Phase 3: Encoder Integration
- Read encoder angle from CAPTURE_0 (P1.1)
- Package into feedback frame

### Phase 4: XMC UART Sender (Feedback)
- Send PWM state back to MATLAB via UART

### Phase 5: MATLAB Receiver & Logging
- Receive XMC PWM feedback
- Plot real-time traces
- Verify PWM matches expected voltage command

### Phase 6: Closed-Loop Test
- Enable PI controller in MATLAB
- Send dynamic V_d/V_q reference commands
- Observe motor response via encoder angle
- Tune FOC gains

---

## Next Steps

**Immediate:**
1. Test UART bidirectional communication (MATLAB ↔ XMC)
2. Verify frame parsing on XMC side
3. Confirm PWM update latency acceptable

**Later (14_04+):**
4. Analyze actual UART bottleneck impact (bandwidth, latency, aliasing)
5. Decide: Continue with this approach, or move model entirely to XMC?

---

## Files to Create

- `MATLAB/PMSM_UART_Sender.m` — Send voltage commands to XMC
- `MATLAB/PMSM_UART_Receiver.m` — Receive PWM feedback from XMC
- `XMC_Project/uart_handler.c` — Parse incoming commands, send responses
- `XMC_Project/motor_control.c` — FOC algorithm placeholder (will call MATLAB-style voltage commands)

---

## Status

**Planning:** ✅ Complete  
**Architecture:** ✅ Proposed  
**Implementation:** ⏳ Ready to begin Phase 1 (MATLAB UART sender)

---

## References

- Session 12_04_2026: CCU4 capture configuration & motor phase identification
- Session 11_04_2026: Commissioning strategy (encoder → phase identification → PWM → commutation)
- MATLAB FOC Model: `PMSM_FOC_Simulation.slx` (validated in earlier sessions)
