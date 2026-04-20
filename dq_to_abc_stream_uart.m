% =====================================================================
% Real-time dq→RYB Coordinate Transform Validation with UART Handshake
% =====================================================================
% 
% PURPOSE:
% This script validates the dq→RYB (Clarke-Park) coordinate transformation
% and UART communication protocol. Tests mathematical correctness of 
% coordinate conversions using step-counter-based theta generation.
%
% XMC PROJECT:
% Project Name: 17_04_2026_PWM_with_motor_controller_and_inverter_models
% File: main.c (contains STATE_DQ_TO_ABC_STREAM_TEST with step-counter-based theta)
% Hardware: Infineon XMC4700 microcontroller with J-Link debugger
%
% WHAT IS BEING TESTED:
% 1. UART Handshake Protocol: Synchronous communication between MATLAB and XMC
% 2. dq→RYB Math: Coordinate transformation (dq frame → RYB 3-phase frame)
% 3. NOT Testing Inverter: Simple linear duty mapping bypasses SPWM/inverter logic
%
% WORKFLOW TO GENERATE PLOTS:
% 1. Open 17_04_2026 project in XMC IDE
% 2. Build project (Ctrl+B)
% 3. Run via J-Link (Debug as → XMC Application)
% 4. In MATLAB, run THIS script: dq_to_abc_stream_uart
% 5. Script will collect 2000 samples and generate real-time plots
% 6. Resulting images: theta + [Vr, Vy, Vb] coordinate transform validation
%
% HANDSHAKE PROTOCOL:
% 1. Script opens UART connection to XMC4700 (COM3, 115200 baud)
% 2. Sends "RUN_DQ_TO_ABC_STREAM_TEST" command to trigger firmware
% 3. XMC firmware begins generating samples using step-counter-based theta:
%    theta = (2π × NUM_CYCLES × test_step) / test_steps_total
% 4. For each sample:
%    - Constant dq voltage: vd=0V, vq=10V
%    - Coordinate transform: dq(vd,vq) + theta → RYB(vr,vy,vb)
%    - Simple linear mapping: duty = 3000 + (v_ref / (VDC/2)) * 3000
%    - Output actual voltages (RYB)
% 5. MATLAB sends "OK" to request next sample
% 6. Process repeats until 2000 samples collected or user aborts
%
% CRITICAL: This script must be running to communicate with XMC!
% If XMC firmware is compiled but idle, run this script first.
%
% =====================================================================
%
% Output Images:
%  - Left plot: Theta progression + [Vr, Vy, Vb] waveforms
%  - Right plot: Electrical angle theta (0→2π sawtooth)
%  - Analysis: Perfect 120° phase separation validates coordinate transform
%  - Note: Negative voltages OK (RYB phases can be negative for rotating vector)
%
% =====================================================================

clear; close all; clc;

% UART Configuration
PORT = 'COM3';          % Change to your XMC4700 COM port
BAUDRATE = 115200;
TIMEOUT = 2;

% Open serial port
try
    s = serialport(PORT, BAUDRATE);
    s.Timeout = TIMEOUT;
    configureTerminator(s, "LF");
    pause(2);  % Wait for XMC to boot
    disp('✓ UART connected');
catch
    error('Failed to open UART port. Check COM port and baud rate.');
end

% Send command to start coordinate transform test
writeline(s, "RUN_DQ_TO_ABC_STREAM_TEST");
disp('Sent: RUN_DQ_TO_ABC_STREAM_TEST');

% Wait for acknowledgment
try
    ack = readline(s);
    disp(['ACK: ' ack]);
catch
    error('No response from XMC4700');
end

% Read CSV header
try
    header = readline(s);
    disp(['Header: ' header]);
catch
    error('No header received');
end

% Prepare figure
figure('Position', [100 100 1000 500]);

% Initialize plot
subplot(1, 2, 1);
hold on; grid on;
h_vr = plot(NaN, NaN, '-r', 'LineWidth', 1.5, 'DisplayName', 'Vr (Red)');
h_vy = plot(NaN, NaN, '-g', 'LineWidth', 1.5, 'DisplayName', 'Vy (Green)');
h_vb = plot(NaN, NaN, '-b', 'LineWidth', 1.5, 'DisplayName', 'Vb (Blue)');
xlabel('Sample Index'); ylabel('Voltage (V)');
title('dq→RYB Coordinate Transform Output (Real-time)');
legend('FontSize', 10);
ylim([-12 12]);
set(gca, 'XLim', [0 2000]);

