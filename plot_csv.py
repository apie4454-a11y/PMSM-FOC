# =====================================================================
# Interactive CSV Plotter — FOC Motor HIL Data Visualisation
# =====================================================================
#
# PURPOSE:
#   General-purpose interactive CSV plotter for visualising RTT-logged
#   motor data from the XMC4700 HIL firmware. Allows dynamic selection
#   of CSV file, X-axis, and one or more Y-axis signals — no script
#   editing needed between runs.
#
# ===== Generic RTT CSV Plotter — Reusable Across Projects =====
# Works with any HIL firmware project that logs via SEGGER RTT
# Setup: Copy RTT Viewer output (CSV format) to any subdirectory
# Logging source: SEGGER RTT Viewer Terminal 0
# Current example project: XMC4700/20_04_2026/ (Phase 5 inverter validation)
#
# TYPICAL LOGGED SIGNALS (current CSV schema):
#   step, theta_motor,
#   vr_ref, vy_ref, vb_ref,       (reference from dq->RYB, before PWM)
#   vr_mod, vy_mod, vb_mod,       (normalized modulation commands, ÷ Vdc/2)
#   vr, vy, vb,                   (switched inverter output, after PWM)
#   carrier, id, iq, rpm_motor, T_e
#
# WORKFLOW:
#   1. Flash XMC4700 with XMC4700/20_04_2026/main.c
#   2. Open SEGGER RTT Viewer, connect to J-Link, select Terminal 0
#   3. Copy text output (from CSV DATA START to Complete line) into plot.csv
#      (place plot.csv anywhere under this script's directory tree)
#   4. Run:  python plot_csv.py
#   5. Select CSV file number from the list
#   6. Enter Y-column indices (comma-separated, e.g. 2,3,4)
#   7. Enter X-column index (press Enter for sample index)
#   8. Choose plot mode: single axes / one-per-subplot / custom groups
#   9. Choose color mode: default / tab palettes / monochrome / custom
#
# NOTES:
#   - Script searches recursively under its own directory for all *.csv files
#   - Non-numeric footer lines (e.g. "=== Complete ===") are handled by pandas automatically
#   - ✅ time_s is logged directly in CSV (seconds from firmware time_elapsed counter)
#   - X-axis defaults to time_s when available, no calculation needed!
#   - Works with any decimation: time_s always accurate regardless of RTT_LOGGING_DECIMATION
#   - For MATLAB equivalent, use: XMC4700/20_04_2026/plot_csv.m
#
# DEPENDENCIES:
#   pip install pandas matplotlib
#
# FEATURES:
#   1) Recursively finds CSV files under script directory
#   2) Lets you choose CSV file
#   3) Lets you choose Y columns and X column
#   4) Supports subplot modes (single, one-per-signal, custom groups)
#   5) Supports color modes (default cycle, tab10, dark2, monochrome, custom)
#
# =====================================================================

import pandas as pd
import matplotlib.pyplot as plt
import os
from pathlib import Path


def parse_index_list(text, max_index):
    """Parse comma-separated 1-based indices into 0-based list."""
    try:
        idx = [int(x.strip()) - 1 for x in text.split(',') if x.strip()]
    except ValueError:
        return None
    if not idx or any(i < 0 or i >= max_index for i in idx):
        return None
    return idx


def parse_subplot_groups(text, selected_cols):
    """Parse groups like '1,2;3;4' where indices refer to selected columns."""
    groups = []
    total = len(selected_cols)
    for chunk in text.split(';'):
        chunk = chunk.strip()
        if not chunk:
            continue
        idx = parse_index_list(chunk, total)
        if idx is None:
            return None
        groups.append([selected_cols[i] for i in idx])
    if not groups:
        return None
    return groups


def parse_color_list(text):
    colors = [c.strip() for c in text.split(',') if c.strip()]
    return colors if colors else None


def choose_palette(n_signals):
    print("\n🎨 Color mode:")
    print("  1. Matplotlib default")
    print("  2. tab10")
    print("  3. Dark2")
    print("  4. Monochrome")
    print("  5. Custom list (comma-separated colors)")
    choice = input("Select color mode (1-5, default 1): ").strip() or "1"

    if choice == "1":
        return None
    if choice == "2":
        cmap = plt.get_cmap('tab10')
        return [cmap(i % 10) for i in range(n_signals)]
    if choice == "3":
        cmap = plt.get_cmap('Dark2')
        return [cmap(i % 8) for i in range(n_signals)]
    if choice == "4":
        base = ['black', 'dimgray', 'gray', 'silver']
        return [base[i % len(base)] for i in range(n_signals)]
    if choice == "5":
        raw = input("Enter colors (e.g., red,#1f77b4,cyan): ").strip()
        custom = parse_color_list(raw)
        if custom is None:
            print("⚠️ Invalid custom list, using default colors.")
            return None
        return [custom[i % len(custom)] for i in range(n_signals)]

    print("⚠️ Invalid color mode, using default colors.")
    return None

