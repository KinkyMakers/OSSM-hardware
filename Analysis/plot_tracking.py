#!/usr/bin/env python3
"""
OSSM Tracking Data Plotter

Generates plots from tracking CSV files including:
- Position vs time (centered around 0)
- Sinusoid fit with one period
- Statistics (min, max, range)
"""

import argparse
from pathlib import Path
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
from scipy.optimize import curve_fit

# Set seaborn style
sns.set_theme(style="whitegrid", palette="deep")


def load_tracking_data(csv_path: Path) -> pd.DataFrame:
    """Load tracking data from CSV file.
    
    Supports both old format (with position_mm) and new format (with pose_y).
    """
    df = pd.read_csv(csv_path)
    
    # Handle new format: use pose_y as position_mm
    if 'pose_y' in df.columns and 'position_mm' not in df.columns:
        df['position_mm'] = df['pose_y']
    
    # Drop rows with missing position data
    df = df.dropna(subset=['position_mm'])
    return df


def center_position_data(df: pd.DataFrame) -> pd.DataFrame:
    """Center position data around 0 (midpoint of min/max)."""
    df = df.copy()
    position = df['position_mm']
    midpoint = (position.max() + position.min()) / 2
    df['position_centered'] = position - midpoint
    return df


def sinusoid(t: np.ndarray, amplitude: float, frequency: float, phase: float, offset: float) -> np.ndarray:
    """Sinusoidal function: A * sin(2*pi*f*t + phi) + offset"""
    return amplitude * np.sin(2 * np.pi * frequency * t + phase) + offset


def compute_rms_error(params: tuple, time: np.ndarray, position: np.ndarray) -> float:
    """Compute RMS error for given sinusoid parameters."""
    amplitude, frequency, phase, offset = params
    fitted = sinusoid(time, amplitude, frequency, phase, offset)
    return np.sqrt(np.mean((position - fitted) ** 2))


