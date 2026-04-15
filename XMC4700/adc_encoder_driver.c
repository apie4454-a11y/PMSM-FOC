#include <stdint.h>
#include "xmc4700.h"

// ============================================================================
// XMC4700 ADC Configuration for PMSM FOC Current Sensing
// ============================================================================
// Synchronized with 20 kHz PWM for noise-free current measurements
// ============================================================================

// ---------------------------------------------------------------------------
// ADC Channel Assignments (Schematic-dependent)
// ---------------------------------------------------------------------------

#define ADC_GROUP       VADC_G0           // Use group 0
#define ADC_CHANNEL_IA  6                 // Phase A current: P14.6
#define ADC_CHANNEL_IB  7                 // Phase B current: P14.7
#define ADC_CHANNEL_VBUS 0                // Bus voltage (optional): P14.0

// Current sense amplifier specs
#define SHUNT_RESISTANCE  0.1f            // 0.1 Ω shunt resistor [Ω]
#define CURRENT_SEN_GAIN  20.0f           // Op-amp gain [V/A]
#define ADC_VREF          3.3f            // ADC reference voltage [V]
#define ADC_BITS          12              // 12-bit ADC resolution
#define ADC_COUNTS_MAX    4095            // 2^12 - 1

// Conversion factor: ADC counts → Amps
// I_amps = (ADC_counts / ADC_COUNTS_MAX) * ADC_VREF / (SHUNT_RESISTANCE * CURRENT_SEN_GAIN)
#define ADC_SCALE_IA  (ADC_VREF / (SHUNT_RESISTANCE * CURRENT_SEN_GAIN * ADC_COUNTS_MAX))
#define ADC_SCALE_IB  (ADC_VREF / (SHUNT_RESISTANCE * CURRENT_SEN_GAIN * ADC_COUNTS_MAX))

// ---------------------------------------------------------------------------
// ADC Sampling Configuration
// ---------------------------------------------------------------------------
// Goal: Sample at PWM edges (20 kHz) for minimal ripple noise
// Strategy: Trigger ADC from PWM CCU4 output (PWM sync)

void ADC_Initialize(void) {
    // Initialize VADC global parameters
    VADC->GLOBCFG = 0x00000100;  // Enable VADC, divider = 1 (120 MHz / 1 = 120 MHz clock)
    
    // Configure channel conversions (continuous, triggered by PWM)
    VADC_G0->CHCTR[ADC_CHANNEL_IA] = 0x00000000;  // Channel IA in basic mode
    VADC_G0->CHCTR[ADC_CHANNEL_IB] = 0x00000000;  // Channel IB in basic mode
    
    // Set sample time: SAMPLE_TIME = 4 clock cycles (minimum, ~33 ns)
    VADC_G0->ICLASS[0] = 0x00000003;  // 4-cycle sample time
    
    // Enable PWM synchronization (CCU4 trigger)
    // External trigger from CCU40 output (PWM carrier match)
    VADC_G0->QMCTRL |= 0x00000002;  // Enable queue mode
    
    // Configure queue entry for channel IA
    VADC_G0->QMR0 = ((ADC_CHANNEL_IA << 0) | 0x00000000);  // Queue IA
    VADC_G0->QMR0 |= 0x01000000;  // Enable queue entry, generate trigger
    
    // Configure queue entry for channel IB
    VADC_G0->QMR1 = ((ADC_CHANNEL_IB << 0) | 0x00000000);  // Queue IB
    VADC_G0->QMR1 |= 0x01000000;  // Enable queue entry
    
    // Result register configuration (capture results automatically)
    VADC_G0->RCR[0] = 0x00000000;  // Use FIFO (result buffer 0)
    VADC_G0->RES[0] = 0x00000000;  // Clear result buffer
    VADC_G0->RES[1] = 0x00000000;
}

// ---------------------------------------------------------------------------
// Runtime ADC Readout
// ---------------------------------------------------------------------------

typedef struct {
    float i_a;           // Phase A current [A]
    float i_b;           // Phase B current [A]
    uint16_t raw_ia;     // Raw ADC counts (debug)
    uint16_t raw_ib;     // Raw ADC counts (debug)
} ADC_Currents;

static ADC_Currents adc_result = {0};

// Low-pass filter for noise rejection (optional)
#define ADC_FILTER_ALPHA 0.8f  // Filter coefficient: 0 = full history, 1 = instantaneous

float ADC_LowPassFilter(float new_sample, float prev_sample) {
    return ADC_FILTER_ALPHA * new_sample + (1.0f - ADC_FILTER_ALPHA) * prev_sample;
}

void ADC_ReadCurrents(void) {
    // Read result registers (already populated by PWM-triggered conversion)
    // Results are in VADC_G0->RES[0] and VADC_G0->RES[1]
    
    uint16_t raw_ia = (VADC_G0->RES[0] >> 0) & 0x0FFF;  // 12-bit result
    uint16_t raw_ib = (VADC_G0->RES[1] >> 0) & 0x0FFF;
    
    // Convert to Amperes
    float i_a_raw = (float)raw_ia * ADC_SCALE_IA;
    float i_b_raw = (float)raw_ib * ADC_SCALE_IB;
    
    // Apply low-pass filter (optional)
    adc_result.i_a = ADC_LowPassFilter(i_a_raw, adc_result.i_a);
    adc_result.i_b = ADC_LowPassFilter(i_b_raw, adc_result.i_b);
    
    // Store raw for debugging
    adc_result.raw_ia = raw_ia;
    adc_result.raw_ib = raw_ib;
}

