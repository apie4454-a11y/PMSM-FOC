%% GM3506 Motor Parameters - Inverted Pendulum Project
% This file defines all motor parameters used across simulation phases.
% Source: User measurements for iPower GM3506 Gimbal Motor.
close all; clc;

%% ========== MOTOR ELECTRICAL PARAMETERS ==========
motor.name = 'iPower GM3506 Gimbal Motor';

% Phase resistance [Ohm]
motor.R_phase = 5.6;               % Per-phase (datasheet)
motor.R = 8.4;                     % d-q equivalent with 2/3 amplitude-invariant Clarke transform (3/2 * R_phase)

% Phase inductance [H]
motor.L_phase = 0.002;             % Per-phase inductance (2 mH)
motor.L = 0.003;                   % d-q equivalent with 3/2 scaling (1.5 * L_phase)

% KV Rating and Back EMF Constant
motor.Kv = 141.4;                  % Derived from 2262 RPM / 16V (datasheet average, no-load)

% Relationship: Ke [V·s/rad] = 1 / (Kv * 2π/60)
% For PMSM/BLDC, we calculate Ke per pole-pair for the dq model
motor.poles = 22;                  % 22P configuration
motor.pp = motor.poles / 2;        % 11 Pole Pairs

% Ke_total is the mechanical Back-EMF constant
motor.Ke_total = 1 / (motor.Kv * (2*pi/60)); 

% Flux Linkage [Wb] (Used in dq equations: vq = ... + omega_e * lambda_f)
motor.lambda_f = motor.Ke_total / motor.pp; 

% Torque constant [N·m/A]
% Relationship for PMSM: Kt = 1.5 * pp * lambda_f
motor.Kt = 1.5 * motor.pp * motor.lambda_f;

% Rated voltage and current
motor.V_rated = 12;                % Typical working voltage (3S)
motor.I_rated = 1.0;               % Rated load current [A]

%% ========== MOTOR MECHANICAL PARAMETERS ==========
% Two sets of parameters for comparison and validation:
% SET 1 (LEGACY): Original estimates [2.2e-5, 1e-6]
% SET 2 (CALCULATED): Derived from iFligh specs via power analysis [3.8e-6, 4.5e-5]

% --- LEGACY PARAMETERS (Session 09-04 Start) ---
motor.J_legacy = 2.2e-5;           % Legacy: "User measured" inertia (overestimated)
motor.B_legacy = 1e-6;             % Legacy: "User measured" damping (greatly underestimated)
% Legacy Ki settling time: 28 seconds (slow)
% Legacy Ki×20 settling time: 7 seconds (empirical compensation for wrong J,B)

% --- CALCULATED PARAMETERS (Session 09-04 Physics-Based) ---
% Derived from iFligh GM3506 datasheet specifications:
%   - No-load power analysis: 2.72 W input @ 16V, 0.17A
%   - Rotor geometry: OD ~36 mm, mass ~22 g (30% of 72g total)
%   - Angular velocity: 2262 RPM = 236.8 rad/s
%   - Mechanical loss: 2.56 W, B·ω² dominates damping
%
motor.J_calc = 3.8e-6;             % Calculated: J = 0.5×m×(r_o² + r_i²) = 3.8e-6
motor.B_calc = 4.5e-5;             % Calculated: B from P_mech = B·ω² + T_f·ω = 4.5e-5
% Calculated Ki settling time: 7 seconds (no ×20 multiplier needed!)

% --- ACTIVE SET (Switch here to test) ---
% Option A: Use calculated values (physics-based, validated)
motor.J = motor.J_calc;            % 3.8e-6 [kg·m²]
motor.B = motor.B_calc;            % 4.5e-5 [N·m·s/rad]

% Option B: Use legacy values (original estimates)
% motor.J = motor.J_legacy;          
% motor.B = motor.B_legacy;

% Coulomb friction torque [N·m] (Estimated starting friction)
motor.Tf_coulomb = 0.001;          

%% ========== INVERTER & PWM PARAMETERS ==========
% PWM switching frequency [Hz]
inverter.fsw = 20e3;               % 20 kHz switching frequency
inverter.Ts = 1 / inverter.fsw;    % Control sampling time