def fit_sinusoid(time: np.ndarray, position: np.ndarray) -> tuple[dict, np.ndarray]:
    """
    Fit a sinusoid to position data using RMS minimization with grid search.
    
    Uses a multi-stage approach:
    1. Grid search over frequencies to find best starting point
    2. For each candidate frequency, optimize amplitude and phase analytically
    3. Refine the best solution with full nonlinear optimization
    
    Returns fit parameters and fitted values.
    """
    from scipy.optimize import minimize, differential_evolution
    
    # Basic estimates
    amplitude_guess = (position.max() - position.min()) / 2
    offset_guess = position.mean()
    position_centered = position - offset_guess
    
    dt = np.mean(np.diff(time))
    duration = time[-1] - time[0]
    
    # Stage 1: Grid search over frequencies
    # Search from very low frequencies to Nyquist
    min_freq = 0.5 / duration  # Half a cycle minimum
    max_freq = min(10.0, 0.5 / dt)  # Up to Nyquist or 10 Hz
    
    # Also use FFT to get a candidate frequency
    n = len(position)
    fft = np.fft.fft(position_centered)
    freqs = np.fft.fftfreq(n, dt)
    pos_mask = freqs > 0
    fft_freq = freqs[pos_mask][np.argmax(np.abs(fft[pos_mask]))]
    
    # Create frequency grid with finer resolution, include FFT estimate region
    n_freqs = 500
    freq_grid = np.linspace(min_freq, max_freq, n_freqs)
    
    # Add extra points around FFT estimate
    if fft_freq > min_freq and fft_freq < max_freq:
        extra_freqs = np.linspace(fft_freq * 0.8, fft_freq * 1.2, 50)
        freq_grid = np.unique(np.concatenate([freq_grid, extra_freqs]))
    
    best_rms = np.inf
    best_freq = 1.0
    best_amplitude = amplitude_guess
    best_phase = 0.0
    
    # For each frequency, find optimal amplitude and phase analytically
    # Using linear least squares: position = A*sin(wt) + B*cos(wt) + offset
    # where amplitude = sqrt(A^2 + B^2), phase = atan2(B, A)
    for freq in freq_grid:
        omega_t = 2 * np.pi * freq * time
        sin_wt = np.sin(omega_t)
        cos_wt = np.cos(omega_t)
        
        # Solve for coefficients using normal equations
        # [sum(sin^2)  sum(sin*cos)] [A]   [sum(pos*sin)]
        # [sum(sin*cos) sum(cos^2) ] [B] = [sum(pos*cos)]
        ss = np.sum(sin_wt ** 2)
        cc = np.sum(cos_wt ** 2)
        sc = np.sum(sin_wt * cos_wt)
        ps = np.sum(position_centered * sin_wt)
        pc = np.sum(position_centered * cos_wt)
        
        det = ss * cc - sc * sc
        if abs(det) < 1e-10:
            continue
            
        A = (cc * ps - sc * pc) / det
        B = (ss * pc - sc * ps) / det
        
        amp = np.sqrt(A * A + B * B)
        phase = np.arctan2(B, A)
        
        # Compute RMS error
        fitted = amp * np.sin(omega_t + phase) + offset_guess
        rms = np.sqrt(np.mean((position - fitted) ** 2))
        
        if rms < best_rms:
            best_rms = rms
            best_freq = freq
            best_amplitude = amp
            best_phase = phase
    
    # Stage 2: Refine with nonlinear optimization around best solution
    def objective(params):
        amp, freq, phase, off = params
        fitted = sinusoid(time, amp, freq, phase, off)
        return np.sqrt(np.mean((position - fitted) ** 2))
    
    # Try optimization from best grid search result
    x0 = [best_amplitude, best_freq, best_phase, offset_guess]
    
    bounds = [
        (0.1, amplitude_guess * 3),           # amplitude
        (best_freq * 0.5, best_freq * 2.0),   # frequency (near grid search result)
        (-np.pi, np.pi),                       # phase
        (-amplitude_guess, amplitude_guess),   # offset (should be near 0 for centered data)
    ]
    
    try:
        # Use Nelder-Mead for robust convergence
        result = minimize(
            objective,
            x0,
            method='Nelder-Mead',
            options={'maxiter': 5000, 'xatol': 1e-8, 'fatol': 1e-8}
        )
        
        if result.fun < best_rms:
            best_amplitude, best_freq, best_phase, best_offset = result.x
        else:
            best_offset = offset_guess
            
    except Exception:
        best_offset = offset_guess
    
    # Stage 3: Try differential evolution for global search if RMS is still high
    if best_rms > amplitude_guess * 0.3:  # If error is > 30% of amplitude
        try:
            de_bounds = [
                (amplitude_guess * 0.5, amplitude_guess * 1.5),
                (min_freq, max_freq),  # Full frequency range
                (-np.pi, np.pi),
                (-amplitude_guess * 0.5, amplitude_guess * 0.5),
            ]
            
            de_result = differential_evolution(
                objective,
                de_bounds,
                maxiter=1000,
                tol=1e-7,
                seed=42,
                workers=1,
                updating='deferred'
            )
            
            if de_result.fun < best_rms:
                best_amplitude, best_freq, best_phase, best_offset = de_result.x
                best_rms = de_result.fun
                
        except Exception:
            pass
    
    # Compute final fitted values
    fitted = sinusoid(time, best_amplitude, best_freq, best_phase, best_offset)
    
    # Calculate R-squared
    ss_res = np.sum((position - fitted) ** 2)
    ss_tot = np.sum((position - np.mean(position)) ** 2)
    r_squared = 1 - (ss_res / ss_tot) if ss_tot > 0 else 0
    
    period = 1 / best_freq if best_freq > 0 else np.nan
    
    return {
        'amplitude': abs(best_amplitude),
        'frequency': best_freq,
        'period': period,
        'phase': best_phase,
        'offset': best_offset,
        'r_squared': max(0, r_squared),
        'rms_error': best_rms,
        'success': r_squared > 0.5
    }, fitted


def calculate_velocity(df: pd.DataFrame) -> pd.Series:
    """Calculate velocity from position data using central difference."""
    dt = df['time_s'].diff()
    dx = df['position_centered'].diff()
    velocity = dx / dt
    
    # Use forward difference for first point
    velocity.iloc[0] = velocity.iloc[1] if len(velocity) > 1 else 0
    
    return velocity


def calculate_statistics(df: pd.DataFrame, velocity: pd.Series, fit_params: dict) -> dict:
    """Calculate summary statistics for the tracking data."""
    position = df['position_centered']
    
    return {
        'min_position': position.min(),
        'max_position': position.max(),
        'range': position.max() - position.min(),
        'mean_position': position.mean(),
        'std_position': position.std(),
        'min_velocity': velocity.min(),
        'max_velocity': velocity.max(),
        'mean_velocity': velocity.abs().mean(),
        'max_abs_velocity': velocity.abs().max(),
        'duration': df['time_s'].max() - df['time_s'].min(),
        'frame_count': len(df),
        'fit_amplitude': fit_params['amplitude'],
        'fit_frequency': fit_params['frequency'],
        'fit_period': fit_params['period'],
        'fit_r_squared': fit_params['r_squared'],
    }