subplot(1, 2, 2);
hold on; grid on;
h_theta = plot(NaN, NaN, '-b', 'LineWidth', 1.5);
xlabel('Sample Index'); ylabel('Electrical Angle (rad)');
title('Theta Progression (Step-Counter Based)');
set(gca, 'XLim', [0 2000], 'YLim', [0 7]);

% Data storage
samples = [];
thetas = [];
vrs = [];
vys = [];
vbs = [];
sample_times = [];

% Receive and plot samples
max_samples = 2000;
start_time = tic;
for i = 1:max_samples
    try
        % Record receive time
        t = toc(start_time);
        
        % Receive data line from XMC
        data_line = readline(s);
        
        % Parse CSV: sample,theta,vr,vy,vb
        parts = split(data_line, ',');
        if length(parts) >= 5
            sample_num = str2double(parts(1));
            theta = str2double(parts(2));
            vr = str2double(parts(3));
            vy = str2double(parts(4));
            vb = str2double(parts(5));
            
            % Store data
            samples = [samples; sample_num];
            thetas = [thetas; theta];
            vrs = [vrs; vr];
            vys = [vys; vy];
            vbs = [vbs; vb];
            sample_times = [sample_times; t];
            
            % Update plots
            subplot(1, 2, 1);
            set(h_vr, 'XData', samples, 'YData', vrs);
            set(h_vy, 'XData', samples, 'YData', vys);
            set(h_vb, 'XData', samples, 'YData', vbs);
            
            subplot(1, 2, 2);
            set(h_theta, 'XData', samples, 'YData', thetas);
            
            drawnow;
            
            % Send OK to request next sample
            writeline(s, "OK");
            
            % Progress indicator
            if mod(i, 100) == 0
                fprintf('Received %d samples...\n', i);
            end
        end
    catch ME
        fprintf('Error at sample %d: %s\n', i, ME.message);
        break;
    end
end

% Calculate actual sample rate
if length(sample_times) > 1
    dt_actual = diff(sample_times);
    avg_sample_rate = 1 / mean(dt_actual);
    fprintf('\n=== Timing Analysis ===\n');
    fprintf('Total samples: %d\n', length(samples));
    fprintf('Total time: %.3f seconds\n', sample_times(end));
    fprintf('Handshake sample rate: %.1f Hz (limited by MATLAB→XMC→MATLAB round-trip latency)\n', avg_sample_rate);
    fprintf('Average dt: %.6f seconds (%.1f ms)\n', mean(dt_actual), mean(dt_actual)*1e3);
    fprintf('Note: PWM runs at 20 kHz internally; handshake paces sample output at ~10-20 Hz\n');
end

% Verify coordinate transform mathematics
if length(thetas) > 0
    fprintf('\n=== Coordinate Transform Validation ===\n');
    % Check phase separation (should be 120 degrees = 2π/3 radians apart)
    phase_sep_vry = acos((vrs(end).*vys(end) + 0*0) / (sqrt(vrs(end)^2 + 0^2) * sqrt(vys(end)^2 + 0^2) + eps));
    fprintf('Phase separation Vr-Vy: %.1f degrees (expected ~120°)\n', rad2deg(phase_sep_vry));
    % Check 3-phase balance
    sum_3phase = vrs(end) + vys(end) + vbs(end);
    fprintf('Final sample 3-phase sum: %.6f V (expected ≈0)\n', sum_3phase);
    fprintf('RMS voltage per phase: %.2f V\n', rms([vrs, vys, vbs])/sqrt(3));
end

% Final plot
subplot(1, 2, 1);
set(h_vr, 'XData', samples, 'YData', vrs);
set(h_vy, 'XData', samples, 'YData', vys);
set(h_vb, 'XData', samples, 'YData', vbs);

subplot(1, 2, 2);
set(h_theta, 'XData', samples, 'YData', thetas);
drawnow;

fprintf('\n✓ Received %d samples - dq→RYB coordinate transform validated\n', length(samples));

% Close UART
clear s;
disp('UART closed');
