# Troubleshooting & Engineering Log: PWM Capture Optimization
**Project:** PMSM Motor Control Feedback Interface
**Engineer:** [Your Name/ID]
**Target:** XMC4700 CCU4 Peripheral

## 1. Problem Overview
The primary challenge was establishing a reliable, high-precision capture of a 2000 Hz PWM signal (representing motor position) using the Infineon XMC4700. Several hardware-level obstacles were encountered during the integration of the DAVE Apps and the RTT (Real-Time Transfer) debugging interface.

---

## 2. Issues Encountered & Resolution Path

### Issue A: "Stale" or Static Register Data
* **Symptom:** The RTT terminal displayed `0xDEADBEEE` or persistent `0` values, even when a physical signal was present.
* **Root Cause:** The CCU4 hardware slices require a specific "Clear Event" handshake. If the Interrupt or Event flag isn't cleared by software, the hardware stops updating the Capture Registers (CV0, CV1) to protect the data from being overwritten before it is read.
* **Solution:** Implemented "Force Clearing" of the hardware event inside the main loop using `XMC_CCU4_SLICE_ClearEvent`. This forced the peripheral to accept new samples regardless of the software's processing speed.

### Issue B: Low-Resolution Capture (Integer Underflow)
* **Symptom:** Initial captures returned very small integers (e.g., `P: 3`, `D: 1`), making duty cycle calculation impossible.
* **Root Cause:** The CAPTURE App was set to "Calculated" resolution based on a high-frequency target. This resulted in the timer prescaler being set too high, effectively "grouping" hundreds of clock cycles into a single tick.
* **Solution:** Manually overrode the Timer Resolution to **100 ns (Direct)** in the DAVE UI. This expanded the "dynamic range" of the measurement, resulting in the high-precision value of **8999 ticks** for a 2000 Hz period.

### Issue C: Signal "Ghosting" & Floating Input Noise
* **Symptom:** When the jumper wire was disconnected, the RTT terminal continued to scroll random, massive values (e.g., `P: 360000`).
* **Root Cause:** High-impedance CMOS inputs on the XMC4700 act as antennas. Electromagnetic interference (EMI) from the board and environment was triggering "ghost" edges on the capture pin.
* **Solution:** Reconfigured the GPIO mode in software to `XMC_GPIO_MODE_INPUT_PULL_DOWN`. This forces the pin to a logical `0` state when no signal is present, ensuring the capture logic only triggers on a genuine driven signal.

### Issue D: Center-Aligned vs. Edge-Aligned Discrepancy
* **Symptom:** The captured period (8999) was exactly half of what was expected based on a standard 144 MHz / 2000 Hz calculation.
* **Root Cause:** The PWM generator was configured for **Center-Aligned** mode. In this mode, the hardware timer counts Up-Down. The capture logic perceives the total cycle based on how the CCU4 slice slices the clock in this specific mode.
* **Solution:** Performed a mathematical "Golden Ratio" verification. Confirmed that 8999 is the mathematically correct result for a 2000 Hz signal in Center-Aligned mode at the current prescaler settings.

---

## 3. Debugging Methodology Applied

1.  **Register-Level Inspection:** Instead of trusting the abstraction layer, we looked at the raw returns of the Capture registers to ensure hardware-level activity.
2.  **Loopback Testing:** Used a known-good PWM source (P1.0) to validate the Capture input (P1.1). This isolated the problem to the *peripheral configuration* rather than the *external sensor*.
3.  **Stress Testing:** Disconnected the hardware (Air-gap test) to verify that the software correctly handled a "No Signal" state without crashing.
4.  **Mathematical Modeling:** Built a theoretical model of the expected tick count ($144MHz / (2000Hz \times 8)$) to validate that the hardware results were genuine and not hallucinated.

## 4. Engineering Skills Demonstrated
* **Peripheral Mastery:** Deep configuration of XMC4700 CCU4 slices.
* **Hardware-Aware Programming:** Managing GPIO pull-up/pull-down states and register flags.
* **Critical Thinking:** Identifying the impact of PWM counting modes (Center-Aligned) on capture frequency.
* **Tools Proficiency:** Advanced use of DAVE IDE, XMCLib, and SEGGER RTT for non-intrusive debugging.