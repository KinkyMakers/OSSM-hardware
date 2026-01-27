"""Main LiveTracker class with AprilTag-based tracking."""

import csv
import math
import threading
import time
from datetime import datetime
from typing import Optional

import cv2
import numpy as np
from IPython.display import display, FileLink
from ipycanvas import MultiCanvas, hold_canvas
from ipyevents import Event
from ipywidgets import (
    Dropdown, Button, FloatText, HBox, VBox,
    Output, Label, HTML, Layout, Checkbox
)

from .types import CalibrationData, AprilTagConfig, DetectedTag, TrackerMode
from .tracking import (
    project_to_axis, list_cameras, create_apriltag_detector, 
    detect_apriltags, track_apriltag_frame
)


class LiveTracker:
    """
    Live OSSM position tracker with Jupyter notebook UI.
    
    Usage:
        tracker = LiveTracker()
        tracker.show()
    """
    
    # Canvas settings
    DEFAULT_WIDTH = 640
    DEFAULT_HEIGHT = 480
    TARGET_FPS = 30
    
    # Step states
    STEP_CAMERA = 1
    STEP_CALIBRATE = 2
    STEP_POINTS = 3
    STEP_TRACK = 4
    STEP_EXPORT = 5
    
    def __init__(self):
        # State
        self.current_step = self.STEP_CAMERA
        self.mode = TrackerMode.IDLE
        self.calibration: Optional[CalibrationData] = None
        self.positions: list[dict] = []
        self.recording_active = False  # Recording data
        self.preview_active = False
        
        # AprilTag detection
        self._apriltag_config = AprilTagConfig()
        self._apriltag_detector = create_apriltag_detector(self._apriltag_config)
        self._detected_tags: list[DetectedTag] = []  # Tags detected in current frame
        self._tracked_tag_ids: set[int] = set()  # Tag IDs selected for tracking
        self._tag_checkboxes: dict[int, Checkbox] = {}  # UI checkboxes for tag selection
        
        # Canvas dimensions (fixed size)
        self.canvas_width = self.DEFAULT_WIDTH
        self.canvas_height = self.DEFAULT_HEIGHT
        
        # Camera diagnostics
        self._actual_width = 0
        self._actual_height = 0
        self._actual_fps = 0
        self._frame_count = 0
        self._fps_timer = time.time()
        self._measured_fps = 0.0
        
        # Frame scaling info (set when first frame arrives)
        self._frame_scale = 1.0
        self._frame_offset_x = 0
        self._frame_offset_y = 0
        
        # Calibration: two draggable points (canvas coordinates)
        self._cal_point1: Optional[tuple[int, int]] = None
        self._cal_point2: Optional[tuple[int, int]] = None
        self._dragging_cal_point: Optional[int] = None
        
        # Debug
        self._debug_mouse_pos: Optional[tuple[int, int]] = None
        
        # Camera
        self._cap: Optional[cv2.VideoCapture] = None
        self._last_frame: Optional[np.ndarray] = None
        
        # Build UI
        self._build_ui()
        self._connect_events()
        self._update_step_styles()
    
    def _build_ui(self):
        """Build all UI components."""
        # Canvas with EXPLICIT CSS dimensions to prevent browser stretching
        # Use outline instead of border to avoid affecting element dimensions
        self._canvas = MultiCanvas(2, width=self.canvas_width, height=self.canvas_height)
        self._canvas.layout = Layout(
            width=f'{self.canvas_width}px',
            height=f'{self.canvas_height}px',
            min_width=f'{self.canvas_width}px',
            min_height=f'{self.canvas_height}px',
            max_width=f'{self.canvas_width}px',
            max_height=f'{self.canvas_height}px',
        )
        self._video_layer = self._canvas[0]
        self._overlay_layer = self._canvas[1]
        
        # Initial canvas state
        self._video_layer.fill_style = '#1a1a2e'
        self._video_layer.fill_rect(0, 0, self.canvas_width, self.canvas_height)
        self._draw_welcome()
        
        # Step 1: Camera
        self._camera_dropdown = Dropdown(
            options=list_cameras(),
            description='',
            layout=Layout(width='180px')
        )
        self._preview_btn = Button(
            description='Start', 
            button_style='info',
            layout=Layout(width='70px')
        )
        self._camera_next_btn = Button(
            description='Next →',
            button_style='success',
            disabled=True,
            layout=Layout(width='70px')
        )
        self._camera_diag = HTML('<span style="color: #666; font-size: 11px;">—</span>')
        self._step1_header = HTML('<b style="color: #4CAF50;">1. Camera</b>')
        self._step1 = VBox([
            self._step1_header,
            HBox([self._camera_dropdown, self._preview_btn]),
            self._camera_diag,
            self._camera_next_btn
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Step 2: Calibration
        self._cal_length = FloatText(
            value=200.0,
            description='',
            layout=Layout(width='80px')
        )
        self._cal_done_btn = Button(
            description='Done ✓',
            button_style='success',
            disabled=True,
            layout=Layout(width='80px')
        )
        self._cal_clear_btn = Button(
            description='Clear',
            button_style='warning',
            layout=Layout(width='60px')
        )
        self._cal_status = HTML('<span style="color: #888;">Click to place 2 points</span>')
        self._step2_header = HTML('<b style="color: #2196F3;">2. Calibrate</b>')
        self._step2 = VBox([
            self._step2_header,
            HBox([Label('Length (mm):'), self._cal_length]),
            self._cal_status,
            HBox([self._cal_clear_btn, self._cal_done_btn])
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Step 3: Tag Selection (AprilTags)
        self._tags_container = VBox([], layout=Layout(max_height='120px', overflow_y='auto'))
        self._tags_select_all_btn = Button(
            description='Select All',
            button_style='info',
            layout=Layout(width='80px')
        )
        self._tags_clear_btn = Button(
            description='Clear',
            button_style='warning',
            layout=Layout(width='60px')
        )
        self._tags_next_btn = Button(
            description='Next →',
            button_style='success',
            disabled=True,
            layout=Layout(width='70px')
        )
        self._tags_status = HTML('<span style="color: #888;">Searching for AprilTags...</span>')
        self._step3_header = HTML('<b style="color: #FF9800;">3. Select Tags</b>')
        self._step3 = VBox([
            self._step3_header,
            HTML('<span style="font-size: 11px; color: #888;">tagStandard41h12 family</span>'),
            self._tags_status,
            self._tags_container,
            HBox([self._tags_select_all_btn, self._tags_clear_btn, self._tags_next_btn])
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Step 4: Record
        self._track_go_btn = Button(
            description='● Record',
            button_style='success',
            disabled=True,
            layout=Layout(width='80px')
        )
        self._track_stop_btn = Button(
            description='■ Stop',
            button_style='danger',
            disabled=True,
            layout=Layout(width='80px')
        )
        self._track_status = HTML('<span style="color: #888;">Ready to record</span>')
        self._step4_header = HTML('<b style="color: #E91E63;">4. Record</b>')
        self._step4 = VBox([
            self._step4_header,
            self._track_status,
            HBox([self._track_go_btn, self._track_stop_btn])
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Step 5: Export
        self._export_status = HTML('<span style="color: #888;">—</span>')
        self._reset_btn = Button(
            description='Start Over',
            button_style='',
            layout=Layout(width='90px')
        )
        self._step5_header = HTML('<b style="color: #9C27B0;">5. Export</b>')
        self._step5 = VBox([
            self._step5_header,
            self._export_status,
            self._reset_btn
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Status/log output
        self._output = Output(layout=Layout(
            height='60px', 
            overflow='auto',
            border='1px solid #333',
            border_radius='5px',
            padding='5px'
        ))
        
        # Controls panel
        self._controls = VBox([
            self._step1, self._step2, self._step3, self._step4, self._step5,
            HTML('<hr style="border-color: #333; margin: 5px 0;">'),
            self._output
        ], layout=Layout(width='280px', padding='10px'))
        
        # Main layout
        self._main_ui = HBox([
            self._canvas,
            self._controls
        ], layout=Layout(gap='10px'))
        
    
    def _connect_events(self):
        """Connect all event handlers."""
        # Use ipycanvas native mouse events for reliable coordinates
        self._canvas.on_mouse_down(self._on_canvas_mouse_down)
        self._canvas.on_mouse_up(self._on_canvas_mouse_up)
        self._canvas.on_mouse_move(self._on_canvas_mouse_move)
        
        # Keyboard events (still need ipyevents for keyboard)
        self._key_event = Event(
            source=self._canvas, 
            watched_events=['keydown', 'keyup'],
            prevent_default_action=True
        )
        self._key_event.on_dom_event(self._handle_key)
        
        # Button clicks
        self._preview_btn.on_click(self._on_preview)
        self._camera_next_btn.on_click(self._on_camera_next)
        self._cal_clear_btn.on_click(self._on_cal_clear)
        self._cal_done_btn.on_click(self._on_cal_done)
        self._tags_select_all_btn.on_click(self._on_tags_select_all)
        self._tags_clear_btn.on_click(self._on_tags_clear)
        self._tags_next_btn.on_click(self._on_tags_next)
        self._track_go_btn.on_click(self._on_track_go)
        self._track_stop_btn.on_click(self._on_track_stop)
        self._reset_btn.on_click(self._on_reset)
    
    def _update_step_styles(self):
        """Update step panel styles based on current step."""
        steps = [
            (self._step1, self._step1_header, "1. Camera", "#4CAF50"),
            (self._step2, self._step2_header, "2. Calibrate", "#2196F3"),
            (self._step3, self._step3_header, "3. Track Points", "#FF9800"),
            (self._step4, self._step4_header, "4. Record", "#E91E63"),
            (self._step5, self._step5_header, "5. Export", "#9C27B0"),
        ]
        
        for i, (step_box, header, title, color) in enumerate(steps, 1):
            if i == self.current_step:
                step_box.layout.border = f'2px solid {color}'
                step_box.layout.opacity = '1'
                header.value = f'<b style="color: {color};">▶ {title}</b>'
            elif i < self.current_step:
                step_box.layout.border = '1px solid #4CAF50'
                step_box.layout.opacity = '0.7'
                header.value = f'<b style="color: #4CAF50;">✓ {title}</b>'
            else:
                step_box.layout.border = '1px solid #333'
                step_box.layout.opacity = '0.4'
                header.value = f'<b style="color: #666;">{title}</b>'
    
    def _log(self, msg: str):
        """Log message to output widget."""
        with self._output:
            print(msg)
    
    def _draw_welcome(self):
        """Draw welcome message on canvas."""
        self._overlay_layer.clear()
        self._overlay_layer.fill_style = '#666'
        self._overlay_layer.font = '16px sans-serif'
        self._overlay_layer.text_align = 'center'
        self._overlay_layer.fill_text(
            'Select a camera and click Start',
            self.canvas_width // 2, 
            self.canvas_height // 2
        )
        self._overlay_layer.text_align = 'left'
    
    def _frame_to_rgba(self, frame: np.ndarray) -> np.ndarray:
        """Convert BGR frame to RGBA for canvas, maintaining aspect ratio."""
        frame_h, frame_w = frame.shape[:2]
        
        # Calculate scale to fit canvas while maintaining aspect ratio
        scale = min(self.canvas_width / frame_w, self.canvas_height / frame_h)
        
        # Calculate display dimensions
        display_w = int(frame_w * scale)
        display_h = int(frame_h * scale)
        
        # Calculate centering offset
        offset_x = (self.canvas_width - display_w) // 2
        offset_y = (self.canvas_height - display_h) // 2
        
        # Store for coordinate conversion
        self._frame_scale = scale
        self._frame_offset_x = offset_x
        self._frame_offset_y = offset_y
        
        # Resize frame to display size
        resized = cv2.resize(frame, (display_w, display_h), interpolation=cv2.INTER_LINEAR)
        
        # Create output canvas (black background)
        output = np.zeros((self.canvas_height, self.canvas_width, 4), dtype=np.uint8)
        output[:, :, 3] = 255  # Fully opaque
        
        # Convert resized frame to RGBA and place centered
        rgba = cv2.cvtColor(resized, cv2.COLOR_BGR2RGBA)
        output[offset_y:offset_y + display_h, offset_x:offset_x + display_w] = rgba
        
        return output
    
    def _canvas_to_frame(self, cx: int, cy: int) -> tuple[int, int]:
        """Convert canvas coordinates to original frame coordinates."""
        fx = int((cx - self._frame_offset_x) / self._frame_scale)
        fy = int((cy - self._frame_offset_y) / self._frame_scale)
        return (fx, fy)
    
    def _frame_to_canvas(self, fx: int, fy: int) -> tuple[int, int]:
        """Convert original frame coordinates to canvas coordinates."""
        cx = int(fx * self._frame_scale + self._frame_offset_x)
        cy = int(fy * self._frame_scale + self._frame_offset_y)
        return (cx, cy)
    
    def _update_camera_diag(self):
        """Update camera diagnostics display."""
        if self._actual_width > 0:
            gcd = math.gcd(self._actual_width, self._actual_height)
            ratio = f"{self._actual_width // gcd}:{self._actual_height // gcd}"
            self._camera_diag.value = (
                f'<span style="color: #4CAF50; font-size: 11px;">'
                f'{self._actual_width}×{self._actual_height} ({ratio}) | '
                f'{self._measured_fps:.1f} FPS</span>'
            )
        else:
            self._camera_diag.value = '<span style="color: #666; font-size: 11px;">—</span>'
    
    def _draw_overlay(self):
        """Draw overlay based on current state."""
        self._overlay_layer.clear()
        
        # Calibration points (two draggable points)
        if self._cal_point1:
            self._draw_cal_point(self._cal_point1, "1", self._dragging_cal_point == 1)
        if self._cal_point2:
            self._draw_cal_point(self._cal_point2, "2", self._dragging_cal_point == 2)
        
        # Calibration line between points
        if self._cal_point1 and self._cal_point2:
            self._draw_line(self._cal_point1, self._cal_point2, '#00ff00', 2)
            # Show pixel length
            dx = self._cal_point2[0] - self._cal_point1[0]
            dy = self._cal_point2[1] - self._cal_point1[1]
            px_len = math.sqrt(dx*dx + dy*dy) / self._frame_scale if self._frame_scale > 0 else 0
            mid_x = (self._cal_point1[0] + self._cal_point2[0]) // 2
            mid_y = (self._cal_point1[1] + self._cal_point2[1]) // 2
            self._overlay_layer.fill_style = 'white'
            self._overlay_layer.font = '12px sans-serif'
            self._overlay_layer.fill_text(f'{px_len:.0f} px', mid_x + 5, mid_y - 5)
        
        # Confirmed calibration (shown when past calibration step)
        if self.calibration and self.current_step > self.STEP_CALIBRATE:
            p1 = self._frame_to_canvas(*self.calibration.point1)
            p2 = self._frame_to_canvas(*self.calibration.point2)
            self._draw_line(p1, p2, '#00ff00', 2)
            self._draw_circle(p1, 6, '#00ff00', fill=True)
            self._draw_circle(p2, 6, '#00ff00', fill=True)
        
        # Draw detected AprilTags
        for tag in self._detected_tags:
            is_tracked = tag.tag_id in self._tracked_tag_ids
            color = '#00ffff' if is_tracked else '#ff9800'
            
            # Convert frame coordinates to canvas coordinates
            canvas_corners = [self._frame_to_canvas(int(c[0]), int(c[1])) for c in tag.corners]
            canvas_center = self._frame_to_canvas(*tag.center_int)
            
            # Draw quadrilateral outline
            self._draw_tag_quad(canvas_corners, color)
            
            # Draw center cross
            self._draw_cross(canvas_center, 8, color)
            
            # Draw tag ID label
            self._overlay_layer.fill_style = color
            self._overlay_layer.font = 'bold 14px sans-serif'
            label = f"ID:{tag.tag_id}"
            # Position label above the tag
            label_y = min(c[1] for c in canvas_corners) - 8
            label_x = canvas_center[0] - 15
            self._overlay_layer.fill_text(label, label_x, label_y)
            
            # Show quality indicator for tracked tags
            if is_tracked:
                quality = f"{tag.decision_margin:.0f}"
                self._overlay_layer.font = '10px sans-serif'
                self._overlay_layer.fill_text(quality, canvas_center[0] + 12, canvas_center[1] + 4)
        
        # Average marker for multiple tracked tags
        tracked_tags = [t for t in self._detected_tags if t.tag_id in self._tracked_tag_ids]
        if len(tracked_tags) > 1:
            avg_x = sum(t.center[0] for t in tracked_tags) / len(tracked_tags)
            avg_y = sum(t.center[1] for t in tracked_tags) / len(tracked_tags)
            canvas_avg = self._frame_to_canvas(int(avg_x), int(avg_y))
            self._draw_cross(canvas_avg, 12, '#ffff00')
        
        # Mode indicator
        mode_messages = {
            self.STEP_CAMERA: '',
            self.STEP_CALIBRATE: 'Click to place points, drag to adjust',
            self.STEP_POINTS: f'AprilTags detected: {len(self._detected_tags)} | Selected: {len(self._tracked_tag_ids)}',
            self.STEP_TRACK: 'RECORDING' if self.recording_active else 'Press Record to start',
            self.STEP_EXPORT: '',
        }
        msg = mode_messages.get(self.current_step, '')
        if msg:
            self._overlay_layer.fill_style = 'white'
            self._overlay_layer.font = 'bold 14px sans-serif'
            self._overlay_layer.fill_text(msg, 10, 20)
        
        # Position readout during tracking
        if self.recording_active and self.positions:
            last_pos = self.positions[-1]
            self._overlay_layer.fill_style = '#00ffff'
            self._overlay_layer.font = 'bold 24px sans-serif'
            self._overlay_layer.fill_text(
                f"Position: {last_pos['position_mm']:.1f} mm",
                10, self.canvas_height - 15
            )
            self._overlay_layer.font = '14px sans-serif'
            tags_info = f"Tags: {last_pos.get('tags_tracked', 0)}"
            self._overlay_layer.fill_text(
                f"Frame: {len(self.positions)} | {tags_info}",
                10, self.canvas_height - 40
            )
        
        # Debug: Draw crosshair at mouse position
        if self._debug_mouse_pos and self.current_step in (self.STEP_CALIBRATE, self.STEP_POINTS):
            mx, my = self._debug_mouse_pos
            self._overlay_layer.stroke_style = '#ff00ff'
            self._overlay_layer.line_width = 1
            self._overlay_layer.set_line_dash([4, 4])
            # Horizontal line
            self._overlay_layer.begin_path()
            self._overlay_layer.move_to(0, my)
            self._overlay_layer.line_to(self.canvas_width, my)
            self._overlay_layer.stroke()
            # Vertical line
            self._overlay_layer.begin_path()
            self._overlay_layer.move_to(mx, 0)
            self._overlay_layer.line_to(mx, self.canvas_height)
            self._overlay_layer.stroke()
            self._overlay_layer.set_line_dash([])
            # Coordinate display
            self._overlay_layer.fill_style = '#ff00ff'
            self._overlay_layer.font = '11px monospace'
            self._overlay_layer.fill_text(f'({mx}, {my})', mx + 5, my - 5)
    
    def _draw_cal_point(self, pos: tuple, label: str, active: bool):
        """Draw a calibration point marker."""
        color = '#00ffff' if active else '#00ff00'
        # Outer circle
        self._draw_circle(pos, 10, color, fill=False)
        # Inner dot
        self._draw_circle(pos, 4, color, fill=True)
        # Label
        self._overlay_layer.fill_style = color
        self._overlay_layer.font = 'bold 14px sans-serif'
        self._overlay_layer.fill_text(label, pos[0] + 14, pos[1] + 5)
    
    def _draw_line(self, p1: tuple, p2: tuple, color: str, width: int):
        """Draw a line on overlay."""
        self._overlay_layer.stroke_style = color
        self._overlay_layer.line_width = width
        self._overlay_layer.begin_path()
        self._overlay_layer.move_to(*p1)
        self._overlay_layer.line_to(*p2)
        self._overlay_layer.stroke()
    
    def _draw_circle(self, center: tuple, radius: int, color: str, fill: bool):
        """Draw a circle on overlay."""
        if fill:
            self._overlay_layer.fill_style = color
        else:
            self._overlay_layer.stroke_style = color
            self._overlay_layer.line_width = 2
        self._overlay_layer.begin_path()
        self._overlay_layer.arc(center[0], center[1], radius, 0, 2 * math.pi)
        if fill:
            self._overlay_layer.fill()
        else:
            self._overlay_layer.stroke()
    
    def _draw_cross(self, center: tuple, size: int, color: str):
        """Draw a cross marker."""
        self._overlay_layer.stroke_style = color
        self._overlay_layer.line_width = 2
        self._overlay_layer.begin_path()
        self._overlay_layer.move_to(center[0] - size, center[1])
        self._overlay_layer.line_to(center[0] + size, center[1])
        self._overlay_layer.move_to(center[0], center[1] - size)
        self._overlay_layer.line_to(center[0], center[1] + size)
        self._overlay_layer.stroke()
    
    def _draw_tag_quad(self, corners: list[tuple[int, int]], color: str):
        """Draw a quadrilateral for an AprilTag."""
        if len(corners) != 4:
            return
        self._overlay_layer.stroke_style = color
        self._overlay_layer.line_width = 2
        self._overlay_layer.begin_path()
        self._overlay_layer.move_to(*corners[0])
        for corner in corners[1:]:
            self._overlay_layer.line_to(*corner)
        self._overlay_layer.close_path()
        self._overlay_layer.stroke()
    
    def _find_cal_point_at(self, x: int, y: int) -> Optional[int]:
        """Find which calibration point (1 or 2) is at position, or None."""
        if self._cal_point1:
            dist = math.sqrt((x - self._cal_point1[0])**2 + (y - self._cal_point1[1])**2)
            if dist <= 15:
                return 1
        if self._cal_point2:
            dist = math.sqrt((x - self._cal_point2[0])**2 + (y - self._cal_point2[1])**2)
            if dist <= 15:
                return 2
        return None
    
    def _update_cal_status(self):
        """Update calibration status display."""
        if self._cal_point1 and self._cal_point2:
            dx = self._cal_point2[0] - self._cal_point1[0]
            dy = self._cal_point2[1] - self._cal_point1[1]
            px_len = math.sqrt(dx*dx + dy*dy) / self._frame_scale if self._frame_scale > 0 else 0
            mm_per_px = self._cal_length.value / px_len if px_len > 0 else 0
            self._cal_status.value = (
                f'<span style="color: #4CAF50;">'
                f'{px_len:.0f}px = {self._cal_length.value}mm</span>'
            )
            self._cal_done_btn.disabled = px_len < 20
        elif self._cal_point1:
            self._cal_status.value = '<span style="color: #2196F3;">Click to place point 2</span>'
            self._cal_done_btn.disabled = True
        else:
            self._cal_status.value = '<span style="color: #888;">Click to place point 1</span>'
            self._cal_done_btn.disabled = True
    
    def _update_tags_status(self):
        """Update tag detection status display."""
        n_detected = len(self._detected_tags)
        n_selected = len(self._tracked_tag_ids)
        
        if n_detected > 0:
            if n_selected > 0:
                self._tags_status.value = f'<span style="color: #4CAF50;">{n_selected} of {n_detected} tags selected</span>'
                self._tags_next_btn.disabled = False
            else:
                self._tags_status.value = f'<span style="color: #FF9800;">{n_detected} tags detected - select to track</span>'
                self._tags_next_btn.disabled = True
        else:
            self._tags_status.value = '<span style="color: #888;">No AprilTags detected</span>'
            self._tags_next_btn.disabled = True
    
    def _update_tag_checkboxes(self):
        """Update the tag selection checkboxes based on detected tags."""
        detected_ids = {tag.tag_id for tag in self._detected_tags}
        
        # Remove checkboxes for tags no longer detected
        for tag_id in list(self._tag_checkboxes.keys()):
            if tag_id not in detected_ids:
                del self._tag_checkboxes[tag_id]
        
        # Add checkboxes for newly detected tags
        for tag in sorted(self._detected_tags, key=lambda t: t.tag_id):
            if tag.tag_id not in self._tag_checkboxes:
                cb = Checkbox(
                    value=False,
                    description=f'Tag {tag.tag_id}',
                    layout=Layout(width='auto'),
                    indent=False
                )
                cb.observe(lambda change, tid=tag.tag_id: self._on_tag_checkbox_change(tid, change), names='value')
                self._tag_checkboxes[tag.tag_id] = cb
        
        # Update container with sorted checkboxes
        sorted_checkboxes = [self._tag_checkboxes[tag_id] for tag_id in sorted(self._tag_checkboxes.keys())]
        self._tags_container.children = sorted_checkboxes
    
    def _on_tag_checkbox_change(self, tag_id: int, change):
        """Handle tag checkbox state change."""
        if change['new']:
            self._tracked_tag_ids.add(tag_id)
        else:
            self._tracked_tag_ids.discard(tag_id)
        self._update_tags_status()
    
    def _on_canvas_mouse_down(self, x: float, y: float):
        """Handle canvas mousedown - ipycanvas provides x, y directly."""
        pos = (int(x), int(y))
        self._handle_mouse_event('mousedown', pos)
    
    def _on_canvas_mouse_up(self, x: float, y: float):
        """Handle canvas mouseup - ipycanvas provides x, y directly."""
        pos = (int(x), int(y))
        self._handle_mouse_event('mouseup', pos)
    
    def _on_canvas_mouse_move(self, x: float, y: float):
        """Handle canvas mousemove - ipycanvas provides x, y directly."""
        pos = (int(x), int(y))
        self._handle_mouse_event('mousemove', pos)
    
    def _handle_mouse_event(self, event_type: str, pos: tuple[int, int]):
        """Handle mouse events with direct canvas coordinates."""
        x, y = pos
        
        # Debug: store mouse position for overlay
        self._debug_mouse_pos = pos
        
        # Step 2: Calibration - place and drag two points
        if self.current_step == self.STEP_CALIBRATE:
            if event_type == 'mousedown':
                # Check if clicking on existing point
                hit = self._find_cal_point_at(x, y)
                if hit:
                    self._dragging_cal_point = hit
                else:
                    # Place new point
                    if not self._cal_point1:
                        self._cal_point1 = pos
                    elif not self._cal_point2:
                        self._cal_point2 = pos
                    self._update_cal_status()
                self._draw_overlay()
            
            elif event_type == 'mousemove':
                if self._dragging_cal_point == 1:
                    self._cal_point1 = pos
                    self._update_cal_status()
                elif self._dragging_cal_point == 2:
                    self._cal_point2 = pos
                    self._update_cal_status()
                # Always redraw overlay for debug crosshair
                self._draw_overlay()
            
            elif event_type == 'mouseup':
                self._dragging_cal_point = None
        
        # Step 3: Tag selection - no mouse interaction needed (use checkboxes)
        elif self.current_step == self.STEP_POINTS:
            if event_type == 'mousemove':
                # Just update overlay for debug crosshair
                self._draw_overlay()
    
    def _handle_key(self, ev):
        """Handle keyboard events."""
        key = ev.get('key', '')
        event_type = ev.get('type', '')
        
        if event_type != 'keydown':
            return
        
        if key == 'Enter':
            if self.current_step == self.STEP_CAMERA and self.preview_active:
                self._on_camera_next(None)
            elif self.current_step == self.STEP_CALIBRATE and not self._cal_done_btn.disabled:
                self._on_cal_done(None)
            elif self.current_step == self.STEP_POINTS and not self._tags_next_btn.disabled:
                self._on_tags_next(None)
    
    # === Button Handlers ===
    
    def _on_preview(self, btn):
        """Toggle camera preview."""
        if self.preview_active:
            self.preview_active = False
            if self._cap:
                self._cap.release()
                self._cap = None
            btn.description = 'Start'
            btn.button_style = 'info'
            self._camera_next_btn.disabled = True
            self._camera_diag.value = '<span style="color: #666; font-size: 11px;">—</span>'
        else:
            cam_idx = self._camera_dropdown.value
            if cam_idx < 0:
                self._log("No camera available")
                return
            
            self._cap = cv2.VideoCapture(cam_idx)
            if not self._cap.isOpened():
                self._log(f"Failed to open camera {cam_idx}")
                return
            
            self._actual_width = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            self._actual_height = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            self._actual_fps = self._cap.get(cv2.CAP_PROP_FPS)
            
            self.preview_active = True
            btn.description = 'Stop'
            btn.button_style = 'warning'
            self._camera_next_btn.disabled = False
            
            self._fps_timer = time.time()
            self._frame_count = 0
            
            thread = threading.Thread(target=self._preview_loop, daemon=True)
            thread.start()
    
    def _on_camera_next(self, btn):
        """Advance from camera step to calibration."""
        if not self.preview_active:
            self._log("Start preview first")
            return
        self.current_step = self.STEP_CALIBRATE
        self._cal_point1 = None
        self._cal_point2 = None
        self.calibration = None
        self._update_cal_status()
        self._update_step_styles()
        self._draw_overlay()
    
    def _on_cal_clear(self, btn):
        """Clear calibration points."""
        self._cal_point1 = None
        self._cal_point2 = None
        self._update_cal_status()
        self._draw_overlay()
    
    def _on_cal_done(self, btn):
        """Confirm calibration and advance."""
        if not self._cal_point1 or not self._cal_point2:
            self._log("Place both calibration points")
            return
        
        # Convert canvas coords to frame coords
        frame_p1 = self._canvas_to_frame(*self._cal_point1)
        frame_p2 = self._canvas_to_frame(*self._cal_point2)
        
        self.calibration = CalibrationData(
            point1=frame_p1,
            point2=frame_p2,
            length_mm=self._cal_length.value
        )
        self._log(f"Calibration: {self.calibration.mm_per_pixel:.4f} mm/px")
        
        self.current_step = self.STEP_POINTS
        self._tracked_tag_ids = set()
        self._tag_checkboxes = {}
        self._update_step_styles()
        self._update_tags_status()
        self._draw_overlay()
    
    def _on_tags_select_all(self, btn):
        """Select all detected tags for tracking."""
        for tag in self._detected_tags:
            self._tracked_tag_ids.add(tag.tag_id)
            if tag.tag_id in self._tag_checkboxes:
                self._tag_checkboxes[tag.tag_id].value = True
        self._update_tags_status()
        self._draw_overlay()
    
    def _on_tags_clear(self, btn):
        """Clear all tag selections."""
        self._tracked_tag_ids.clear()
        for cb in self._tag_checkboxes.values():
            cb.value = False
        self._update_tags_status()
        self._draw_overlay()
    
    def _on_tags_next(self, btn):
        """Confirm tag selection and advance to tracking."""
        if not self._tracked_tag_ids:
            self._log("Select at least one tag to track")
            return
        
        self._log(f"Tracking tags: {sorted(self._tracked_tag_ids)}")
        self.current_step = self.STEP_TRACK
        self._track_go_btn.disabled = False
        self._track_stop_btn.disabled = True
        self._update_step_styles()
        self._draw_overlay()
    
    def _on_track_go(self, btn):
        """Start recording tracking data."""
        if not self.calibration:
            self._log("Complete calibration first")
            return
        
        if not self._tracked_tag_ids:
            self._log("Select at least one tag to track")
            return
        
        self._log(f"Starting recording - tracking tags: {sorted(self._tracked_tag_ids)}")
        
        self.positions = []
        self.recording_active = True
        
        self._track_go_btn.disabled = True
        self._track_stop_btn.disabled = False
        self._track_status.value = '<span style="color: #E91E63; font-weight: bold;">● RECORDING</span>'
        
        thread = threading.Thread(target=self._recording_loop, daemon=True)
        thread.start()
    
    def _on_track_stop(self, btn):
        """Stop recording and auto-save."""
        self.recording_active = False
        
        self._track_go_btn.disabled = False
        self._track_stop_btn.disabled = True
        
        if self.positions:
            n = len(self.positions)
            self._track_status.value = f'<span style="color: #4CAF50;">✓ {n} frames captured</span>'
            self._auto_save_data()
            self.current_step = self.STEP_EXPORT
        else:
            self._track_status.value = '<span style="color: #888;">No data recorded</span>'
        
        self._update_step_styles()
        self._draw_overlay()
    
    def _auto_save_data(self):
        """Automatically save tracking data to a dated file."""
        if not self.positions:
            return
        
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = f"tracking_data_{timestamp}.csv"
        
        fieldnames = ['frame', 'time_s', 'position_mm', 'raw_x', 'raw_y', 'tags_tracked', 'tag_ids']
        
        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(self.positions)
        
        self._export_status.value = f'<span style="color: #4CAF50;">✓ Saved: {filename}</span>'
        self._log(f"Data saved to {filename}")
        
        with self._output:
            display(FileLink(filename))
    
    def _on_reset(self, btn):
        """Reset everything and start over."""
        self.recording_active = False
        self.preview_active = False
        
        if self._cap:
            self._cap.release()
            self._cap = None
        
        self.current_step = self.STEP_CAMERA
        self.calibration = None
        self.positions = []
        self._cal_point1 = None
        self._cal_point2 = None
        self._dragging_cal_point = None
        
        # Reset AprilTag state
        self._detected_tags = []
        self._tracked_tag_ids = set()
        self._tag_checkboxes = {}
        self._tags_container.children = []
        
        self._preview_btn.description = 'Start'
        self._preview_btn.button_style = 'info'
        self._camera_next_btn.disabled = True
        self._camera_diag.value = '<span style="color: #666; font-size: 11px;">—</span>'
        self._cal_status.value = '<span style="color: #888;">Click to place 2 points</span>'
        self._cal_done_btn.disabled = True
        self._tags_status.value = '<span style="color: #888;">Searching for AprilTags...</span>'
        self._tags_next_btn.disabled = True
        self._track_status.value = '<span style="color: #888;">Ready to record</span>'
        self._track_go_btn.disabled = True
        self._track_stop_btn.disabled = True
        self._export_status.value = '<span style="color: #888;">—</span>'
        
        self._update_step_styles()
        
        self._video_layer.fill_style = '#1a1a2e'
        self._video_layer.fill_rect(0, 0, self.canvas_width, self.canvas_height)
        self._draw_welcome()
        
        self._output.clear_output()
    
    # === Background Loops ===
    
    def _preview_loop(self):
        """Background preview loop with AprilTag detection."""
        while self.preview_active and not self.recording_active:
            if self._cap and self._cap.isOpened():
                ret, frame = self._cap.read()
                if ret:
                    self._last_frame = frame
                    
                    # FPS tracking
                    self._frame_count += 1
                    elapsed = time.time() - self._fps_timer
                    if elapsed >= 1.0:
                        self._measured_fps = self._frame_count / elapsed
                        self._frame_count = 0
                        self._fps_timer = time.time()
                        self._update_camera_diag()
                    
                    # Detect AprilTags in current frame
                    self._detected_tags = detect_apriltags(self._apriltag_detector, frame)
                    
                    # Update tag checkboxes if in tag selection step
                    if self.current_step == self.STEP_POINTS:
                        self._update_tag_checkboxes()
                        self._update_tags_status()
                    
                    with hold_canvas(self._canvas):
                        self._video_layer.put_image_data(self._frame_to_rgba(frame), 0, 0)
                        self._draw_overlay()
            
            time.sleep(1 / self.TARGET_FPS)
    
    def _recording_loop(self):
        """Background loop for recording tracking data using AprilTag detection."""
        frame_count = 0
        start_time = time.time()
        
        # Wait for preview loop to stop
        time.sleep(0.1)
        
        self._log(f"Recording started")
        
        while self.recording_active:
            if self._cap and self._cap.isOpened():
                ret, frame = self._cap.read()
                if ret:
                    self._last_frame = frame
                    
                    # Detect AprilTags and filter to tracked IDs
                    tracked_tags = track_apriltag_frame(
                        self._apriltag_detector, frame, self._tracked_tag_ids
                    )
                    self._detected_tags = list(tracked_tags.values())
                    
                    # Record data if we have valid detections and calibration
                    if tracked_tags and self.calibration:
                        # Calculate average center of all tracked tags
                        centers = [tag.center for tag in tracked_tags.values()]
                        avg_x = sum(c[0] for c in centers) / len(centers)
                        avg_y = sum(c[1] for c in centers) / len(centers)
                        avg_frame_center = (int(avg_x), int(avg_y))
                        
                        position_mm = project_to_axis(avg_frame_center, self.calibration)
                        current_time = time.time() - start_time
                        
                        self.positions.append({
                            'frame': frame_count,
                            'time_s': round(current_time, 4),
                            'position_mm': round(position_mm, 2),
                            'raw_x': avg_frame_center[0],
                            'raw_y': avg_frame_center[1],
                            'tags_tracked': len(tracked_tags),
                            'tag_ids': ','.join(str(tid) for tid in sorted(tracked_tags.keys()))
                        })
                    
                    with hold_canvas(self._canvas):
                        self._video_layer.put_image_data(self._frame_to_rgba(frame), 0, 0)
                        self._draw_overlay()
                    
                    frame_count += 1
            
            time.sleep(1 / self.TARGET_FPS)
        
        self._log(f"Recording stopped: {frame_count} frames captured")
    
    def show(self):
        """Display the tracker UI."""
        display(self._main_ui)
