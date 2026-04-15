# Technical Transition: From External HIL to Embedded Rapid Prototyping

## 1. The Original Problem Statement (External HIL)
The initial objective was to create a Hardware-in-the-Loop (HIL) simulation where:
1. **MATLAB** calculates the modulation voltages ($V_{abc}$).
2. **XMC** receives these voltages via UART and generates PWM pulses.
3. **XMC** sends the PWM pulses back to **MATLAB** to drive a virtual inverter model.

## 2. The UART Bottleneck (The Hard Constraint)
The primary reason for pivoting is the **Data Bandwidth Limitation** of the UART interface.

### The Math of the Failure:
* **PWM Frequency:** 20,000 Hz (Cycle time = 50 µs).
* **Nyquist Sampling:** To accurately "see" a 20 kHz square wave in MATLAB, you need at least 10–20 samples per cycle.
* **Required Data Rate:** $20,000 \text{ cycles/sec} \times 10 \text{ samples/cycle} \times 3 \text{ phases} = 600,000 \text{ samples/sec}$.
* **UART Capacity:** At 115,200 baud, the maximum theoretical throughput is roughly 11,520 bytes per second.
* **Conclusion:** The UART is **~50x too slow** to stream real-time PWM data back to MATLAB. Any attempt results in massive aliasing, data loss, and "lag" that makes the inverter model unstable.

## 3. The Solution: Rapid Control Prototyping (RCP)

### Why the Initial Plan Failed
The original HIL approach (MATLAB calculates, XMC generates PWM, PWM data sent back to MATLAB) required streaming high-frequency PWM data over UART back to the PC. As shown in Section 2, UART is **~50x too slow** for this workflow—resulting in aliasing, data loss, and closed-loop instability.

### Architecture Shift: XMC Autonomous Execution
Rather than trying to send PWM data back to MATLAB, the solution is to **migrate the entire Inverter Plant Model into the XMC4700**. This eliminates the UART bottleneck entirely and enables **autonomous motor control**.

**Final Architecture:**
* **XMC4700:** Complete autonomous motor control engine.
    * Reads encoder PWM signal for rotor angle feedback
    * Executes FOC algorithm and Inverter Mathematical Model at native speed (20 kHz)
    * Updates CCU8 hardware timers for three-phase PWM generation
    * Runs full control loop at real-time without PC dependency
    
* **MATLAB:** Optional for post-test analysis and parameter optimization.
    * Not required for motor operation
    * Can analyze logged data or compare simulations post-run
    * Future: rapid prototyping of different control parameters

* **Visibility Solution:** Logic Analyzer provides real-time debugging (Section 4).
    * No need for MATLAB as monitoring tool
    * Direct measurement of PWM, timing, current probe data
    * Can observe internal state via GPIO toggle signals

## 4. Solving the "Flying Blind" Problem
Moving the model into the chip usually makes it harder to visualize what is happening. However, the use of a **Logic Analyzer** solves this:

1. **Hardware Verification:** The Logic Analyzer connects directly to the XMC PWM pins. It can sample at 24MHz or higher, easily capturing the 20 kHz pulses.
2. **Timing Analysis:** We can use spare GPIO pins to measure "Execution Time." By toggling a pin at the start and end of the math code, the Logic Analyzer shows exactly how much CPU overhead the model is using.
3. **Internal Data Recovery:** If we need to see a variable (like Phase Voltage), we can output it as a variable-duty PWM signal and let the Logic Analyzer "decode" it.

## 5. Summary of Benefits
* **Zero Latency:** The Inverter math reacts to the PWM update in the same clock cycle.
* **Real-World Jitter:** The simulation now includes the effects of the XMC's timer resolution and quantization.
* **System Stability:** The control loop is no longer dependent on the PC's operating system or the USB-Serial cable latency.