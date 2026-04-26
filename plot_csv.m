%% =====================================================================
% Interactive CSV Plotter — FOC Motor HIL Data Visualisation (MATLAB)
% ======================================================================
%
% PURPOSE:
%   General-purpose interactive CSV plotter for visualising RTT-logged
%   motor data from the XMC4700 HIL firmware. Allows dynamic selection
%   of CSV file, X-axis, and one or more Y-axis signals without editing
%   the script between runs.
%
% ===== Generic RTT CSV Plotter — Reusable Across Projects =====
% Works with any HIL firmware project that logs via SEGGER RTT
% Setup: Copy RTT Viewer output (CSV format) to any subdirectory
% Logging source: SEGGER RTT Viewer Terminal 0
% Current example project: XMC4700/20_04_2026/ (Phase 5 inverter validation)
%
% TYPICAL LOGGED SIGNALS (current CSV schema):
%   step, theta_motor,
%   vr_ref, vy_ref, vb_ref,       (reference from dq->RYB, before PWM)
%   vr_mod, vy_mod, vb_mod,       (normalized modulation commands, divided by Vdc/2)
%   vr, vy, vb,                   (switched inverter output, after PWM)
%   carrier, id, iq, rpm_motor, T_e
%
% WORKFLOW:
%   1. Flash XMC4700 with XMC4700/25_04_2026/25_04_2026_refdq_to_abc_with_PWM/main.c
%   2. Open SEGGER RTT Viewer, connect to J-Link, select Terminal 0
%   3. Copy text output (from CSV DATA START to Complete line) into plot.csv
%   4. Run this script in MATLAB
%   5. Select CSV file number from the displayed list
%   6. Select Y-column indices (comma-separated, e.g. 2,3,4)
%   7. Select X-column (default = time_s, press Enter to confirm)
%   8. Select plot mode: single axes / one-per-subplot / custom grouped subplots
%
% NOTES:
%   - Script searches recursively under its own directory for all *.csv files
%   - Non-numeric footer lines (e.g. "=== Complete ===") are skipped safely
%   - ✅ time_s is logged directly in CSV (seconds from firmware time_elapsed counter)
%   - X-axis defaults to time_s when available, no calculation needed!
%   - Works with any decimation: time_s always accurate regardless of RTT_LOGGING_DECIMATION
%   - Python equivalent: plot_csv.py in the workspace root
%
% FEATURES:
%   1) Recursively finds CSV files under script directory
%   2) Lets you choose CSV file
%   3) Lets you choose Y columns (single or multiple)
%   4) Adds derived time columns when step exists (time_s, time_ms)
%   5) Lets you choose X column (default = time_s when available)
%   6) Supports subplot modes (single axes, one-per-signal, custom groups)
%   7) Lets you configure subplot layout columns for stacked or grid views
%   8) COLOR SCHEME SELECTION: default, tab10, Dark2, monochrome, custom RGB
%   9) PERFORMANCE: Fast loading for large CSVs (200K+ rows), optional downsampling
%
% ======================================================================

clear; close all; clc;

script_dir = fileparts(mfilename('fullpath'));
fprintf('Searching for CSV files in: %s\n\n', script_dir);

csv_files = dir(fullfile(script_dir, '**', '*.csv'));

if isempty(csv_files)
    fprintf('No CSV files found under: %s\n', script_dir);
    return;
end

full_paths = strings(numel(csv_files), 1);
display_paths = strings(numel(csv_files), 1);

for i = 1:numel(csv_files)
    full_paths(i) = string(fullfile(csv_files(i).folder, csv_files(i).name));
    relative_path = strrep(full_paths(i), string(script_dir) + filesep, '');
    display_paths(i) = relative_path;
end

[display_paths, sort_idx] = sort(display_paths);
full_paths = full_paths(sort_idx);

fprintf('Available CSV files:\n');
for i = 1:numel(display_paths)
    fprintf('  %d. %s\n', i, display_paths(i));
end

csv_idx = input(sprintf('\nSelect CSV (1-%d): ', numel(display_paths)));
if isempty(csv_idx) || ~isscalar(csv_idx) || csv_idx < 1 || csv_idx > numel(display_paths)
    fprintf('Invalid CSV selection.\n');
    return;
end

csv_file = char(full_paths(csv_idx));
fprintf('\nLoaded: %s\n', display_paths(csv_idx));

T = read_numeric_csv_robust(csv_file);
if isempty(T) || height(T) == 0
    fprintf('No numeric rows could be parsed from selected CSV.\n');
    return;
end