def plot_single_file(csv_path: Path, output_dir: Optional[Path] = None, show: bool = False) -> dict:
    """
    Generate plots for a single tracking CSV file.
    
    Returns statistics dictionary.
    """
    print(f"\nProcessing: {csv_path.name}")
    
    # Load and center data
    df = load_tracking_data(csv_path)
    if df.empty:
        print(f"  WARNING: No valid data in {csv_path.name}")
        return {}
    
    df = center_position_data(df)
    
    # Fit sinusoid to centered data
    time = df['time_s'].values
    position = df['position_centered'].values
    fit_params, fitted_values = fit_sinusoid(time, position)
    
    # Calculate velocity
    velocity = calculate_velocity(df)
    
    # Calculate statistics
    stats = calculate_statistics(df, velocity, fit_params)
    
    # Print statistics
    print(f"  Duration: {stats['duration']:.2f} s ({stats['frame_count']} frames)")
    print(f"  Position: min={stats['min_position']:.2f} mm, max={stats['max_position']:.2f} mm")
    print(f"  Range: {stats['range']:.2f} mm")
    print(f"  Fit: A={fit_params['amplitude']:.1f} mm, f={fit_params['frequency']:.3f} Hz, T={fit_params['period']:.3f} s, R²={fit_params['r_squared']:.4f}, RMS={fit_params['rms_error']:.2f} mm")
    
    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Extract speed setting from filename (e.g., "OSSM_SPEED_050" -> "50%")
    speed_label = ""
    speed_num = None
    if "SPEED_" in csv_path.stem:
        try:
            speed_num = int(csv_path.stem.split("SPEED_")[1].split("_")[0])
            speed_label = f" (Speed: {speed_num}%)"
        except (IndexError, ValueError):
            pass
    
    fig.suptitle(f"OSSM Tracking Analysis: {csv_path.stem}{speed_label}", fontsize=14, fontweight='bold')
    
    # Plot 1: Position vs Time (centered, fixed y-axis)
    ax1 = axes[0, 0]
    ax1.plot(df['time_s'], df['position_centered'], color=sns.color_palette()[0], linewidth=0.8, alpha=0.8)
    ax1.axhline(y=stats['min_position'], color=sns.color_palette()[3], linestyle='--', alpha=0.7, label=f"Min: {stats['min_position']:.1f} mm")
    ax1.axhline(y=stats['max_position'], color=sns.color_palette()[2], linestyle='--', alpha=0.7, label=f"Max: {stats['max_position']:.1f} mm")
    ax1.axhline(y=0, color='gray', linestyle='-', alpha=0.5, linewidth=1)
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Position (mm)')
    ax1.set_title('Position vs Time (Centered)')
    ax1.set_ylim(-140, 140)
    ax1.legend(loc='upper right', fontsize=8)
    
    # Plot 2: Sinusoid Fit - One Period
    ax2 = axes[0, 1]
    
    if fit_params['success'] and fit_params['period'] > 0:
        # Find a good starting point (near a zero crossing going up)
        period = fit_params['period']
        
        # Extract one period of data
        # Find where we have at least one full period
        period_samples = int(period / np.mean(np.diff(time)))
        if period_samples < len(time):
            # Start from beginning, show one period
            start_idx = 0
            end_idx = min(start_idx + period_samples, len(time))
            
            t_period = time[start_idx:end_idx] - time[start_idx]
            pos_period = position[start_idx:end_idx]
            fit_period = fitted_values[start_idx:end_idx]
            
            ax2.plot(t_period, pos_period, color=sns.color_palette()[0], linewidth=1.5, alpha=0.7, label='Data')
            ax2.plot(t_period, fit_period, color=sns.color_palette()[3], linewidth=2, linestyle='--', label='Sinusoid Fit')
        else:
            # Not enough data for full period, show all
            ax2.plot(time - time[0], position, color=sns.color_palette()[0], linewidth=1.5, alpha=0.7, label='Data')
            ax2.plot(time - time[0], fitted_values, color=sns.color_palette()[3], linewidth=2, linestyle='--', label='Sinusoid Fit')
    else:
        ax2.plot(time - time[0], position, color=sns.color_palette()[0], linewidth=1.5, alpha=0.7, label='Data')
        ax2.text(0.5, 0.5, 'Fit Failed', transform=ax2.transAxes, ha='center', va='center', fontsize=14, color='red')
    
    ax2.axhline(y=0, color='gray', linestyle='-', alpha=0.5, linewidth=1)
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Position (mm)')
    ax2.set_title(f'One Period (T={fit_params["period"]:.3f} s, f={fit_params["frequency"]:.3f} Hz)')
    ax2.set_ylim(-140, 140)
    ax2.legend(loc='upper right', fontsize=8)
    
    # Plot 3: Position Histogram
    ax3 = axes[1, 0]
    sns.histplot(df['position_centered'], bins=50, ax=ax3, color=sns.color_palette()[0], edgecolor='white', alpha=0.7)
    ax3.axvline(x=stats['min_position'], color=sns.color_palette()[3], linestyle='--', linewidth=2, label='Min')
    ax3.axvline(x=stats['max_position'], color=sns.color_palette()[2], linestyle='--', linewidth=2, label='Max')
    ax3.axvline(x=0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
    ax3.set_xlabel('Position (mm)')
    ax3.set_ylabel('Frequency')
    ax3.set_title(f'Position Distribution (Range: {stats["range"]:.1f} mm)')
    ax3.set_xlim(-140, 140)
    ax3.legend(loc='upper right', fontsize=8)
    
    # Plot 4: Statistics Summary as text
    ax4 = axes[1, 1]
    ax4.axis('off')
    
    fit_status = "✓" if fit_params['success'] else "✗"
    
    stats_text = f"""
    Summary Statistics
    {'─' * 40}
    
    Position (Centered)
    ├─ Minimum:      {stats['min_position']:>10.2f} mm
    ├─ Maximum:      {stats['max_position']:>10.2f} mm
    ├─ Range:        {stats['range']:>10.2f} mm
    └─ Std Dev:      {stats['std_position']:>10.2f} mm
    
    Sinusoid Fit {fit_status}
    ├─ Amplitude:    {fit_params['amplitude']:>10.2f} mm
    ├─ Frequency:    {fit_params['frequency']:>10.4f} Hz
    ├─ Period:       {fit_params['period']:>10.4f} s
    ├─ R²:           {fit_params['r_squared']:>10.4f}
    └─ RMS Error:    {fit_params['rms_error']:>10.2f} mm
    
    Velocity (from data)
    ├─ Max |v|:      {stats['max_abs_velocity']:>10.1f} mm/s
    └─ Mean |v|:     {stats['mean_velocity']:>10.1f} mm/s
    
    Timing
    ├─ Duration:     {stats['duration']:>10.2f} s
    └─ Frames:       {stats['frame_count']:>10d}
    """
    
    ax4.text(0.05, 0.95, stats_text, transform=ax4.transAxes, fontsize=11,
             verticalalignment='top', fontfamily='monospace',
             bbox=dict(boxstyle='round', facecolor=sns.color_palette("pastel")[0], alpha=0.3))
    
    plt.tight_layout()
    
    # Save plot
    if output_dir is None:
        output_dir = csv_path.parent
    
    plot_path = output_dir / f"{csv_path.stem}_plot.png"
    fig.savefig(plot_path, dpi=150, bbox_inches='tight')
    print(f"  Saved: {plot_path.name}")
    
    if show:
        plt.show()
    else:
        plt.close(fig)
    
    return stats


def plot_all_outputs(output_dir: Path, show: bool = False) -> list[dict]:
    """
    Process all tracking CSV files in the output directory.
    
    Returns list of statistics dictionaries.
    """
    # Support both old pattern (*_tracking_data.csv) and new pattern (tracking_data_*.csv)
    csv_files = sorted(
        set(output_dir.glob("*_tracking_data.csv")) | 
        set(output_dir.glob("tracking_data_*.csv"))
    )
    
    if not csv_files:
        print(f"No tracking CSV files found in: {output_dir}")
        return []
    
    print(f"\n{'=' * 60}")
    print(f"OSSM Tracking Data Plotter")
    print(f"{'=' * 60}")
    print(f"Found {len(csv_files)} tracking files in: {output_dir}")
    
    all_stats = []
    
    for csv_path in csv_files:
        stats = plot_single_file(csv_path, output_dir, show=show)
        if stats:
            stats['filename'] = csv_path.stem
            all_stats.append(stats)
    
    # Create comparison plot if multiple files
    if len(all_stats) > 1:
        create_comparison_plot(all_stats, output_dir)
    
    print(f"\n{'=' * 60}")
    print(f"Complete! Processed {len(all_stats)} files")
    print(f"{'=' * 60}")
    
    return all_stats


def create_comparison_plot(all_stats: list[dict], output_dir: Path) -> None:
    """Create a comparison plot across all tracking files."""
    print("\nCreating comparison plot...")
    
    # Extract data for plotting
    filenames = [s['filename'] for s in all_stats]
    
    # Try to extract speed numbers from filenames
    speeds = []
    for name in filenames:
        if "SPEED_" in name:
            try:
                speed = int(name.split("SPEED_")[1].split("_")[0])
                speeds.append(speed)
            except (IndexError, ValueError):
                speeds.append(None)
        else:
            speeds.append(None)
    
    # Use speeds as x-axis if all extracted successfully, otherwise use indices
    if all(s is not None for s in speeds):
        x_values = speeds
        x_label = "Speed Setting (%)"
        # Sort by speed
        sorted_data = sorted(zip(x_values, all_stats), key=lambda x: x[0])
        x_values = [d[0] for d in sorted_data]
        all_stats = [d[1] for d in sorted_data]
    else:
        x_values = list(range(len(filenames)))
        x_label = "File Index"
    
    palette = sns.color_palette()
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle("OSSM Tracking Comparison Across All Tests", fontsize=14, fontweight='bold')
    
    # Plot 1: Range comparison
    ax1 = axes[0, 0]
    ranges = [s['range'] for s in all_stats]
    sns.barplot(x=x_values, y=ranges, ax=ax1, color=palette[0], edgecolor='white')
    ax1.set_xlabel(x_label)
    ax1.set_ylabel('Position Range (mm)')
    ax1.set_title('Stroke Range by Speed Setting')
    
    # Plot 2: Frequency comparison
    ax2 = axes[0, 1]
    frequencies = [s.get('fit_frequency', 0) for s in all_stats]
    sns.barplot(x=x_values, y=frequencies, ax=ax2, color=palette[1], edgecolor='white')
    ax2.set_xlabel(x_label)
    ax2.set_ylabel('Frequency (Hz)')
    ax2.set_title('Oscillation Frequency by Speed Setting')
    
    # Plot 3: Amplitude comparison (from sinusoid fit)
    ax3 = axes[1, 0]
    amplitudes = [s.get('fit_amplitude', 0) for s in all_stats]
    sns.barplot(x=x_values, y=amplitudes, ax=ax3, color=palette[2], edgecolor='white')
    ax3.set_xlabel(x_label)
    ax3.set_ylabel('Amplitude (mm)')
    ax3.set_title('Fit Amplitude by Speed Setting')
    
    # Plot 4: Max velocity comparison
    ax4 = axes[1, 1]
    max_vels = [s['max_abs_velocity'] for s in all_stats]
    sns.barplot(x=x_values, y=max_vels, ax=ax4, color=palette[3], edgecolor='white')
    ax4.set_xlabel(x_label)
    ax4.set_ylabel('Max |Velocity| (mm/s)')
    ax4.set_title('Maximum Speed by Speed Setting')
    
    plt.tight_layout()
    
    plot_path = output_dir / "comparison_plot.png"
    fig.savefig(plot_path, dpi=150, bbox_inches='tight')
    print(f"  Saved: {plot_path.name}")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Generate plots from OSSM tracking CSV files"
    )
    parser.add_argument(
        "path",
        nargs="?",
        default=None,
        help="Path to single CSV file or directory containing CSV files (default: ./output)"
    )
    parser.add_argument(
        "-o", "--output-dir",
        help="Directory to save plots (default: same as input)"
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display plots interactively (default: just save to files)"
    )
    
    args = parser.parse_args()
    
    # Determine input path
    if args.path is None:
        # Default to output directory in same folder as script
        input_path = Path(__file__).parent / "output"
    else:
        input_path = Path(args.path)
    
    if not input_path.exists():
        print(f"Error: Path does not exist: {input_path}")
        return 1
    
    # Determine output directory
    output_dir = Path(args.output_dir) if args.output_dir else None
    
    # Process files
    if input_path.is_file():
        if not input_path.suffix == '.csv':
            print(f"Error: Expected CSV file, got: {input_path}")
            return 1
        plot_single_file(input_path, output_dir, show=args.show)
    else:
        plot_all_outputs(input_path, show=args.show)
    
    return 0


if __name__ == "__main__":
    exit(main())