% DC-link voltage [V]
inverter.Vdc = 59;                 % SVPWM minimum for 2262 RPM @ 1A MTPA with 15% transient margin

%% ========== CONTROL LOOP PARAMETERS ==========
% Target Bandwidth for Current Loops [Hz]
current.f_bw_current = 2000; 
current.omega_bw = 2 * pi * current.f_bw_current;

% PI Gains for Current (Id/Iq) Controller using Pole-Zero Cancellation
% Based on: (Kp*s + Ki)/s * 1/(L*s + R)
% Note: Lq = Ld, so both axes use identical gains
current.Kp_id = motor.L * current.omega_bw;
current.Ki_id = motor.R * current.omega_bw;
current.Kp_iq = motor.L * current.omega_bw;
current.Ki_iq = motor.R * current.omega_bw;

% Target Bandwidth for Speed Loop [Hz]
speed.f_bw_speed = 200;
speed.omega_bw = 2 * pi * speed.f_bw_speed;

% PI Gains for Speed Controller using Pole-Zero Cancellation
% Based on: (Kp*s + Ki)/s * 1/(J*s + B)
%
% CRITICAL DISCOVERY (09-04-2026):
% The legacy J and B values (2.2e-5, 1e-6) were severely mismatched to reality.
% This manifested as: "need Ki×20 to get 7 second settling"
% 
% After calculating from iFligh specs via no-load power analysis:
%   Legacy B = 1e-6 was 45× too LOW (actual ≈ 4.5e-5)
%   Legacy J = 2.2e-5 was 5.8× too HIGH (actual ≈ 3.8e-6)
%
% Result: Ki formula (B × ω) now naturally gives 7-second settling.
% The ×20 multiplier was compensation for wrong parameters, not a tuning feature.
%
% Compare:
%   Legacy Ki = 1e-6 × 1256.6 = 0.00126 (needs ×20 = 0.0252)
%   Calculated Ki = 4.5e-5 × 1256.6 = 0.0565 (no multiplier needed!)
%
speed.Kp_speed = motor.J * speed.omega_bw;
speed.Ki_speed = motor.B * speed.omega_bw;

%% ========== SIMULATION PARAMETERS ==========
sim.t_end = 2.0;                   % Total simulation time [seconds]
sim.t_step = 5e-6;                 % Solver step (must be < Ts, reverted to 5e-6 for discrete PI stability)

%% ========== SAVE TO WORKSPACE & PRINT ==========
fprintf('\n========== %s LOADED ==========\n', motor.name);
fprintf('Resistance (R): %.2f Ω | Inductance (L): %.2f mH\n', motor.R, motor.L*1e3);
fprintf('Kv: %d RPM/V | Kt: %.4f Nm/A\n', motor.Kv, motor.Kt);
fprintf('Pole Pairs: %d | Flux Linkage: %.6f Wb\n', motor.pp, motor.lambda_f);
fprintf('---------------------------------------------\n');

% Show mechanical parameters with both sets
fprintf('Mechanical Parameters (ACTIVE):\n');
fprintf('  J = %.3e kg·m² | B = %.3e N·m·s/rad\n', motor.J, motor.B);
fprintf('  (Legacy: J=%.3e, B=%.3e for reference)\n', motor.J_legacy, motor.B_legacy);

fprintf('---------------------------------------------\n');
fprintf('Current Loop Bandwidth: %d Hz\n', current.f_bw_current);
fprintf('PI Gains (Id) -> Kp_id: %.4f | Ki_id: %.1f\n', current.Kp_id, current.Ki_id);
fprintf('PI Gains (Iq) -> Kp_iq: %.4f | Ki_iq: %.1f\n', current.Kp_iq, current.Ki_iq);
fprintf('Speed Loop Bandwidth: %d Hz\n', speed.f_bw_speed);
fprintf('PI Gains (Speed) -> Kp_speed: %.6f | Ki_speed: %.6f\n', speed.Kp_speed, speed.Ki_speed);
fprintf('  (Legacy Ki would be: %.6f, needed ×20 = %.6f)\n', motor.B_legacy * speed.omega_bw, motor.B_legacy * speed.omega_bw * 20);
fprintf('=============================================\n\n');