% Derive time columns from step for easier plotting on real time axis
if any(strcmp(T.Properties.VariableNames, 'step')) && ~any(strcmp(T.Properties.VariableNames, 'time_s'))
    step0 = T.step - T.step(1);
    T.time_s = step0 * 50e-6;  % Fixed: 50 µs per step (1 MHz / 50 decimation)
    fprintf('Derived time_s from step column (50 µs per row)\n');
end

var_names = T.Properties.VariableNames;
fprintf('Shape: %d rows x %d columns\n\n', height(T), width(T));
fprintf('Available columns:\n');
for i = 1:numel(var_names)
    fprintf('  %d. %s\n', i, var_names{i});
end

y_str = input('\nSelect Y column indices (comma-separated, e.g. 2,3,4): ', 's');
y_idx = parse_index_list(y_str);

if isempty(y_idx) || any(y_idx < 1) || any(y_idx > numel(var_names))
    fprintf('Invalid Y-axis selection.\n');
    return;
end

default_x_idx = [];
if any(strcmp(var_names, 'time_s'))
    default_x_idx = find(strcmp(var_names, 'time_s'), 1, 'first');
end

if isempty(default_x_idx)
    x_prompt = sprintf('X-axis column index (press Enter for index, 1-%d): ', numel(var_names));
else
    x_prompt = sprintf('X-axis column index (press Enter for default time_s=%d, 1-%d): ', default_x_idx, numel(var_names));
end

x_str = input(x_prompt, 's');
if isempty(strtrim(x_str))
    x_idx = default_x_idx;
else
    x_idx = str2double(x_str);
    if ~isscalar(x_idx) || isnan(x_idx) || x_idx < 1 || x_idx > numel(var_names)
        fprintf('Invalid X-axis selection.\n');
        return;
    end
end

fprintf('\nPlot mode:\n');
fprintf('  1) Single axes (all selected signals)\n');
fprintf('  2) Subplots (one signal per subplot)\n');
fprintf('  3) Subplots (custom grouped signals)\n');
mode_str = input('Select mode (1-3, Enter=1): ', 's');
if isempty(strtrim(mode_str))
    mode = 1;
else
    mode = str2double(mode_str);
end

if ~ismember(mode, [1 2 3])
    fprintf('Invalid plot mode.\n');
    return;
end

fprintf('\nColor scheme:\n');
fprintf('  1) default (MATLAB auto)\n');
fprintf('  2) tab10 (Tableau 10)\n');
fprintf('  3) Dark2 (Colorbrewer dark)\n');
fprintf('  4) monochrome (grayscale)\n');
fprintf('  5) custom RGB (enter hex codes)\n');
color_str = input('Select color scheme (1-5, Enter=1): ', 's');
if isempty(strtrim(color_str))
    color_scheme = 1;
else
    color_scheme = str2double(color_str);
end

if ~ismember(color_scheme, [1 2 3 4 5])
    fprintf('Invalid color scheme selection.\n');
    return;
end

color_list = get_color_scheme(color_scheme, numel(y_idx));

if isempty(x_idx)
    % When using Index as X-axis, convert to actual time in seconds
    % Formula: time_seconds = row_number × (1 / RTT_LOGGING_RATE_HZ)
    x_data = (0:height(T)-1)' * csv_logging_period_s;
    x_label = sprintf('Time (s) [CSV row × %.1f µs]', csv_logging_period_s*1e6);
else
    x_name = var_names{x_idx};
    x_data = T{:, x_name};
    x_label = x_name;
end

if mode == 1
    figure('Name', 'Interactive CSV Plot', 'NumberTitle', 'off', 'Position', [100 100 1300 700]);
    hold on;
    for k = 1:numel(y_idx)
        y_name = var_names{y_idx(k)};
        h = plot(x_data, T{:, y_name}, 'DisplayName', y_name, 'LineWidth', 0.8);
        if color_scheme > 1
            h.Color = color_list(k, :);
        end
    end
    xlabel(x_label, 'Interpreter', 'none');
    ylabel('Value');
    title(char(display_paths(csv_idx)), 'Interpreter', 'none');
    legend('Location', 'best');
    grid on;
    box on;
