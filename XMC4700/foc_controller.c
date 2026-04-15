#include <stdint.h>
#include <math.h>
#include <string.h>

// ============================================================================
// XMC4700 PMSM FOC — PWM + Current Control Integration
// ============================================================================
// Implements:
//  - 20 kHz PWM update ISR (current loop)
//  - 2 kHz speed loop (cascaded)
//  - Clarke/Park transforms
//  - dq PI current control
//  - Voltage circle saturation
//  - Anti-windup for PI integrators
// ============================================================================

// ---------------------------------------------------------------------------
// Motor Parameters (from motor_constants.md, locked)
// ---------------------------------------------------------------------------
#define MOTOR_R_DQ          8.4f     // d-q equivalent resistance [Ω]
#define MOTOR_L_DQ          0.003f   // d-q equivalent inductance [H]
#define MOTOR_FLUX_LINKAGE  0.00611f // λm [Wb]
#define MOTOR_P             11        // Pole pairs
#define MOTOR_I_RATED       1.0f     // Rated current [A]
#define MOTOR_TORQUE_CONST  (MOTOR_FLUX_LINKAGE * (3.0f/2.0f) * MOTOR_P)  // dq-frame

// ---------------------------------------------------------------------------
// Control Tuning (Physics-based gains from motor_constants.md)
// ---------------------------------------------------------------------------
#define BW_CURRENT   2000.0f  // Current loop bandwidth [Hz]
#define BW_SPEED     200.0f   // Speed loop bandwidth [Hz]

// Current PI Gains (L_dq = 3mH, R_dq = 8.4Ω, f_bw = 2000 Hz)
#define KP_ID        37.7f    // d-axis current proportional
#define KI_ID        105558.0f // d-axis current integral
#define KP_IQ        37.7f    // q-axis current proportional
#define KI_IQ        105558.0f // q-axis current integral

// Speed PI Gains (J=3.8e-6, B=4.5e-5, f_bw = 200 Hz)
#define KP_SPEED     0.4763f  // Speed proportional
#define KI_SPEED     0.0565f  // Speed integral (physics-derived)

// ---------------------------------------------------------------------------
// Hardware Constraints
// ---------------------------------------------------------------------------
#define PWM_FREQUENCY    20000   // 20 kHz
#define PWM_PERIOD_US    50      // 50 µs
#define PWM_PERIOD_COUNTS 6000   // 120 MHz / 20 kHz
#define VMAX             29.5f   // Max voltage = 59V / 2 [V] (sine-triangle)
#define DEAD_TIME_NS     100     // Shoot-through protection [ns]
#define DEAD_TIME_COUNTS 12      // 100 ns / 8.33 ns per count @ 120 MHz

// ---------------------------------------------------------------------------
// Control Loop State Structures
// ---------------------------------------------------------------------------

typedef struct {
    float d;     // d-axis current [A]
    float q;     // q-axis current [A]
} DQ_Currents;

typedef struct {
    float d;     // d-axis voltage [V]
    float q;     // q-axis voltage [V]
} DQ_Voltages;

typedef struct {
    float alpha; // α-axis component
    float beta;  // β-axis component
} AlphaBeta;

typedef struct {
    float kp;            // Proportional gain
    float ki;            // Integral gain
    float integral;      // Integrator state [V·s]
    float error_prev;    // For derivative filtering (optional)
    float output_limit;  // Anti-windup saturation [V]
} PI_Controller;

typedef struct {
    PI_Controller id;    // d-axis current controller
    PI_Controller iq;    // q-axis current controller
} CurrentControllers;

typedef struct {
    PI_Controller speed; // Speed error to torque reference
    float integral;      // Integrator state [A·s]
    float output_limit;  // Anti-windup: max iq_ref [A]
} SpeedController;

typedef struct {
    float theta_e;       // Electrical angle [rad]
    float omega_e;       // Electrical speed [rad/s]
    float speed_ref;     // Speed reference [RPM]
    float speed_meas;    // Measured speed [RPM]
    float id_ref;        // d-axis current reference [A] (=0 for MTPA)
    float iq_ref;        // q-axis current reference [A]
} MotorState;

typedef struct {
    CurrentControllers current;
    SpeedController speed;
    MotorState motor;
    float fc;  // Control frequency (debugging)
} FOC_Controller;

// Global controller instance
static FOC_Controller foc;

// ---------------------------------------------------------------------------
// Utility Functions
// ---------------------------------------------------------------------------

// Math: Sine-cosine lookup (can be optimized with LUT)
static inline float sin_f(float angle) {
    return sinf(angle);
}

static inline float cos_f(float angle) {
    return cosf(angle);
}

