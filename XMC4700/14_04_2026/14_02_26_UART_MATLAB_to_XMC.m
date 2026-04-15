% Setup Serial Port
port = "COM3"; % Confirm via Device Manager
baud = 115200;
device = serialport(port, baud);
configureTerminator(device, "LF");

% 1. Prepare Data
v_send = [0.850, -0.123, 0.444];
msg = sprintf("%.3f,%.3f,%.3f", v_send(1), v_send(2), v_send(3));

% 2. Send Data
fprintf('Sending to XMC: %s\n', msg);
writeline(device, msg);

% 3. Wait for XMC to process (Interrupts + Parsing take time)
pause(0.5);

% 4. Read Response
if device.NumBytesAvailable > 0
    response = readline(device);
    fprintf('XMC Response: %s\n', response);
else
    disp('No response from XMC.');
end

clear device;