def interactive_csv_plotter():
    """Interactive CSV plotter with dynamic column selection"""
    
    # Search in script's directory and ALL subdirectories recursively
    script_dir = Path(__file__).parent
    print(f"🔍 Searching in: {script_dir}\n")
    
    # Recursive search using rglob (more reliable than glob with **)
    csv_files = list(script_dir.rglob('*.csv'))
    csv_files = list(set(csv_files))  # Remove duplicates
    csv_files.sort(key=lambda x: str(x))
    
    if not csv_files:
        print(f"❌ No CSV files found in {script_dir} or subdirectories")
        return
    
    print("\n📁 Available CSV files:")
    for i, f in enumerate(csv_files, 1):
        rel_path = f.relative_to(script_dir)
        print(f"  {i}. {rel_path}")
    
    # Select CSV
    choice = input(f"\nSelect CSV (1-{len(csv_files)}): ").strip()
    try:
        csv_file = csv_files[int(choice) - 1]
    except (ValueError, IndexError):
        print("❌ Invalid selection")
        return
    
    # Load and display columns
    df = pd.read_csv(csv_file)
    rel_path = csv_file.relative_to(script_dir)
    print(f"\n✅ Loaded: {rel_path}")
    print(f"   Shape: {df.shape[0]} rows × {df.shape[1]} columns")
    
    print(f"\n📊 Available columns:")
    for i, col in enumerate(df.columns, 1):
        print(f"  {i}. {col}")
    
    # Select columns to plot
    print("\n🎯 Select columns to plot (comma-separated indices, e.g., '1,2,3'):")
    col_choice = input("   Columns: ").strip()
    col_indices = parse_index_list(col_choice, len(df.columns))
    if col_indices is None:
        print("❌ Invalid selection")
        return
    plot_cols = [df.columns[i] for i in col_indices]
    
    # Select X-axis (default to time_s if it exists)
    has_time_s = 'time_s' in df.columns
    if has_time_s:
        print(f"\n⏱️  Using 'time_s' column as default X-axis (already in seconds!)")
        x_choice = input(f"Override with different column (1-{len(df.columns)}, press Enter to use time_s): ").strip()
    else:
        x_choice = input(f"\nX-axis column (default: index, or enter column number 1-{len(df.columns)}): ").strip()
    
    if x_choice:
        try:
            x_col = df.columns[int(x_choice) - 1]
        except (ValueError, IndexError):
            x_col = 'time_s' if has_time_s else None
    else:
        x_col = 'time_s' if has_time_s else None
    
    # Subplot configuration
    print("\n🧩 Plot mode:")
    print("  1. Single axes (all selected signals)")
    print("  2. Subplots (one signal per subplot)")
    print("  3. Subplots (custom grouped signals)")
    mode = input("Select mode (1-3, default 1): ").strip() or "1"
    if mode not in {"1", "2", "3"}:
        print("❌ Invalid mode")
        return

    palette = choose_palette(len(plot_cols))
    color_by_col = {}
    if palette is not None:
        for i, col in enumerate(plot_cols):
            color_by_col[col] = palette[i % len(palette)]

    def get_x_data():
        if x_col:
            return df[x_col], x_col
        # No X-axis selected, use sample index
        return df.index, 'Sample Index'

    # Mode 1: single axes
    if mode == "1":
        plt.figure(figsize=(14, 7))
        x_data, x_label = get_x_data()
        for col in plot_cols:
            kwargs = {'label': col, 'linewidth': 0.8, 'alpha': 0.9}
            if col in color_by_col:
                kwargs['color'] = color_by_col[col]
            plt.plot(x_data, df[col], **kwargs)
        plt.xlabel(x_label)
        plt.ylabel('Value')
        rel_path = csv_file.relative_to(script_dir)
        plt.title(f'{rel_path}')
        plt.legend(loc='best')
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.show()
        return

    # Common layout for subplot modes
    if mode == "2":
        groups = [[col] for col in plot_cols]
    else:
        print("\nCustom group syntax uses selected-column order.")
        print("Example: 1,2;3;4 means subplot1 has cols 1&2, subplot2 has col 3, subplot3 has col 4")
        for i, col in enumerate(plot_cols, 1):
            print(f"  {i}. {col}")
        group_text = input("Enter subplot groups: ").strip()
        groups = parse_subplot_groups(group_text, plot_cols)
        if groups is None:
            print("❌ Invalid subplot groups")
            return

    n_subplots = len(groups)
    cols_text = input(f"Subplot columns (1-{n_subplots}, default 1): ").strip()
    if cols_text:
        try:
            n_cols = int(cols_text)
        except ValueError:
            print("❌ Invalid subplot column count")
            return
    else:
        n_cols = 1
    if n_cols < 1 or n_cols > n_subplots:
        print("❌ Invalid subplot column count")
        return

    n_rows = (n_subplots + n_cols - 1) // n_cols
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(14, max(4, 3.5 * n_rows)), sharex=True)
    if not isinstance(axes, (list, tuple)):
        import numpy as np
        axes = np.array(axes).reshape(-1)
    else:
        import numpy as np
        axes = np.array(axes).reshape(-1)

    x_data, x_label = get_x_data()
    for i, group in enumerate(groups):
        ax = axes[i]
        for col in group:
            kwargs = {'label': col, 'linewidth': 0.8, 'alpha': 0.9}
            if col in color_by_col:
                kwargs['color'] = color_by_col[col]
            ax.plot(x_data, df[col], **kwargs)
        ax.set_ylabel('Value')
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        ax.set_title(', '.join(group))

    # Hide unused axes in a rectangular grid
    for i in range(n_subplots, len(axes)):
        axes[i].set_visible(False)

    axes[min(n_subplots - 1, len(axes) - 1)].set_xlabel(x_label)
    rel_path = csv_file.relative_to(script_dir)
    fig.suptitle(f'{rel_path}')
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    interactive_csv_plotter()