// Clamp function
static inline float clamp(float value, float min, float max) {
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

// ---------------------------------------------------------------------------
// Clarke Transform (3-phase to 2-phase α-β)
// Amplitude-invariant, 2/3 scaling: i_alpha = (2/3)(ia - 0.5*ib - 0.5*ic)
// ---------------------------------------------------------------------------

AlphaBeta clarke_transform(float ia, float ib, float ic) {
    AlphaBeta result;
    
    // Using 2/3 scaling
    result.alpha = (2.0f / 3.0f) * (ia - 0.5f * ib - 0.5f * ic);
    result.beta = (2.0f / 3.0f) * (0.866f * ib - 0.866f * ic);  // √3/2 ≈ 0.866
    
    return result;
}

// Inverse Clarke: α-β back to 3-phase (a, b, c)
AlphaBeta inverse_clarke_transform(float ia_alpha, float ia_beta) {
    AlphaBeta result;
    
    // Approximate 3-phase reconstruction (for reference)
    result.alpha = ia_alpha;
    result.beta = ia_beta;
    
    // In practice, use direct voltage modulation on α-β directly
    return result;
}

// ---------------------------------------------------------------------------
// Park Transform: α-β (stationary) → d-q (rotating frame)
// dq = R(θ) * αβ, where R = [cos(θ), sin(θ); -sin(θ), cos(θ)]
// ---------------------------------------------------------------------------

DQ_Currents park_transform(AlphaBeta iab, float theta_e) {
    DQ_Currents result;
    
    float cos_theta = cos_f(theta_e);
    float sin_theta = sin_f(theta_e);
    
    // Magnitude-invariant transformation
    result.d = cos_theta * iab.alpha + sin_theta * iab.beta;
    result.q = -sin_theta * iab.alpha + cos_theta * iab.beta;
    
    return result;
}

// Inverse Park: d-q (rotating) → α-β (stationary)
// αβ = R(-θ) * dq
AlphaBeta inverse_park_transform(DQ_Voltages vdq, float theta_e) {
    AlphaBeta result;
    
    float cos_theta = cos_f(theta_e);
    float sin_theta = sin_f(theta_e);
    
    // Reverse transformation
    result.alpha = cos_theta * vdq.d - sin_theta * vdq.q;
    result.beta = sin_theta * vdq.d + cos_theta * vdq.q;
    
    return result;
}

// ---------------------------------------------------------------------------
// PI Controller (General Purpose)
// ---------------------------------------------------------------------------

void PI_Init(PI_Controller *pi, float kp, float ki, float output_limit) {
    pi->kp = kp;
    pi->ki = ki;
    pi->integral = 0.0f;
    pi->error_prev = 0.0f;
    pi->output_limit = output_limit;
}

float PI_Update(PI_Controller *pi, float error, float dt) {
    // Proportional term
    float p_term = pi->kp * error;
    
    // Integrate error
    pi->integral += pi->ki * error * dt;
    
    // Clamp integral (anti-windup)
    pi->integral = clamp(pi->integral, -pi->output_limit, pi->output_limit);
    
    // Total output
    float output = p_term + pi->integral;
    
    // Hard clamp output
    output = clamp(output, -pi->output_limit, pi->output_limit);
    
    return output;
}

void PI_ResetIntegral(PI_Controller *pi) {
    pi->integral = 0.0f;
}

// ---------------------------------------------------------------------------
// Sine-Triangle PWM Modulation (3-phase)
// Generates normalized duty cycles [0, 1] for U, V, W phases
// ---------------------------------------------------------------------------

typedef struct {
    float duty_u;
    float duty_v;
    float duty_w;
} PWM_Duties;

PWM_Duties sine_triangle_modulation(AlphaBeta vab, float theta_e) {
    PWM_Duties duty;
    
    // Voltage magnitude in α-β frame
    float v_mag = sqrtf(vab.alpha * vab.alpha + vab.beta * vab.beta);
    
    // Phase angle of voltage vector
    float phase_v = atan2f(vab.beta, vab.alpha);
    
    // For each phase, apply 120° offset
    for (int phase = 0; phase < 3; phase++) {
        float phase_offset = (float)phase * 2.0f * 3.14159f / 3.0f;  // 0°, 120°, 240°
        float sine_ref = sinf(phase_v + phase_offset);
        
        // Normalize by VMAX (half of Vdc)
        float modulation_index = (v_mag / VMAX) * sine_ref;
        
        // Scale to PWM [0, 1]
        float duty_normalized = 0.5f + 0.5f * modulation_index;
        
        // Clamp to [0, 1]
        duty_normalized = clamp(duty_normalized, 0.0f, 1.0f);
        
        if (phase == 0) duty.duty_u = duty_normalized;
        else if (phase == 1) duty.duty_v = duty_normalized;
        else duty.duty_w = duty_normalized;
    }
    
    return duty;
}

// ---------------------------------------------------------------------------
// Voltage Circle Saturation (Anti-Overflow)
// Ensures √(Vd² + Vq²) ≤ VMAX before sending to PWM
// ---------------------------------------------------------------------------

DQ_Voltages saturate_voltage_circle(DQ_Voltages vdq) {
    float v_mag = sqrtf(vdq.d * vdq.d + vdq.q * vdq.q);
    
    if (v_mag > VMAX) {
        // Scale back proportionally
        float scale = VMAX / v_mag;
        vdq.d *= scale;
        vdq.q *= scale;
    }
    
    return vdq;
}

// ---------------------------------------------------------------------------
// 20 kHz Current Control Loop (Core ISR)
// ---------------------------------------------------------------------------
// Call this at every PWM period (50 µs)

void FOC_CurrentControl_20kHz(
    float i_a,      // Phase A current [A] from shunt
    float i_b,      // Phase B current [A] from shunt
    float theta_e   // Electrical angle [rad] from encoder
) {
    // Step 1: Clarke Transform (3-phase → 2-phase)
    AlphaBeta i_ab = clarke_transform(i_a, i_b, 0.0f);
    // Note: i_c can be calculated as -(i_a + i_b) or measured separately
    
    // Step 2: Park Transform (stationary → rotating frame)
    DQ_Currents i_dq = park_transform(i_ab, theta_e);
    
    // Step 3: Current PI Control (dq frame)
    float dt = 1.0f / PWM_FREQUENCY;  // 50 µs
    
    float v_d = PI_Update(&foc.current.id, foc.motor.id_ref - i_dq.d, dt);
    float v_q = PI_Update(&foc.current.iq, foc.motor.iq_ref - i_dq.q, dt);
    
    // Step 4: Voltage Circle Saturation
    DQ_Voltages v_dq = {v_d, v_q};
    v_dq = saturate_voltage_circle(v_dq);
    
    // Step 5: Inverse Park (rotating → stationary)
    AlphaBeta v_ab = inverse_park_transform(v_dq, theta_e);
    
    // Step 6: Sine-Triangle PWM Modulation
    PWM_Duties pwm_duty = sine_triangle_modulation(v_ab, theta_e);
    
    // Step 7: Update PWM registers
    // (Implementation-specific: write to CCU40 compare registers)
    // PWM_UpdateDuty(pwm_duty.duty_u, pwm_duty.duty_v, pwm_duty.duty_w);
}

// ---------------------------------------------------------------------------
// 2 kHz Speed Control Loop (Cascade Outer Loop)
// ---------------------------------------------------------------------------
// Call this every 10 PWM cycles (500 µs)

void FOC_SpeedControl_2kHz(float speed_measured) {
    float dt = 0.0005f;  // 500 µs
    
    // Speed error
    float speed_error = foc.motor.speed_ref - speed_measured;
    
    // Speed PI control → torque (iq) reference
    float iq_ref_new = PI_Update(&foc.speed.speed, speed_error, dt);
    
    // Clamp to rated current
    foc.motor.iq_ref = clamp(iq_ref_new, 0.0f, MOTOR_I_RATED);
    
    // d-axis = 0 (MTPA for non-salient motor)
    foc.motor.id_ref = 0.0f;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void FOC_Initialize(float speed_ref_rpm) {
    memset(&foc, 0, sizeof(FOC_Controller));
    
    // Initialize current PI controllers (max voltage = VMAX)
    PI_Init(&foc.current.id, KP_ID, KI_ID, VMAX);
    PI_Init(&foc.current.iq, KP_IQ, KI_IQ, VMAX);
    
    // Initialize speed PI controller (max iq_ref = rated current)
    PI_Init(&foc.speed.speed, KP_SPEED, KI_SPEED, MOTOR_I_RATED);
    
    // Initial motor state
    foc.motor.speed_ref = speed_ref_rpm;
    foc.motor.id_ref = 0.0f;
    foc.motor.iq_ref = 0.0f;
    foc.motor.theta_e = 0.0f;
}

// ---------------------------------------------------------------------------
// Status / Debugging
// ---------------------------------------------------------------------------

typedef struct {
    DQ_Currents i_dq;
    DQ_Voltages v_dq;
    float v_mag;
    float output_duty_u, output_duty_v, output_duty_w;
} FOC_Telemetry;

// (Call periodically for logging)
FOC_Telemetry foc_telemetry = {0};

// ---------------------------------------------------------------------------
// Example: Main ISR Integration (Pseudo-code)
// ---------------------------------------------------------------------------

/*
void CCU4_PWM_ISR(void) {
    // Read ADC channels (externally triggered)
    static float i_a_prev = 0, i_b_prev = 0;
    float i_a = ADC_Read_ChannelA() * ADC_SCALE;  // Convert to Amperes
    float i_b = ADC_Read_ChannelB() * ADC_SCALE;
    
    // Read encoder electrical angle
    float theta_e = encoder_read_angle_electrical();
    
    // Run 20 kHz current loop
    FOC_CurrentControl_20kHz(i_a, i_b, theta_e);
    
    // Update PWM outputs
    // (Platform-specific: write to XMC4700 CCU40 shadow registers)
    
    // Every 10th cycle (2 kHz), run speed loop
    static int pwm_cycle_count = 0;
    if (++pwm_cycle_count >= 10) {
        pwm_cycle_count = 0;
        
        float speed_rpm = encoder_read_speed_rpm();
        FOC_SpeedControl_2kHz(speed_rpm);
    }
}
*/

// ---------------------------------------------------------------------------
// End of File
// ---------------------------------------------------------------------------
