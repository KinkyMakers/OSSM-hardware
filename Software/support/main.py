import matplotlib.pyplot as plt
import numpy as np
import re

# Parse the log lines
data = []
pattern = r'\[Streaming\].*?targetPosition: ([-\d.]+).*?targetVelocity: ([-\d.]+).*?currentPosition: ([-\d.]+).*?currentVelocity: ([-\d.]+)'
index_pattern = r'\[Streaming\] Speed multiplier index: (\d+)'

index_changes = []
for line in open("log.txt", "r").read().split('\n'):
    match = re.search(pattern, line)
    index_match = re.search(index_pattern, line)
    if match:
        target_pos = float(match.group(1))
        target_vel = float(match.group(2))
        current_pos = float(match.group(3))
        current_vel = float(match.group(4)) / 1000.0  # Divide by 1000
        data.append({
            'target_pos': target_pos,
            'target_vel': target_vel,
            'current_pos': current_pos,
            'current_vel': current_vel
        })
    if index_match:
        index_changes.append(len(data))

# Create time array (67ms intervals based on timestamps)
time = np.arange(len(data)) * 0.067

# Create subplots
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
# Position plot
ax1.plot(time, [d['target_pos'] for d in data], label='Target Position', color='blue')
ax1.scatter(time, [d['current_pos'] for d in data], label='Current Position', color='red', marker='.')
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Position (steps)')
ax1.grid(True)
ax1.legend()

# Add index change lines to position plot
for idx, change_point in enumerate(index_changes):
    if change_point < len(time):
        ax1.axvline(x=time[change_point], color='green', linestyle='--', alpha=0.5)
        ax1.text(time[change_point], ax1.get_ylim()[1], f'Index {idx}', rotation=90)

# Velocity plot
ax2.plot(time, [d['target_vel'] for d in data], label='Target Velocity', color='blue')
ax2.plot(time, [d['current_vel'] for d in data], label='Current Velocity', color='red')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Velocity (steps/s)')
ax2.grid(True)
ax2.legend()

# Add index change lines to velocity plot
for idx, change_point in enumerate(index_changes):
    if change_point < len(time):
        ax2.axvline(x=time[change_point], color='green', linestyle='--', alpha=0.5)
        ax2.text(time[change_point], ax2.get_ylim()[1], f'Index {idx}', rotation=90)

plt.tight_layout()
plt.show()