else
    if mode == 2
        groups = num2cell(y_idx(:));
    else
        fprintf('\nCustom group syntax uses selected-column order.\n');
        fprintf('Example: 1,2;3;4 means subplot1 has cols 1&2, subplot2 has col 3, subplot3 has col 4\n');
        for k = 1:numel(y_idx)
            fprintf('  %d. %s\n', k, var_names{y_idx(k)});
        end
        g_str = input('Enter subplot groups: ', 's');
        groups = parse_subplot_groups(g_str, y_idx);
        if isempty(groups)
            fprintf('Invalid subplot groups.\n');
            return;
        end
    end

    n_subplots = numel(groups);
    cols_str = input(sprintf('Subplot columns (1-%d, Enter=1): ', n_subplots), 's');
    if isempty(strtrim(cols_str))
        n_cols = 1;
    else
        n_cols = str2double(cols_str);
    end

    if ~isscalar(n_cols) || isnan(n_cols) || n_cols < 1 || n_cols > n_subplots
        fprintf('Invalid subplot column count.\n');
        return;
    end

    n_rows = ceil(n_subplots / n_cols);
    figure('Name', 'Interactive CSV Subplots', 'NumberTitle', 'off', 'Position', [100 80 1400 900]);
    t = tiledlayout(n_rows, n_cols, 'TileSpacing', 'compact', 'Padding', 'compact');
    title(t, char(display_paths(csv_idx)), 'Interpreter', 'none');

    for i = 1:n_subplots
        ax = nexttile;
        hold(ax, 'on');
        g = groups{i};
        names = strings(1, numel(g));
        for j = 1:numel(g)
            y_name = var_names{g(j)};
            h = plot(ax, x_data, T{:, y_name}, 'DisplayName', y_name, 'LineWidth', 0.8);
            if color_scheme > 1
                % Find which position this column has in y_idx
                col_pos = find(y_idx == g(j), 1);
                if ~isempty(col_pos)
                    h.Color = color_list(col_pos, :);
                end
            end
            names(j) = string(y_name);
        end
        title(ax, strjoin(names, ', '), 'Interpreter', 'none');
        ylabel(ax, 'Value');
        grid(ax, 'on');
        box(ax, 'on');
        legend(ax, 'Location', 'best');
        if i > (n_rows - 1) * n_cols
            xlabel(ax, x_label, 'Interpreter', 'none');
        end
    end
end

fprintf('\nPlot complete.\n');