// Function to call from 20 kHz PWM ISR
float ADC_GetPhaseACurrent(void) {
    return adc_result.i_a;
}

float ADC_GetPhaseBCurrent(void) {
    return adc_result.i_b;
}

// ---------------------------------------------------------------------------
// PWM-to-ADC Synchronization (Middleware Layer)
// ---------------------------------------------------------------------------
// Goal: Trigger ADC exactly when PWM counter matches period
//       → Sample currents at PWM reload (consistent point in cycle)

void PWM_ADC_Sync_Setup(void) {
    // Enable PWM compare match trigger to ADC
    // Configuration depends on XMC4700 variant and DAVE app
    
    // Example (pseudo-register):
    // CCU40->GCTRL |= (1 << 16);  // Enable trigger output
    // VADC->GLOBEVNP |= (1 << 0);  // Connect CCU4 event to VADC trigger
}

// ---------------------------------------------------------------------------
// Encoder Integration (Electrical Angle Measurement)
// ---------------------------------------------------------------------------
// Use CCU4 slice 3 as quadrature encoder interface (QEI)

#define ENCODER_COUNTS_PER_REV 4096  // AS5048A: 14-bit = 16384 counts/rev (half for diffs)
#define MOTOR_POLE_PAIRS       11     // GM3506 pole pairs

// Convert encoder position to electrical angle
float Encoder_GetElectricalAngle(void) {
    // Read CCU4 position counter
    uint16_t encoder_pos = (CCU40_CC43->TIMER) & 0xFFFF;  // Slice 3, timer value
    
    // Mechanical angle: position → revolutions → radians
    float mech_angle = (float)encoder_pos / ENCODER_COUNTS_PER_REV * 2.0f * 3.14159f;
    
    // Electrical angle: mechanical × pole pairs
    float elec_angle = mech_angle * MOTOR_POLE_PAIRS;
    
    // Wrap to [0, 2π)
    while (elec_angle >= 2.0f * 3.14159f) elec_angle -= 2.0f * 3.14159f;
    while (elec_angle < 0.0f) elec_angle += 2.0f * 3.14159f;
    
    return elec_angle;
}

// Derive speed from encoder ticks
float Encoder_GetElectricalSpeed(void) {
    static uint16_t encoder_pos_prev = 0;
    uint16_t encoder_pos_curr = (CCU40_CC43->TIMER) & 0xFFFF;
    
    // Difference (accounting for wraparound)
    int16_t encoder_delta = (int16_t)(encoder_pos_curr - encoder_pos_prev);
    encoder_pos_prev = encoder_pos_curr;
    
    // Convert to rad/s
    // delta_counts → delta_revolutions → delta_angle
    float delta_mech_rad = (float)encoder_delta / ENCODER_COUNTS_PER_REV * 2.0f * 3.14159f;
    float delta_elec_rad = delta_mech_rad * MOTOR_POLE_PAIRS;
    
    // Speed = delta_angle / dt
    // Called at 20 kHz, so dt = 50 µs
    float omega_e = delta_elec_rad / 50e-6f;  // rad/s (electrical)
    
    return omega_e;
}

// Convert electrical speed to RPM
float Encoder_GetSpeedRPM(void) {
    float omega_e = Encoder_GetElectricalSpeed();
    
    // ω [rad/s] → RPM
    // 1 rev = 2π rad, 60 sec/min
    float rpm = (omega_e * 60.0f) / (2.0f * 3.14159f * MOTOR_POLE_PAIRS);
    
    return rpm;
}

// ---------------------------------------------------------------------------
// Timer Interrupt: 20 kHz PWM ISR (Platform-Specific XMC4700)
// ---------------------------------------------------------------------------

// Declare FOC update function (from foc_controller.c)
extern void FOC_CurrentControl_20kHz(float i_a, float i_b, float theta_e);
extern void FOC_SpeedControl_2kHz(float speed_measured);

void CCU4_ISR_20kHz(void) {
    // Read current measurements
    ADC_ReadCurrents();
    float i_a = ADC_GetPhaseACurrent();
    float i_b = ADC_GetPhaseBCurrent();
    
    // Read position/speed feedback
    float theta_e = Encoder_GetElectricalAngle();
    
    // ** 20 kHz Current Loop **
    FOC_CurrentControl_20kHz(i_a, i_b, theta_e);
    
    // ** 2 kHz Speed Loop (every 10th cycle) **
    static uint8_t loop_counter = 0;
    if (++loop_counter >= 10) {
        loop_counter = 0;
        
        float speed_rpm = Encoder_GetSpeedRPM();
        FOC_SpeedControl_2kHz(speed_rpm);
    }
    
    // Telemetry: Optionally log for debugging
    // if (telemetry_enabled) { log_foc_state(...); }
    
    // Clear interrupt flag
    CCU40->GCTRL &= ~0x01;  // (Platform-specific)
}

// ---------------------------------------------------------------------------
// End of File
// ---------------------------------------------------------------------------
