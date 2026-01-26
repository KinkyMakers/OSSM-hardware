#!/usr/bin/env python3
"""
OSSM Video Analysis Tool

Tracks a point on an OSSM from video footage and exports position-time data.
"""

import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import cv2
import numpy as np


@dataclass
class CalibrationData:
    """Stores calibration line data for pixel-to-mm conversion."""
    point1: tuple[int, int]
    point2: tuple[int, int]
    length_mm: float
    
    @property
    def pixel_length(self) -> float:
        """Calculate the pixel length of the calibration line."""
        dx = self.point2[0] - self.point1[0]
        dy = self.point2[1] - self.point1[1]
        return math.sqrt(dx * dx + dy * dy)
    
    @property
    def mm_per_pixel(self) -> float:
        """Get the conversion factor from pixels to millimeters."""
        return self.length_mm / self.pixel_length
    
    @property
    def angle(self) -> float:
        """Get the angle of the calibration line in radians."""
        dx = self.point2[0] - self.point1[0]
        dy = self.point2[1] - self.point1[1]
        return math.atan2(dy, dx)


@dataclass
class TrackingCircle:
    """Stores the tracking circle parameters."""
    center: tuple[int, int]
    radius: int


class CalibrationUI:
    """Handles the calibration line drawing UI."""
    
    def __init__(self, frame: np.ndarray):
        self.frame = frame.copy()
        self.original_frame = frame.copy()
        self.point1: Optional[tuple[int, int]] = None
        self.point2: Optional[tuple[int, int]] = None
        self.drawing = False
        self.done = False
        
    def mouse_callback(self, event: int, x: int, y: int, flags: int, param) -> None:
        """Handle mouse events for drawing the calibration line."""
        if event == cv2.EVENT_LBUTTONDOWN:
            self.point1 = (x, y)
            self.point2 = None
            self.drawing = True
            
        elif event == cv2.EVENT_MOUSEMOVE and self.drawing:
            self.point2 = (x, y)
            self.frame = self.original_frame.copy()
            if self.point1 and self.point2:
                cv2.line(self.frame, self.point1, self.point2, (0, 255, 0), 2)
                # Draw endpoint circles
                cv2.circle(self.frame, self.point1, 5, (0, 255, 0), -1)
                cv2.circle(self.frame, self.point2, 5, (0, 255, 0), -1)
                
        elif event == cv2.EVENT_LBUTTONUP:
            self.drawing = False
            self.point2 = (x, y)
            self.frame = self.original_frame.copy()
            if self.point1 and self.point2:
                cv2.line(self.frame, self.point1, self.point2, (0, 255, 0), 2)
                cv2.circle(self.frame, self.point1, 5, (0, 255, 0), -1)
                cv2.circle(self.frame, self.point2, 5, (0, 255, 0), -1)
    
    def run(self) -> Optional[tuple[tuple[int, int], tuple[int, int]]]:
        """Run the calibration UI and return the line endpoints."""
        window_name = "Draw Calibration Line (Enter to confirm, R to reset, Q to quit)"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.setMouseCallback(window_name, self.mouse_callback)
        
        # Add instructions to frame
        instructions = [
            "CALIBRATION: Draw a line along the linear rail",
            "Click and drag to draw the line",
            "Press ENTER to confirm, R to reset, Q to quit"
        ]
        
        while True:
            display_frame = self.frame.copy()
            
            # Add instructions
            y_offset = 30
            for instruction in instructions:
                cv2.putText(display_frame, instruction, (10, y_offset),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
                cv2.putText(display_frame, instruction, (10, y_offset),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 1)
                y_offset += 25
            
            # Show pixel length if line is drawn
            if self.point1 and self.point2:
                dx = self.point2[0] - self.point1[0]
                dy = self.point2[1] - self.point1[1]
                pixel_length = math.sqrt(dx * dx + dy * dy)
                length_text = f"Line length: {pixel_length:.1f} pixels"
                cv2.putText(display_frame, length_text, (10, y_offset + 10),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            
            cv2.imshow(window_name, display_frame)
            
            key = cv2.waitKey(1) & 0xFF
            
            if key == 13:  # Enter
                if self.point1 and self.point2:
                    cv2.destroyWindow(window_name)
                    return (self.point1, self.point2)
                    
            elif key == ord('r') or key == ord('R'):
                self.frame = self.original_frame.copy()
                self.point1 = None
                self.point2 = None
                
            elif key == ord('q') or key == ord('Q'):
                cv2.destroyWindow(window_name)
                return None
        
        return None


class TrackingPointUI:
    """Handles the tracking point selection UI."""
    
    def __init__(self, frame: np.ndarray, calibration: CalibrationData):
        self.frame = frame.copy()
        self.original_frame = frame.copy()
        self.calibration = calibration
        self.center: Optional[tuple[int, int]] = None
        self.radius = 20
        
    def mouse_callback(self, event: int, x: int, y: int, flags: int, param) -> None:
        """Handle mouse events for selecting the tracking point."""
        if event == cv2.EVENT_LBUTTONDOWN:
            self.center = (x, y)
            self._update_frame()
            
        elif event == cv2.EVENT_MOUSEWHEEL:
            if flags > 0:
                self.radius = min(100, self.radius + 2)
            else:
                self.radius = max(5, self.radius - 2)
            if self.center:
                self._update_frame()
    
    def _update_frame(self) -> None:
        """Update the frame with the current tracking circle."""
        self.frame = self.original_frame.copy()
        
        # Draw calibration line
        cv2.line(self.frame, self.calibration.point1, 
                self.calibration.point2, (0, 255, 0), 2)
        
        # Draw tracking circle
        if self.center:
            cv2.circle(self.frame, self.center, self.radius, (0, 0, 255), 2)
            cv2.circle(self.frame, self.center, 3, (0, 0, 255), -1)
    
    def run(self) -> Optional[TrackingCircle]:
        """Run the tracking point selection UI."""
        window_name = "Select Tracking Point (Enter to confirm, R to reset, Q to quit)"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.setMouseCallback(window_name, self.mouse_callback)
        
        # Draw initial calibration line
        cv2.line(self.frame, self.calibration.point1, 
                self.calibration.point2, (0, 255, 0), 2)
        
        instructions = [
            "TRACKING: Click to place tracking circle",
            "Mouse wheel to adjust radius",
            "Press ENTER to confirm, R to reset, Q to quit"
        ]
        
        while True:
            display_frame = self.frame.copy()
            
            # Add instructions
            y_offset = 30
            for instruction in instructions:
                cv2.putText(display_frame, instruction, (10, y_offset),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
                cv2.putText(display_frame, instruction, (10, y_offset),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 1)
                y_offset += 25
            
            # Show current radius
            radius_text = f"Circle radius: {self.radius} pixels"
            cv2.putText(display_frame, radius_text, (10, y_offset + 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            
            cv2.imshow(window_name, display_frame)
            
            key = cv2.waitKey(1) & 0xFF
            
            if key == 13:  # Enter
                if self.center:
                    cv2.destroyWindow(window_name)
                    return TrackingCircle(self.center, self.radius)
                    
            elif key == ord('r') or key == ord('R'):
                self.frame = self.original_frame.copy()
                cv2.line(self.frame, self.calibration.point1, 
                        self.calibration.point2, (0, 255, 0), 2)
                self.center = None
                
            elif key == ord('q') or key == ord('Q'):
                cv2.destroyWindow(window_name)
                return None
        
        return None


class OSSMTracker:
    """Main tracker class that processes video and tracks the selected point."""
    
    def __init__(self, video_path: str):
        self.video_path = Path(video_path)
        if not self.video_path.exists():
            raise FileNotFoundError(f"Video file not found: {video_path}")
        
        # Try to open with default backend first, then try AVFoundation for iPhone videos
        self.cap = cv2.VideoCapture(str(self.video_path))
        if not self.cap.isOpened():
            # Try AVFoundation backend (better for iPhone HEVC videos on macOS)
            self.cap = cv2.VideoCapture(str(self.video_path), cv2.CAP_AVFOUNDATION)
        if not self.cap.isOpened():
            raise ValueError(
                f"Could not open video: {video_path}\n"
                "If this is an iPhone HEVC video, try converting to H.264:\n"
                "  ffmpeg -i input.mov -c:v libx264 -crf 18 output.mp4"
            )
        
        self.fps = self.cap.get(cv2.CAP_PROP_FPS)
        self.frame_count = int(self.cap.get(cv2.CAP_PROP_FRAME_COUNT))
        self.frame_width = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.frame_height = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        
        # Validate we got reasonable values
        if self.fps <= 0 or self.frame_count <= 0:
            raise ValueError(
                f"Could not read video properties (fps={self.fps}, frames={self.frame_count}).\n"
                "The video codec may not be supported. Try converting to H.264."
            )
        
        print(f"Video loaded: {self.video_path.name}")
        print(f"  Resolution: {self.frame_width}x{self.frame_height}")
        print(f"  FPS: {self.fps}")
        print(f"  Frames: {self.frame_count}")
        print(f"  Duration: {self.frame_count / self.fps:.2f} seconds")
        
        # Warn about high frame rates (likely slo-mo) and variable frame rate
        if self.fps >= 100:
            print(f"\n  NOTE: High frame rate detected ({self.fps} fps)")
            print("  This appears to be a slow-motion video.")
            print("  If this is an iPhone slo-mo video, be aware that:")
            print("    - Time calculations assume constant frame rate")
            print("    - Variable frame rate sections may have timing inaccuracies")
            print("    - For best results, trim to just the slo-mo portion")
        
        self.calibration: Optional[CalibrationData] = None
        self.tracking_circle: Optional[TrackingCircle] = None
        
    def get_first_frame(self) -> np.ndarray:
        """Get the first frame of the video."""
        self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        ret, frame = self.cap.read()
        if not ret:
            raise ValueError("Could not read first frame")
        return frame
    
    def calibrate(self, fixed_length_mm: Optional[float] = None) -> bool:
        """
        Run the calibration process.
        
        If fixed_length_mm is provided, use that value instead of asking the user.
        """
        frame = self.get_first_frame()
        
        # Get calibration line
        calibration_ui = CalibrationUI(frame)
        line_points = calibration_ui.run()
        
        if line_points is None:
            print("Calibration cancelled")
            return False
        
        # Get length in mm (or use fixed value)
        if fixed_length_mm is not None:
            length_mm = fixed_length_mm
            print(f"\nUsing fixed calibration length: {length_mm} mm")
        else:
            print("\n" + "=" * 50)
            try:
                length_mm = float(input("Enter the real length of the line in mm: "))
            except (ValueError, KeyboardInterrupt):
                print("Invalid input or cancelled")
                return False
            
            if length_mm <= 0:
                print("Length must be positive")
                return False
        
        self.calibration = CalibrationData(
            point1=line_points[0],
            point2=line_points[1],
            length_mm=length_mm
        )
        
        print(f"\nCalibration complete:")
        print(f"  Pixel length: {self.calibration.pixel_length:.1f} pixels")
        print(f"  Real length: {length_mm} mm")
        print(f"  Scale: {self.calibration.mm_per_pixel:.4f} mm/pixel")
        print("=" * 50 + "\n")
        
        return True
    
    def select_tracking_point(self) -> bool:
        """Run the tracking point selection process."""
        if self.calibration is None:
            print("Must calibrate first")
            return False
        
        frame = self.get_first_frame()
        tracking_ui = TrackingPointUI(frame, self.calibration)
        self.tracking_circle = tracking_ui.run()
        
        if self.tracking_circle is None:
            print("Tracking point selection cancelled")
            return False
        
        print(f"Tracking point selected at {self.tracking_circle.center}")
        print(f"  Radius: {self.tracking_circle.radius} pixels")
        
        return True
    
    def project_to_axis(self, point: tuple[int, int]) -> float:
        """Project a point onto the calibration axis and return position in mm."""
        if self.calibration is None:
            raise ValueError("Not calibrated")
        
        # Vector from calibration point1 to the tracked point
        px = point[0] - self.calibration.point1[0]
        py = point[1] - self.calibration.point1[1]
        
        # Unit vector along calibration line
        angle = self.calibration.angle
        ux = math.cos(angle)
        uy = math.sin(angle)
        
        # Project onto the line (dot product)
        projection_pixels = px * ux + py * uy
        
        # Convert to mm
        return projection_pixels * self.calibration.mm_per_pixel
    
    def track_frame(self, frame: np.ndarray, prev_center: tuple[int, int], 
                   radius: int) -> Optional[tuple[int, int]]:
        """
        Track the point in a single frame using template matching 
        within a search region around the previous position.
        """
        # Convert to grayscale
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Define search region (larger than tracking circle)
        search_margin = radius * 3
        x1 = max(0, prev_center[0] - search_margin)
        y1 = max(0, prev_center[1] - search_margin)
        x2 = min(frame.shape[1], prev_center[0] + search_margin)
        y2 = min(frame.shape[0], prev_center[1] + search_margin)
        
        search_region = gray[y1:y2, x1:x2]
        
        # Use template matching with a circle template
        template_size = radius * 2 + 1
        template = np.zeros((template_size, template_size), dtype=np.uint8)
        cv2.circle(template, (radius, radius), radius, 255, -1)
        
        # Match template
        if search_region.shape[0] >= template_size and search_region.shape[1] >= template_size:
            result = cv2.matchTemplate(search_region, template, cv2.TM_CCOEFF_NORMED)
            _, max_val, _, max_loc = cv2.minMaxLoc(result)
            
            if max_val > 0.3:  # Threshold for acceptable match
                # Convert back to full frame coordinates
                match_x = x1 + max_loc[0] + radius
                match_y = y1 + max_loc[1] + radius
                return (match_x, match_y)
        
        # If template matching fails, return None (tracking lost)
        return None
    
    def run_tracking(self) -> list[dict]:
        """Run the tracking process on the entire video at maximum speed."""
        if self.calibration is None or self.tracking_circle is None:
            raise ValueError("Must calibrate and select tracking point first")
        
        self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        
        results = []
        current_center = self.tracking_circle.center
        radius = self.tracking_circle.radius
        
        frame_num = 0
        lost_frames = 0
        
        print("\nProcessing video at maximum speed...")
        print(f"  Total frames: {self.frame_count}")
        
        import time
        start_time = time.time()
        
        while True:
            ret, frame = self.cap.read()
            if not ret:
                break
            
            # Track the point
            new_center = self.track_frame(frame, current_center, radius)
            
            if new_center is not None:
                current_center = new_center
                
                # Calculate position
                position_mm = self.project_to_axis(current_center)
                time_s = frame_num / self.fps
                
                results.append({
                    'frame': frame_num,
                    'time_s': time_s,
                    'position_mm': position_mm,
                    'raw_x': current_center[0],
                    'raw_y': current_center[1]
                })
            else:
                # Tracking lost - record with None
                lost_frames += 1
                results.append({
                    'frame': frame_num,
                    'time_s': frame_num / self.fps,
                    'position_mm': None,
                    'raw_x': None,
                    'raw_y': None
                })
            
            frame_num += 1
            
            # Print progress every 500 frames
            if frame_num % 500 == 0:
                elapsed = time.time() - start_time
                fps_rate = frame_num / elapsed
                eta = (self.frame_count - frame_num) / fps_rate if fps_rate > 0 else 0
                print(f"  {frame_num}/{self.frame_count} frames ({fps_rate:.0f} fps, ETA: {eta:.1f}s)")
        
        elapsed = time.time() - start_time
        fps_rate = frame_num / elapsed if elapsed > 0 else 0
        
        print(f"\nTracking complete:")
        print(f"  {len(results)} frames in {elapsed:.1f}s ({fps_rate:.0f} fps)")
        if lost_frames > 0:
            print(f"  Lost tracking on {lost_frames} frames")
        
        return results
    
    def save_results(self, results: list[dict], output_path: Optional[str] = None) -> str:
        """Save tracking results to CSV."""
        if output_path is None:
            output_path = self.video_path.stem + "_tracking_data.csv"
        
        output_path = Path(output_path)
        
        with open(output_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=['frame', 'time_s', 'position_mm', 'raw_x', 'raw_y'])
            writer.writeheader()
            writer.writerows(results)
        
        print(f"\nResults saved to: {output_path}")
        
        # Print summary statistics
        valid_positions = [r['position_mm'] for r in results if r['position_mm'] is not None]
        if valid_positions:
            print(f"\nSummary:")
            print(f"  Valid frames: {len(valid_positions)}/{len(results)}")
            print(f"  Position range: {min(valid_positions):.2f} to {max(valid_positions):.2f} mm")
            print(f"  Total travel: {max(valid_positions) - min(valid_positions):.2f} mm")
        
        return str(output_path)
    
    def close(self) -> None:
        """Release video capture resources."""
        self.cap.release()
        cv2.destroyAllWindows()


VIDEO_EXTENSIONS = {'.mp4', '.mov', '.avi', '.mkv', '.webm', '.m4v', '.hevc'}


def find_videos_in_directory(directory: Path) -> list[Path]:
    """Find all video files in a directory."""
    videos = []
    for ext in VIDEO_EXTENSIONS:
        videos.extend(directory.glob(f'*{ext}'))
        videos.extend(directory.glob(f'*{ext.upper()}'))
    return sorted(videos, key=lambda p: p.name.lower())


def process_single_video(video_path: Path, output_path: Optional[str] = None, 
                         calibration_length_mm: Optional[float] = None) -> Optional[float]:
    """
    Process a single video file.
    
    Always draws calibration line and tracking point.
    If calibration_length_mm is provided, uses that value (no prompt).
    Returns the calibration length in mm (for reuse with next video).
    """
    try:
        tracker = OSSMTracker(str(video_path))
        
        # Always draw calibration line
        # First video: ask for mm, subsequent: use fixed value
        if not tracker.calibrate(fixed_length_mm=calibration_length_mm):
            tracker.close()
            return calibration_length_mm
        calibration_length_mm = tracker.calibration.length_mm
        
        # Always select tracking point
        if not tracker.select_tracking_point():
            tracker.close()
            return calibration_length_mm
        
        # Run tracking
        results = tracker.run_tracking()
        
        # Save results
        if results:
            tracker.save_results(results, output_path)
        else:
            print("No results to save")
        
        tracker.close()
        return calibration_length_mm
        
    except (FileNotFoundError, ValueError) as e:
        print(f"Error processing {video_path.name}: {e}")
        return calibration_length_mm


def main():
    parser = argparse.ArgumentParser(
        description="OSSM Video Analysis Tool - Track motion from video"
    )
    parser.add_argument(
        "path",
        nargs="?",
        help="Path to video file or directory containing videos (processes ALL videos in directory)"
    )
    parser.add_argument(
        "-o", "--output",
        help="Output CSV file path (only used for single file mode)"
    )
    
    args = parser.parse_args()
    
    # Get input path
    input_path = args.path
    if input_path is None:
        input_path = input("Enter path to video file or directory: ").strip()
        if not input_path:
            print("No path provided")
            sys.exit(1)
    
    # Remove quotes if present
    input_path = Path(input_path.strip('"\''))
    
    try:
        # Handle directory input - process ALL videos
        if input_path.is_dir():
            videos = find_videos_in_directory(input_path)
            
            if not videos:
                print(f"No video files found in: {input_path}")
                print(f"Supported formats: {', '.join(VIDEO_EXTENSIONS)}")
                sys.exit(1)
            
            print(f"\n{'='*60}")
            print(f"BATCH MODE: Processing {len(videos)} video(s)")
            print(f"{'='*60}")
            for i, video in enumerate(videos, 1):
                size_mb = video.stat().st_size / (1024 * 1024)
                print(f"  {i}. {video.name} ({size_mb:.1f} MB)")
            print(f"{'='*60}\n")
            
            print("Each video: Draw calibration line (green) + tracking point (red)")
            print("First video only: Enter mm dimension for the line.\n")
            
            calibration_length_mm = None
            
            for i, video_path in enumerate(videos, 1):
                print(f"\n{'='*60}")
                print(f"VIDEO {i}/{len(videos)}: {video_path.name}")
                print(f"{'='*60}\n")
                
                calibration_length_mm = process_single_video(
                    video_path, 
                    output_path=None,  # Use default naming
                    calibration_length_mm=calibration_length_mm
                )
                
                if calibration_length_mm is None:
                    print("\nCalibration cancelled. Stopping batch.")
                    break
            
            print(f"\n{'='*60}")
            print(f"BATCH COMPLETE: Processed {i} video(s)")
            print(f"{'='*60}")
        
        else:
            # Single file mode
            process_single_video(input_path, args.output)
        
    except KeyboardInterrupt:
        print("\nCancelled by user")
        cv2.destroyAllWindows()
        sys.exit(1)


if __name__ == "__main__":
    main()
