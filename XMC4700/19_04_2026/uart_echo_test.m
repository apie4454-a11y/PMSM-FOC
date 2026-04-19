%% UART Handshake Protocol Test - XMC4700 Communication
% Test bidirectional handshake: send message -> receive echo -> send OK -> repeat

clear all; close all; clc;

fprintf('=== XMC4700 UART Handshake Protocol Test ===\n\n');

% Open serial port
try
    port = serialport("COM3", 115200);
    fprintf('✓ Serial port COM3 opened at 115200 baud\n');
    pause(1);  % Wait for connection to stabilize
catch ME
    fprintf('✗ Failed to open COM3: %s\n', ME.message);
    return;
end

% Test messages
test_messages = {
    "Hello from MATLAB",
    "TEST",
    "Motor control active",
    "Another message",
    "Final test"
};

fprintf('\nHandshake test - send message, receive echo, send OK:\n');
fprintf('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');

for i = 1:length(test_messages)
    msg = test_messages{i};
    
    % === STEP 1: MATLAB sends message ===
    fprintf('[%d] Sending: "%s"\n', i, msg);
    writeline(port, msg);
    pause(0.1);
    
    % === STEP 2: MATLAB receives echo from XMC ===
    if port.NumBytesAvailable > 0
        echo_response = readline(port);
        fprintf('     Received: "%s"\n', echo_response);
    else
        fprintf('     ERROR: No echo received!\n');
        continue;
    end
    
    pause(0.1);
    
    % === STEP 3: MATLAB sends OK to continue ===
    fprintf('     Sending: "OK"\n');
    writeline(port, "OK");
    pause(0.2);
    
    fprintf('\n');
end

fprintf('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');

% Clean up
clear port;
fprintf('\n✓ Handshake test complete!\n');
fprintf('  XMC should show: ready for next message (after each OK)\n');
