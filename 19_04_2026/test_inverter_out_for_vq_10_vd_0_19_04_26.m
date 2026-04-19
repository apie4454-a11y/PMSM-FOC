% Read CSV file
data = readtable('19_04_26_test_inverter_out_for_vq_10_vd_0.csv');

% Extract columns with proper prefix
theta = data.theta;
vrn = data.vn;
vyn = data.vyn;
vbn = data.vbn;

% Plot
figure;
plot(theta, [vrn, vyn, vbn], 'LineWidth', 2);
legend('vrn', 'vyn', 'vbn', 'FontSize', 12);
grid on;
xlabel('Electrical Angle (rad)');
ylabel('Voltage (V)');
title('Inverter Model Output');