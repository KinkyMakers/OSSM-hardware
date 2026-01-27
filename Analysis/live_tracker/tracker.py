"""Main LiveTracker class with improved UX flow."""

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
    Dropdown, Button, FloatText, IntSlider, HBox, VBox,
    Output, Label, HTML, Layout
)

from .types import CalibrationData, TrackingCircle, MultiTrackingCircles, TrackerMode
from .tracking import (
    project_to_axis, track_frame, list_cameras, initialize_tracker
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
        self.tracking_circles = MultiTrackingCircles()
        self.positions: list[dict] = []
        self.recording_active = False  # Recording data
        self.preview_active = False
        
        # Track last known frame positions for each circle (by index)
        self._last_frame_centers: dict[int, tuple[int, int]] = {}
        
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
        
        # Tracking point interaction state
        self._dragging_track_idx: Optional[int] = None
        self._hover_track_idx: Optional[int] = None
        self._shift_held = False
        
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
        
        # Step 3: Tracking Points
        self._radius_slider = IntSlider(
            value=20, min=5, max=100, step=1,
            description='Radius:',
            layout=Layout(width='200px')
        )
        self._points_clear_btn = Button(
            description='Clear All',
            button_style='warning',
            layout=Layout(width='80px')
        )
        self._points_next_btn = Button(
            description='Next →',
            button_style='success',
            disabled=True,
            layout=Layout(width='70px')
        )
        self._points_status = HTML('<span style="color: #888;">Click to place point</span>')
        self._step3_header = HTML('<b style="color: #FF9800;">3. Track Points</b>')
        self._step3 = VBox([
            self._step3_header,
            self._radius_slider,
            HTML('<span style="font-size: 11px; color: #888;">Hold Shift for multiple points</span>'),
            self._points_status,
            HBox([self._points_clear_btn, self._points_next_btn])
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
        self._points_clear_btn.on_click(self._on_points_clear)
        self._points_next_btn.on_click(self._on_points_next)
        self._track_go_btn.on_click(self._on_track_go)
        self._track_stop_btn.on_click(self._on_track_stop)
        self._reset_btn.on_click(self._on_reset)
        
        # Radius slider update
        self._radius_slider.observe(self._on_radius_change, names='value')
    
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
        
        # Tracking circles
        for i, circle in enumerate(self.tracking_circles.circles):
            is_hover = (i == self._hover_track_idx)
            color = '#ff4444' if is_hover else '#ffa500'
            self._draw_circle(circle.center, circle.radius, color, fill=False)
            self._draw_circle(circle.center, 3, color, fill=True)
            
            # Point number
            self._overlay_layer.fill_style = color
            self._overlay_layer.font = '12px sans-serif'
            self._overlay_layer.fill_text(str(i + 1), circle.center[0] + circle.radius + 5, circle.center[1])
            
            # Remove button in point selection mode
            if self.current_step == self.STEP_POINTS:
                x_pos = circle.center[0] + circle.radius + 2
                y_pos = circle.center[1] - circle.radius - 2
                self._overlay_layer.fill_style = '#ff4444'
                self._overlay_layer.font = 'bold 14px sans-serif'
                self._overlay_layer.fill_text('✕', x_pos, y_pos)
        
        # Average marker for multiple points
        if len(self.tracking_circles.circles) > 1:
            avg_x = sum(c.center[0] for c in self.tracking_circles.circles) / len(self.tracking_circles.circles)
            avg_y = sum(c.center[1] for c in self.tracking_circles.circles) / len(self.tracking_circles.circles)
            self._draw_cross((int(avg_x), int(avg_y)), 10, '#ffff00')
        
        # Mode indicator
        mode_messages = {
            self.STEP_CAMERA: '',
            self.STEP_CALIBRATE: 'Click to place points, drag to adjust',
            self.STEP_POINTS: 'Click to place | Shift+Click for more | Drag to move',
            self.STEP_TRACK: 'RECORDING' if self.recording_active else 'Press Go to record',
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
            self._overlay_layer.fill_text(
                f"Frame: {len(self.positions)}",
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
    
    def _find_track_point_at(self, x: int, y: int) -> Optional[int]:
        """Find tracking point index at position, or None."""
        for i, circle in enumerate(self.tracking_circles.circles):
            dist = math.sqrt((x - circle.center[0])**2 + (y - circle.center[1])**2)
            if dist <= circle.radius + 10:
                return i
        return None
    
    def _is_on_remove_button(self, x: int, y: int, idx: int) -> bool:
        """Check if position is on a tracking point's remove button."""
        if idx is None or idx >= len(self.tracking_circles.circles):
            return False
        circle = self.tracking_circles.circles[idx]
        x_btn = circle.center[0] + circle.radius + 8
        y_btn = circle.center[1] - circle.radius - 8
        return abs(x - x_btn) < 12 and abs(y - y_btn) < 12
    
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
    
    def _update_points_status(self):
        """Update points status display."""
        n = len(self.tracking_circles.circles)
        if n > 0:
            plural = 's' if n > 1 else ''
            self._points_status.value = f'<span style="color: #FF9800;">{n} point{plural} placed</span>'
            self._points_next_btn.disabled = False
        else:
            self._points_status.value = '<span style="color: #888;">Click to place point</span>'
            self._points_next_btn.disabled = True
    
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
        
        # Step 3: Point selection
        elif self.current_step == self.STEP_POINTS:
            if event_type == 'mousedown':
                idx = self._find_track_point_at(x, y)
                
                # Check for remove button
                if idx is not None and self._is_on_remove_button(x, y, idx):
                    self.tracking_circles.circles.pop(idx)
                    self._update_points_status()
                    self._draw_overlay()
                    return
                
                # Check for drag
                if idx is not None and not self._shift_held:
                    self._dragging_track_idx = idx
                else:
                    # Place new point
                    if self._shift_held or len(self.tracking_circles.circles) == 0:
                        new_circle = TrackingCircle(center=pos, radius=self._radius_slider.value)
                        self.tracking_circles.circles.append(new_circle)
                        # Initialize tracker immediately so it starts following
                        circle_idx = len(self.tracking_circles.circles) - 1
                        if self._initialize_circle_tracker(new_circle, circle_idx):
                            self._log(f"Tracker {circle_idx + 1} initialized - now tracking")
                        self._update_points_status()
                    elif len(self.tracking_circles.circles) == 1 and not self._shift_held:
                        new_circle = TrackingCircle(center=pos, radius=self._radius_slider.value)
                        self.tracking_circles.circles[0] = new_circle
                        # Reinitialize tracker at new position
                        if self._initialize_circle_tracker(new_circle, 0):
                            self._log("Tracker 1 reinitialized - now tracking")
                    self._draw_overlay()
            
            elif event_type == 'mousemove':
                if self._dragging_track_idx is not None:
                    self.tracking_circles.circles[self._dragging_track_idx].center = pos
                else:
                    self._hover_track_idx = self._find_track_point_at(x, y)
                # Always redraw overlay for debug crosshair
                self._draw_overlay()
            
            elif event_type == 'mouseup':
                self._dragging_track_idx = None
    
    def _handle_key(self, ev):
        """Handle keyboard events."""
        key = ev.get('key', '')
        event_type = ev.get('type', '')
        
        if key == 'Shift':
            self._shift_held = (event_type == 'keydown')
        
        if event_type != 'keydown':
            return
        
        if key == 'Enter':
            if self.current_step == self.STEP_CAMERA and self.preview_active:
                self._on_camera_next(None)
            elif self.current_step == self.STEP_CALIBRATE and not self._cal_done_btn.disabled:
                self._on_cal_done(None)
            elif self.current_step == self.STEP_POINTS and not self._points_next_btn.disabled:
                self._on_points_next(None)
    
    def _on_radius_change(self, change):
        """Handle radius slider change."""
        new_radius = change['new']
        for circle in self.tracking_circles.circles:
            circle.radius = new_radius
        self._draw_overlay()
    
    def _initialize_circle_tracker(self, circle: TrackingCircle, idx: int) -> bool:
        """
        Initialize tracking for a single circle using the current frame.
        Called immediately when a point is placed.
        """
        if self._last_frame is None:
            return False
        
        # Convert canvas coordinates to frame coordinates
        frame_center = self._canvas_to_frame(*circle.center)
        frame_radius = int(circle.radius / self._frame_scale) if self._frame_scale > 0 else circle.radius
        
        # Create a temporary circle for initialization in frame coordinates
        temp_circle = TrackingCircle(center=frame_center, radius=frame_radius)
        
        if initialize_tracker(temp_circle, self._last_frame):
            circle.tracker = temp_circle.tracker
            circle.tracker_initialized = True
            # Store initial frame position
            self._last_frame_centers[idx] = frame_center
            return True
        else:
            circle.tracker = None
            circle.tracker_initialized = False
            return False
    
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
        self.tracking_circles = MultiTrackingCircles()
        self._update_step_styles()
        self._update_points_status()
        self._draw_overlay()
    
    def _on_points_clear(self, btn):
        """Clear all tracking points."""
        self.tracking_circles = MultiTrackingCircles()
        self._update_points_status()
        self._draw_overlay()
    
    def _on_points_next(self, btn):
        """Confirm points and advance to tracking."""
        if not self.tracking_circles.circles:
            self._log("Place at least one tracking point")
            return
        
        self.current_step = self.STEP_TRACK
        self._track_go_btn.disabled = False
        self._track_stop_btn.disabled = True
        self._update_step_styles()
        self._draw_overlay()
    
    def _on_track_go(self, btn):
        """Start recording tracking data."""
        if not self.calibration or not self.tracking_circles.circles:
            self._log("Complete setup first")
            return
        
        # Check that at least one tracker is initialized
        initialized_count = sum(
            1 for c in self.tracking_circles.circles 
            if getattr(c, 'tracker_initialized', False)
        )
        
        if initialized_count == 0:
            self._log("No trackers initialized - place tracking points first")
            return
        
        self._log(f"Starting recording with {initialized_count} active tracker(s)")
        
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
        
        fieldnames = ['frame', 'time_s', 'position_mm', 'raw_x', 'raw_y', 'points_tracked']
        
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
        self.tracking_circles = MultiTrackingCircles()
        self.positions = []
        self._last_frame_centers = {}
        self._cal_point1 = None
        self._cal_point2 = None
        self._dragging_cal_point = None
        self._dragging_track_idx = None
        self._hover_track_idx = None
        
        self._preview_btn.description = 'Start'
        self._preview_btn.button_style = 'info'
        self._camera_next_btn.disabled = True
        self._camera_diag.value = '<span style="color: #666; font-size: 11px;">—</span>'
        self._cal_status.value = '<span style="color: #888;">Click to place 2 points</span>'
        self._cal_done_btn.disabled = True
        self._points_status.value = '<span style="color: #888;">Click to place point</span>'
        self._points_next_btn.disabled = True
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
        """Background preview loop with live visual tracking."""
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
                    
                    # Visual tracking - update circle positions if initialized
                    for i, circle in enumerate(self.tracking_circles.circles):
                        if not getattr(circle, 'tracker_initialized', False):
                            continue
                        if getattr(circle, 'tracker', None) is None:
                            continue
                        
                        # Get previous frame position
                        prev_frame_center = self._last_frame_centers.get(
                            i, self._canvas_to_frame(*circle.center)
                        )
                        
                        # Track using template matching
                        new_frame_center = track_frame(frame, circle, prev_frame_center)
                        
                        if new_frame_center:
                            # Update stored frame position
                            self._last_frame_centers[i] = new_frame_center
                            # Update canvas display position
                            new_canvas_center = self._frame_to_canvas(*new_frame_center)
                            circle.center = new_canvas_center
                    
                    with hold_canvas(self._canvas):
                        self._video_layer.put_image_data(self._frame_to_rgba(frame), 0, 0)
                        self._draw_overlay()
            
            time.sleep(1 / self.TARGET_FPS)
    
    def _recording_loop(self):
        """Background loop for recording tracking data."""
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
                    
                    # Track each point using template matching
                    tracked_frame_centers = []
                    for i, circle in enumerate(self.tracking_circles.circles):
                        if not getattr(circle, 'tracker_initialized', False):
                            continue
                        if getattr(circle, 'tracker', None) is None:
                            continue
                        
                        # Get the previous frame position
                        prev_frame_center = self._last_frame_centers.get(
                            i, self._canvas_to_frame(*circle.center)
                        )
                        
                        # Track using template matching
                        new_frame_center = track_frame(frame, circle, prev_frame_center)
                        
                        if new_frame_center:
                            # Update stored frame position
                            self._last_frame_centers[i] = new_frame_center
                            # Update canvas display position
                            new_canvas_center = self._frame_to_canvas(*new_frame_center)
                            circle.center = new_canvas_center
                            tracked_frame_centers.append(new_frame_center)
                        else:
                            # Tracking lost - keep using last known position
                            tracked_frame_centers.append(prev_frame_center)
                    
                    # Record data if we have valid tracking and calibration
                    if tracked_frame_centers and self.calibration:
                        avg_x = sum(c[0] for c in tracked_frame_centers) / len(tracked_frame_centers)
                        avg_y = sum(c[1] for c in tracked_frame_centers) / len(tracked_frame_centers)
                        avg_frame_center = (int(avg_x), int(avg_y))
                        
                        position_mm = project_to_axis(avg_frame_center, self.calibration)
                        current_time = time.time() - start_time
                        
                        self.positions.append({
                            'frame': frame_count,
                            'time_s': round(current_time, 4),
                            'position_mm': round(position_mm, 2),
                            'raw_x': avg_frame_center[0],
                            'raw_y': avg_frame_center[1],
                            'points_tracked': len(tracked_frame_centers)
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