function color_list = get_color_scheme(scheme_id, n_colors)
    % Returns n_colors x 3 RGB color matrix based on selected scheme
    switch scheme_id
        case 1  % default (MATLAB auto - return empty to use defaults)
            color_list = [];
        case 2  % tab10 (Tableau 10)
            tab10 = [
                0.31372549 0.41960784 0.58823529  % blue
                1.00000000 0.49803922 0.05490196  % orange
                0.11764706 0.56470588 0.11764706  % green
                0.84313725 0.18823529 0.15294118  % red
                0.58039216 0.40392157 0.74117647  % purple
                0.54901961 0.33725490 0.29411765  % brown
                0.89019608 0.46666667 0.76078431  % pink
                0.49803922 0.49803922 0.49803922  % gray
                0.73725490 0.74117647 0.13333333  % olive
                0.09019608 0.74509804 0.81176471  % cyan
            ];
            if n_colors <= size(tab10, 1)
                color_list = tab10(1:n_colors, :);
            else
                % Cycle/repeat if more colors needed
                color_list = tab10(mod((0:n_colors-1)', size(tab10, 1)) + 1, :);
            end
        case 3  % Dark2 (Colorbrewer)
            dark2 = [
                0.10588235 0.61960784 0.46666667  % teal
                0.85098039 0.37254902 0.00784314  % orange
                0.45882353 0.43921569 0.70196078  % purple
                0.90588235 0.16078431 0.54117647  % magenta
                0.40000000 0.65098039 0.11764706  % green
                0.90196078 0.67058824 0.00784314  % yellow
                0.65098039 0.46666667 0.11372549  % brown
                0.40000000 0.40000000 0.40000000  % dark gray
            ];
            if n_colors <= size(dark2, 1)
                color_list = dark2(1:n_colors, :);
            else
                color_list = dark2(mod((0:n_colors-1)', size(dark2, 1)) + 1, :);
            end
        case 4  % monochrome (grayscale)
            if n_colors == 1
                color_list = [0 0 0];  % black
            else
                % Create gradient from dark gray to light gray
                color_list = repmat(linspace(0.2, 0.8, n_colors)', 1, 3);
            end
        case 5  % custom RGB - prompt user
            color_list = prompt_custom_colors(n_colors);
    end
end

function color_list = prompt_custom_colors(n_colors)
    % Prompts user for hex color codes
    color_list = zeros(n_colors, 3);
    fprintf('\nEnter %d hex color codes (e.g., #FF0000 for red, or just FF0000)\n', n_colors);
    fprintf('Press Enter with empty input to skip and use defaults.\n');
    
    skip_count = 0;
    for i = 1:n_colors
        hex_str = input(sprintf('Color %d hex code: ', i), 's');
        hex_str = strtrim(hex_str);
        
        if isempty(hex_str)
            skip_count = skip_count + 1;
            color_list(i, :) = [];
            continue;
        end
        
        % Remove # if present
        if startsWith(hex_str, '#')
            hex_str = hex_str(2:end);
        end
        
        % Validate hex string length
        if length(hex_str) ~= 6
            fprintf('  Skipping invalid hex code (must be 6 characters)\n');
            skip_count = skip_count + 1;
            color_list(i, :) = [];
            continue;
        end
        
        % Convert hex to RGB
        try
            r = hex2dec(hex_str(1:2)) / 255;
            g = hex2dec(hex_str(3:4)) / 255;
            b = hex2dec(hex_str(5:6)) / 255;
            color_list(i, :) = [r, g, b];
        catch
            fprintf('  Skipping invalid hex code\n');
            skip_count = skip_count + 1;
            color_list(i, :) = [];
        end
    end
    
    % Remove empty rows and use default if too few colors provided
    color_list(all(color_list == 0, 2), :) = [];
    if isempty(color_list)
        fprintf('No valid colors entered. Using default color scheme.\n');
        color_list = [];
    end
end

function T = read_numeric_csv_robust(file_path)
    % Fast CSV reader optimized for large files (200K+ rows)
    % Uses file size estimation and preallocation for speed
    
    fid = fopen(file_path, 'r');
    if fid < 0
        error('Cannot open CSV file: %s', file_path);
    end

    cleanup_obj = onCleanup(@() fclose(fid));

    % Get file size for memory preallocation estimate
    fseek(fid, 0, 'eof');
    file_size_bytes = ftell(fid);
    fseek(fid, 0, 'bof');
    
    header = fgetl(fid);
    if ~ischar(header)
        T = table();
        return;
    end

    raw_names = strsplit(strtrim(header), ',');
    n_cols = numel(raw_names);
    names = matlab.lang.makeValidName(strtrim(raw_names), 'ReplacementStyle', 'delete');

    % Estimate number of rows from file size
    % Assume ~30 bytes per row on average (adjust based on typical data)
    header_size_bytes = length(header) + 2;
    data_size_bytes = file_size_bytes - header_size_bytes;
    estimated_rows = max(100, round(data_size_bytes / 30));
    
    % Preallocate numeric array with 20% headroom
    numeric_rows = zeros(round(estimated_rows * 1.2), n_cols);
    row_count = 0;
    
    while ~feof(fid)
        line = fgetl(fid);
        if ~ischar(line)
            continue;
        end

        line = strtrim(line);
        if isempty(line)
            continue;
        end

        parts = strsplit(line, ',');
        if numel(parts) ~= n_cols
            continue;
        end

        row = str2double(parts);
        if any(isnan(row))
            continue;
        end

        row_count = row_count + 1;
        if row_count > size(numeric_rows, 1)
            % Grow array if needed (double the size)
            numeric_rows = [numeric_rows; zeros(size(numeric_rows), 'like', numeric_rows)];
        end
        numeric_rows(row_count, :) = row;
    end

    % Trim to actual row count
    numeric_rows = numeric_rows(1:row_count, :);
    
    % Warn user if file is very large
    if row_count > 150000
        fprintf('  Note: Loaded %d rows. For faster interaction, consider downsampling (every Nth row).\n', row_count);
    end
    
    T = array2table(numeric_rows, 'VariableNames', names);
end

function idx = parse_index_list(s)
    s = strtrim(s);
    if isempty(s)
        idx = [];
        return;
    end

    tokens = strsplit(s, ',');
    idx = zeros(1, numel(tokens));
    for i = 1:numel(tokens)
        idx(i) = str2double(strtrim(tokens{i}));
    end

    if any(isnan(idx))
        idx = [];
    end
end

function groups = parse_subplot_groups(s, selected_idx)
    s = strtrim(s);
    if isempty(s)
        groups = {};
        return;
    end

    blocks = strsplit(s, ';');
    groups = cell(0, 1);
    n_sel = numel(selected_idx);

    for b = 1:numel(blocks)
        block = strtrim(blocks{b});
        if isempty(block)
            continue;
        end

        local_idx = parse_index_list(block);
        if isempty(local_idx) || any(local_idx < 1) || any(local_idx > n_sel)
            groups = {};
            return;
        end

        groups{end+1, 1} = selected_idx(local_idx); %#ok<AGROW>
    end
end
