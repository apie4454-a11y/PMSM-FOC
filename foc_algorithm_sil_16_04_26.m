function [vd_ref, vq_ref, id_out, iq_out, te_out, speed_error] = ...
    foc_algorithm_sil_16_04_26(id_motor, iq_motor, motor_rpm, omega_ele, speed_ref, max_flux)
%
% FOC ALGORITHM FOR SIL (Software-in-Loop) Testing
% Matches Simulink Model Exactly (16-04-2026)
%
% INPUTS (6):
%   id_motor   - d-axis current feedback [A]
%   iq_motor   - q-axis current feedback [A]
%   motor_rpm  - Mechanical speed feedback [RPM]
%   omega_ele  - Electrical rotor angle [rad]
%   speed_ref  - Speed reference [RPM]
%   max_flux   - Flux linkage [Wb] (from motor model)
%
% OUTPUTS (6):
%   vd_ref, vq_ref           - Voltage references [V] (to PWM block)
%   id_out, iq_out           - d-q current feedback [A] (debug)
%   te_out                   - Torque reference [Nm] (debug)
%   speed_error              - Speed error [RPM] (debug)
%

% ========== MOTOR PARAMETERS ==========
persistent motor_const;
if isempty(motor_const)
    motor_const.R = 8.4;              % d-q resistance [Ohm]
    motor_const.L = 0.003;            % d-q inductance [H]
    motor_const.pp = 11;              % Pole pairs
end

% ========== SPEED PI CONTROLLER STATE ==========
persistent speed_pi;
persistent speed_counter;
persistent iq_ref_prev;
persistent te_ref_prev;

if isempty(speed_pi)
    % Gains from Motor_Parameters.m (CORRECTED 18-04-2026)
    % speed.Kp_speed = motor.J * speed.omega_bw = 3.8e-6 * 1256.6 = 0.004775
    % speed.Ki_speed = motor.B * speed.omega_bw = 4.5e-5 * 1256.6 = 0.056549
    speed_pi.Kp = 0.004775;      % CORRECTED from 0.0004732 (10x gain fix)
    speed_pi.Ki = 0.056549; 
    speed_pi.Ts = 0.0005;        % 1/2000 Hz
    speed_pi.i_state = 0;
    speed_pi.sat_low = -0.05;
    speed_pi.sat_high = 0.1;
    speed_counter = 0;
    iq_ref_prev = 0;
    te_ref_prev = 0;
end

% Increment counter
speed_counter = speed_counter + 1;

% ========== d-AXIS PI CONTROLLER STATE ==========
persistent id_pi;
if isempty(id_pi)
    % Gains from Motor_Parameters.m
    % current.Kp_id = motor.L * current.omega_bw = 0.003 * 12566.4 = 37.699
    % current.Ki_id = motor.R * current.omega_bw = 8.4 * 12566.4 = 105557.76
    id_pi.Kp = 37.699;
    id_pi.Ki = 105560;
    id_pi.Ts = 5e-5;             % 1/20000 Hz (20 kHz)
    id_pi.i_state = 0;
    id_pi.sat_low = -29.5;
    id_pi.sat_high = 29.5;
end

% ========== q-AXIS PI CONTROLLER STATE ==========
persistent iq_pi;
if isempty(iq_pi)
    % Gains from Motor_Parameters.m (same as d-axis for PMSM)
    iq_pi.Kp = 37.699;
    iq_pi.Ki = 105560;
    iq_pi.Ts = 5e-5;
    iq_pi.i_state = 0;
    iq_pi.sat_low = -29.5;
    iq_pi.sat_high = 29.5;
end

% ========== STEP 1: SPEED CONTROL LOOP (Decimated to 2 kHz) ==========
% Only execute speed PI every 10 calls (20 kHz / 10 = 2 kHz)
if speed_counter >= 10
    speed_counter = 0;
    
    % Compute speed error [RPM]
    rpm_error = speed_ref - motor_rpm;

    % Convert to rad/s
    rpm_error_rad_s = rpm_error * (2*pi/60);

    % Speed PI controller (Forward Euler)
    p_speed = speed_pi.Kp * rpm_error_rad_s;
    speed_pi.i_state = speed_pi.i_state + speed_pi.Ki * speed_pi.Ts * rpm_error_rad_s;

    % Anti-windup: clamp integrator
    speed_pi.i_state = max(min(speed_pi.i_state, speed_pi.sat_high), speed_pi.sat_low);

    % Speed PI output (torque reference)
    te_ref = p_speed + speed_pi.i_state;
    te_ref = max(min(te_ref, speed_pi.sat_high), speed_pi.sat_low);
    te_ref_prev = te_ref;  % Store for next 9 cycles

    % ========== STEP 2: CALCULATE iq_ref FROM Te_ref ==========
    % Te = 1.5 * pp * max_flux * iq  =>  iq_ref = Te_ref / (1.5 * pp * max_flux)
    iq_ref = te_ref / (1.5 * motor_const.pp * max_flux);
    iq_ref_prev = iq_ref;  % Store for next 9 cycles
else
    % Hold iq_ref and te_ref from previous 2 kHz update
    iq_ref = iq_ref_prev;
    te_ref = te_ref_prev;
    rpm_error = speed_ref - motor_rpm;  % For debug output only
end

id_ref = 0;  % MTPA control: keep id = 0

% ========== STEP 3: d-AXIS CURRENT CONTROL LOOP ==========
% Error
id_error = id_ref - id_motor;

% PI controller
p_id = id_pi.Kp * id_error;
id_pi.i_state = id_pi.i_state + id_pi.Ki * id_pi.Ts * id_error;
id_pi.i_state = max(min(id_pi.i_state, id_pi.sat_high), id_pi.sat_low);

% PI output (saturated)
vd_star = p_id + id_pi.i_state;
vd_star = max(min(vd_star, id_pi.sat_high), id_pi.sat_low);

% d-axis Decoupling: subtract cross-coupling
% vd_decouple = omega_ele * L * iq_motor
vd_decouple = omega_ele * motor_const.L * iq_motor;

% Final vd_ref (minus decoupling!)
vd_ref = vd_star - vd_decouple;
vd_ref = max(min(vd_ref, id_pi.sat_high), id_pi.sat_low);

% ========== STEP 4: q-AXIS CURRENT CONTROL LOOP ==========
% Error
iq_error = iq_ref - iq_motor;

% PI controller
p_iq = iq_pi.Kp * iq_error;
iq_pi.i_state = iq_pi.i_state + iq_pi.Ki * iq_pi.Ts * iq_error;
iq_pi.i_state = max(min(iq_pi.i_state, iq_pi.sat_high), iq_pi.sat_low);

% PI output (saturated)
vq_star = p_iq + iq_pi.i_state;
vq_star = max(min(vq_star, iq_pi.sat_high), iq_pi.sat_low);

% q-axis Decoupling: add two terms
% Term 1: omega_ele * L * id_motor (coupling from d-axis inductance)
% Term 2: omega_ele * max_flux (back-EMF)
vq_decouple = omega_ele * motor_const.L * id_motor + omega_ele * max_flux;

% Final vq_ref (plus decoupling!)
vq_ref = vq_star + vq_decouple;
vq_ref = max(min(vq_ref, iq_pi.sat_high), iq_pi.sat_low);

% ========== DEBUG OUTPUTS ==========
id_out = id_motor;
iq_out = iq_motor;
te_out = te_ref;
speed_error = rpm_error;

end
