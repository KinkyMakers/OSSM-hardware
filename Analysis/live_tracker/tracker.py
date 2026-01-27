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

from .types import AprilTagConfig, DetectedTag, TrackerMode, InputSource, InputSourceType
from .tracking import (
    list_cameras, create_apriltag_detector, 
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
    
    def __init__(self, input_source: Optional[InputSource] = None):
        """
        Initialize the LiveTracker.
        
        Args:
            input_source: Optional InputSource to pre-configure. If None, webcam mode
                          with camera selection UI is shown.
        """
        # State
        self.current_step = self.STEP_CAMERA
        self.mode = TrackerMode.IDLE
        self.positions: list[dict] = []
        self.recording_active = False  # Recording data
        self.preview_active = False
        
        # Input source configuration
        self._input_source: Optional[InputSource] = input_source
        self._input_source_type = InputSourceType.WEBCAM  # UI selection
        
        # Video file specific state
        self._video_total_frames = 0
        self._video_current_frame = 0
        self._video_fps = 0.0
        
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
        
        # Calibration: mm per pixel (computed from tag size)
        self._mm_per_pixel: Optional[float] = None
        
        # Debug
        self._debug_mouse_pos: Optional[tuple[int, int]] = None
        
        # Video capture (webcam or video file)
        self._cap: Optional[cv2.VideoCapture] = None
        self._last_frame: Optional[np.ndarray] = None
        
        # Build UI
        self._build_ui()
        self._connect_events()
        self._update_step_styles()
        
        # If input source was provided, pre-configure UI
        if input_source:
            self._apply_input_source(input_source)
    
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
        
        # Step 1: Input Source (Camera or Video File)
        self._source_type_dropdown = Dropdown(
            options=[('Webcam', 'webcam'), ('Video File', 'video_file')],
            value='webcam',
            description='',
            layout=Layout(width='120px')
        )
        self._camera_dropdown = Dropdown(
            options=list_cameras(),
            description='',
            layout=Layout(width='180px')
        )
        self._video_path_input = HTML(
            value='<input type="text" id="video_path" placeholder="Enter video path..." '
                  'style="width: 170px; padding: 4px; border: 1px solid #555; '
                  'border-radius: 3px; background: #2a2a2a; color: #fff;">',
        )
        # Store the actual path value (since HTML widget is readonly)
        from ipywidgets import Text
        self._video_path_text = Text(
            value='',
            placeholder='Enter video file path...',
            description='',
            layout=Layout(width='180px')
        )
        self._video_loop_checkbox = Checkbox(
            value=True,
            description='Loop',
            layout=Layout(width='70px'),
            indent=False
        )
        # Container for webcam controls
        self._webcam_controls = HBox([self._camera_dropdown])
        # Container for video file controls  
        self._video_file_controls = HBox([self._video_path_text, self._video_loop_checkbox])
        self._video_file_controls.layout.display = 'none'  # Hidden by default
        
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
        self._step1_header = HTML('<b style="color: #4CAF50;">1. Source</b>')
        self._step1 = VBox([
            self._step1_header,
            HBox([self._source_type_dropdown, self._preview_btn]),
            self._webcam_controls,
            self._video_file_controls,
            self._camera_diag,
            self._camera_next_btn
        ], layout=Layout(padding='8px', border='1px solid #333', border_radius='5px'))
        
        # Step 2: Tag Size (calibration)
        self._tag_size = FloatText(
            value=self._apriltag_config.tag_size_mm,
            description='',
            layout=Layout(width='70px')
        )
        self._tag_size.observe(self._on_tag_size_change, names='value')
        self._cal_status = HTML('<span style="color: #888;">Set physical tag size</span>')
        self._cal_done_btn = Button(
            description='Next →',
            button_style='success',
            layout=Layout(width='70px')
        )
        self._step2_header = HTML('<b style="color: #2196F3;">2. Tag Size</b>')
        self._step2 = VBox([
            self._step2_header,
            HBox([Label('Tag size (mm):'), self._tag_size]),
            self._cal_status,
            self._cal_done_btn
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
        
        # Source type selection
        self._source_type_dropdown.observe(self._on_source_type_change, names='value')
        
        # Button clicks
        self._preview_btn.on_click(self._on_preview)
        self._camera_next_btn.on_click(self._on_camera_next)
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
            (self._step2, self._step2_header, "2. Tag Size", "#2196F3"),
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
    
    def _on_source_type_change(self, change):
        """Handle source type dropdown change."""
        source_type = change['new']
        self._input_source_type = InputSourceType(source_type)
        
        if source_type == 'webcam':
            self._webcam_controls.layout.display = 'flex'
            self._video_file_controls.layout.display = 'none'
        else:
            self._webcam_controls.layout.display = 'none'
            self._video_file_controls.layout.display = 'flex'
    
    def _apply_input_source(self, source: InputSource):
        """Apply a pre-configured input source to the UI."""
        if source.is_webcam:
            self._source_type_dropdown.value = 'webcam'
            self._input_source_type = InputSourceType.WEBCAM
            # Try to select the camera in dropdown
            cam_options = [opt[1] for opt in self._camera_dropdown.options]
            if source.camera_index in cam_options:
                self._camera_dropdown.value = source.camera_index
        else:
            self._source_type_dropdown.value = 'video_file'
            self._input_source_type = InputSourceType.VIDEO_FILE
            self._video_path_text.value = source.file_path
            self._video_loop_checkbox.value = source.loop
        
        self._input_source = source
    
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
            'Select a source and click Start',
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
        """Update diagnostics display for webcam or video file."""
        if self._actual_width > 0:
            gcd = math.gcd(self._actual_width, self._actual_height)
            ratio = f"{self._actual_width // gcd}:{self._actual_height // gcd}"
            
            if self._input_source and self._input_source.is_video_file:
                # Video file: show progress
                progress = ""
                if self._video_total_frames > 0:
                    pct = (self._video_current_frame / self._video_total_frames) * 100
                    progress = f" | {self._video_current_frame}/{self._video_total_frames} ({pct:.0f}%)"
                loop_indicator = " 🔁" if self._input_source.loop else ""
                self._camera_diag.value = (
                    f'<span style="color: #4CAF50; font-size: 11px;">'
                    f'{self._actual_width}×{self._actual_height} ({ratio}) | '
                    f'{self._measured_fps:.1f} FPS{progress}{loop_indicator}</span>'
                )
            else:
                # Webcam: show as before
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
                f"X: {last_pos['raw_x']:.1f} px",
                10, self.canvas_height - 15
            )
            self._overlay_layer.font = '14px sans-serif'
            mm_px = last_pos.get('mm_per_pixel', 0)
            tags_info = f"Tags: {last_pos.get('tags_tracked', 0)} | {mm_px:.4f} mm/px"
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
    
    def _on_tag_size_change(self, change):
        """Handle tag size input change."""
        new_size = change['new']
        if new_size > 0:
            self._apriltag_config.tag_size_mm = new_size
    
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
        # Debug: store mouse position for overlay
        self._debug_mouse_pos = pos
        
        # Step 3: Tag selection - no mouse interaction needed (use checkboxes)
        if self.current_step == self.STEP_POINTS:
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
        """Toggle preview for webcam or video file."""
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
            # Determine input source
            if self._input_source_type == InputSourceType.WEBCAM:
                cam_idx = self._camera_dropdown.value
                if cam_idx < 0:
                    self._log("No camera available")
                    return
                
                self._cap = cv2.VideoCapture(cam_idx)
                if not self._cap.isOpened():
                    self._log(f"Failed to open camera {cam_idx}")
                    return
                
                self._input_source = InputSource.webcam(cam_idx)
                self._video_total_frames = 0  # Live source, no total
                
            else:  # VIDEO_FILE
                video_path = self._video_path_text.value.strip()
                if not video_path:
                    self._log("Please enter a video file path")
                    return
                
                import os
                if not os.path.isfile(video_path):
                    self._log(f"File not found: {video_path}")
                    return
                
                self._cap = cv2.VideoCapture(video_path)
                if not self._cap.isOpened():
                    self._log(f"Failed to open video: {video_path}")
                    return
                
                loop = self._video_loop_checkbox.value
                self._input_source = InputSource.video_file(video_path, loop=loop)
                self._video_total_frames = int(self._cap.get(cv2.CAP_PROP_FRAME_COUNT))
                self._video_current_frame = 0
                self._video_fps = self._cap.get(cv2.CAP_PROP_FPS)
            
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
        """Advance from camera step to tag size."""
        if not self.preview_active:
            self._log("Start preview first")
            return
        self.current_step = self.STEP_CALIBRATE
        self._update_step_styles()
        self._draw_overlay()
    
    def _on_cal_done(self, btn):
        """Confirm tag size and advance to tag selection."""
        if self._tag_size.value <= 0:
            self._log("Tag size must be positive")
            return
        
        self._apriltag_config.tag_size_mm = self._tag_size.value
        self._cal_status.value = f'<span style="color: #4CAF50;">Tag size: {self._tag_size.value}mm</span>'
        self._log(f"Tag size set to {self._tag_size.value}mm")
        
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
        
        # Restart preview to show live feed again
        if self.preview_active:
            # For video files, rewind to beginning for next recording
            if self._input_source and self._input_source.is_video_file:
                self._cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                self._video_current_frame = 0
            
            thread = threading.Thread(target=self._preview_loop, daemon=True)
            thread.start()
        else:
            self._draw_overlay()
    
    def _auto_save_data(self):
        """Automatically save tracking data to a dated file."""
        if not self.positions:
            return
        
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = f"tracking_data_{timestamp}.csv"
        
        # Include video_frame column if we have video file data
        fieldnames = ['frame', 'time_s', 'raw_x', 'raw_y', 'mm_per_pixel', 'tags_tracked', 'tag_ids',
                      'pose_x', 'pose_y', 'pose_z', 'pose_err']
        if self._input_source and self._input_source.is_video_file:
            fieldnames.append('video_frame')
        
        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
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
        self._mm_per_pixel = None
        self.positions = []
        
        # Reset input source state
        self._input_source = None
        self._video_total_frames = 0
        self._video_current_frame = 0
        self._video_fps = 0.0
        
        # Reset AprilTag state
        self._detected_tags = []
        self._tracked_tag_ids = set()
        self._tag_checkboxes = {}
        self._tags_container.children = []
        
        self._preview_btn.description = 'Start'
        self._preview_btn.button_style = 'info'
        self._camera_next_btn.disabled = True
        self._camera_diag.value = '<span style="color: #666; font-size: 11px;">—</span>'
        self._tag_size.value = 20.0  # Reset to default tag size
        self._apriltag_config.tag_size_mm = 20.0
        self._cal_status.value = '<span style="color: #888;">Set physical tag size</span>'
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
                
                # Handle video file end-of-stream
                if not ret and self._input_source and self._input_source.is_video_file:
                    if self._input_source.loop:
                        # Rewind to beginning
                        self._cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                        self._video_current_frame = 0
                        continue
                    else:
                        # Non-looping: pause at end
                        time.sleep(0.1)
                        continue
                
                if ret:
                    self._last_frame = frame
                    
                    # Track video file position
                    if self._input_source and self._input_source.is_video_file:
                        self._video_current_frame = int(self._cap.get(cv2.CAP_PROP_POS_FRAMES))
                    
                    # FPS tracking
                    self._frame_count += 1
                    elapsed = time.time() - self._fps_timer
                    if elapsed >= 1.0:
                        self._measured_fps = self._frame_count / elapsed
                        self._frame_count = 0
                        self._fps_timer = time.time()
                        self._update_camera_diag()
                    
                    # Detect AprilTags in current frame
                    self._detected_tags = detect_apriltags(
                        self._apriltag_detector, frame, self._apriltag_config.tag_size_mm
                    )
                    
                    # Update tag checkboxes if in tag selection step
                    if self.current_step == self.STEP_POINTS:
                        self._update_tag_checkboxes()
                        self._update_tags_status()
                    
                    with hold_canvas(self._canvas):
                        self._video_layer.put_image_data(self._frame_to_rgba(frame), 0, 0)
                        self._draw_overlay()
            
            time.sleep(1 / self.TARGET_FPS)
    
    def _recording_loop(self):
        """Background loop for recording tracking data using AprilTag detection.
        
        Processes frames as fast as possible without preview rendering.
        """
        frame_count = 0
        start_time = time.time()
        last_status_update = start_time
        status_update_interval = 0.25  # Update status display every 250ms
        
        # Wait for preview loop to stop
        time.sleep(0.1)
        
        # Show "recording" indicator on canvas (static, no live preview)
        self._draw_recording_indicator()
        
        self._log(f"Recording started (preview disabled for max performance)")
        
        while self.recording_active:
            if self._cap and self._cap.isOpened():
                ret, frame = self._cap.read()
                
                # Handle video file end-of-stream during recording
                if not ret and self._input_source and self._input_source.is_video_file:
                    if self._input_source.loop:
                        # Rewind to beginning
                        self._cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                        self._video_current_frame = 0
                        continue
                    else:
                        # Non-looping: stop recording at end
                        self._log("Video file ended")
                        self.recording_active = False
                        break
                
                if ret:
                    self._last_frame = frame
                    
                    # Track video file position
                    if self._input_source and self._input_source.is_video_file:
                        self._video_current_frame = int(self._cap.get(cv2.CAP_PROP_POS_FRAMES))
                    
                    # Detect AprilTags and filter to tracked IDs
                    tracked_tags = track_apriltag_frame(
                        self._apriltag_detector, frame, self._tracked_tag_ids,
                        self._apriltag_config.tag_size_mm
                    )
                    self._detected_tags = list(tracked_tags.values())
                    
                    # Record data if we have valid detections
                    if tracked_tags:
                        # Calculate average center of all tracked tags
                        centers = [tag.center for tag in tracked_tags.values()]
                        avg_x = sum(c[0] for c in centers) / len(centers)
                        avg_y = sum(c[1] for c in centers) / len(centers)
                        
                        # Compute mm_per_pixel from tag size
                        # Use average tag pixel size (diagonal of corners)
                        tag_pixel_sizes = []
                        for tag in tracked_tags.values():
                            corners = tag.corners
                            # Compute tag width as average of top and bottom edge lengths
                            top_edge = math.sqrt((corners[1][0] - corners[0][0])**2 + (corners[1][1] - corners[0][1])**2)
                            bottom_edge = math.sqrt((corners[2][0] - corners[3][0])**2 + (corners[2][1] - corners[3][1])**2)
                            tag_pixel_sizes.append((top_edge + bottom_edge) / 2)
                        
                        avg_tag_pixels = sum(tag_pixel_sizes) / len(tag_pixel_sizes) if tag_pixel_sizes else 1
                        mm_per_pixel = self._apriltag_config.tag_size_mm / avg_tag_pixels if avg_tag_pixels > 0 else 1
                        
                        # Store calibration for display
                        self._mm_per_pixel = mm_per_pixel
                        
                        current_time = time.time() - start_time
                        
                        # Compute average pose from all tracked tags (if pose data available)
                        pose_x, pose_y, pose_z, pose_err = None, None, None, None
                        tags_with_pose = [t for t in tracked_tags.values() if t.has_pose]
                        if tags_with_pose:
                            pose_x = sum(t.pose_t[0][0] for t in tags_with_pose) / len(tags_with_pose)
                            pose_y = sum(t.pose_t[1][0] for t in tags_with_pose) / len(tags_with_pose)
                            pose_z = sum(t.pose_t[2][0] for t in tags_with_pose) / len(tags_with_pose)
                            pose_err = sum(t.pose_err for t in tags_with_pose) / len(tags_with_pose)
                        
                        # For video files, also record the source video frame number
                        video_frame = self._video_current_frame if self._input_source and self._input_source.is_video_file else ''
                        
                        self.positions.append({
                            'frame': frame_count,
                            'time_s': round(current_time, 4),
                            'raw_x': round(avg_x, 2),
                            'raw_y': round(avg_y, 2),
                            'mm_per_pixel': round(mm_per_pixel, 6),
                            'tags_tracked': len(tracked_tags),
                            'tag_ids': ','.join(str(tid) for tid in sorted(tracked_tags.keys())),
                            'pose_x': round(pose_x * 1000, 3) if pose_x is not None else '',  # Convert to mm
                            'pose_y': round(pose_y * 1000, 3) if pose_y is not None else '',  # Convert to mm
                            'pose_z': round(pose_z * 1000, 3) if pose_z is not None else '',  # Convert to mm (distance)
                            'pose_err': round(pose_err, 6) if pose_err is not None else '',
                            'video_frame': video_frame,
                        })
                    
                    frame_count += 1
                    
                    # Update status display periodically (not every frame)
                    now = time.time()
                    if now - last_status_update >= status_update_interval:
                        elapsed = now - start_time
                        fps = frame_count / elapsed if elapsed > 0 else 0
                        self._update_recording_status(frame_count, len(self.positions), fps)
                        last_status_update = now
            
            # No sleep - process as fast as possible!
        
        # Calculate final stats
        total_time = time.time() - start_time
        avg_fps = frame_count / total_time if total_time > 0 else 0
        self._log(f"Recording stopped: {frame_count} frames in {total_time:.1f}s ({avg_fps:.1f} FPS avg)")
    
    def _draw_recording_indicator(self):
        """Draw a static recording indicator on the canvas (no live preview)."""
        # Clear video layer to dark background
        self._video_layer.fill_style = '#1a1a2e'
        self._video_layer.fill_rect(0, 0, self.canvas_width, self.canvas_height)
        
        # Draw recording indicator
        self._overlay_layer.clear()
        self._overlay_layer.fill_style = '#E91E63'
        self._overlay_layer.font = 'bold 24px sans-serif'
        self._overlay_layer.text_align = 'center'
        self._overlay_layer.fill_text(
            '● RECORDING',
            self.canvas_width // 2,
            self.canvas_height // 2 - 40
        )
        self._overlay_layer.fill_style = '#888'
        self._overlay_layer.font = '14px sans-serif'
        self._overlay_layer.fill_text(
            'Preview disabled for maximum performance',
            self.canvas_width // 2,
            self.canvas_height // 2
        )
        self._overlay_layer.fill_text(
            'Processing frames...',
            self.canvas_width // 2,
            self.canvas_height // 2 + 30
        )
        self._overlay_layer.text_align = 'left'
    
    def _update_recording_status(self, frames_processed: int, frames_recorded: int, fps: float):
        """Update the recording status display."""
        # Update overlay with current stats
        self._overlay_layer.clear()
        self._overlay_layer.fill_style = '#E91E63'
        self._overlay_layer.font = 'bold 24px sans-serif'
        self._overlay_layer.text_align = 'center'
        self._overlay_layer.fill_text(
            '● RECORDING',
            self.canvas_width // 2,
            self.canvas_height // 2 - 60
        )
        
        self._overlay_layer.fill_style = '#4CAF50'
        self._overlay_layer.font = 'bold 32px sans-serif'
        self._overlay_layer.fill_text(
            f'{frames_recorded} frames',
            self.canvas_width // 2,
            self.canvas_height // 2
        )
        
        self._overlay_layer.fill_style = '#888'
        self._overlay_layer.font = '14px sans-serif'
        self._overlay_layer.fill_text(
            f'{fps:.1f} FPS | {frames_processed} processed',
            self.canvas_width // 2,
            self.canvas_height // 2 + 40
        )
        
        # Show video progress if applicable
        if self._input_source and self._input_source.is_video_file and self._video_total_frames > 0:
            pct = (self._video_current_frame / self._video_total_frames) * 100
            self._overlay_layer.fill_text(
                f'Video: {self._video_current_frame}/{self._video_total_frames} ({pct:.0f}%)',
                self.canvas_width // 2,
                self.canvas_height // 2 + 70
            )
        
        self._overlay_layer.text_align = 'left'
        
        # Also update the step status
        self._track_status.value = (
            f'<span style="color: #E91E63; font-weight: bold;">'
            f'● {frames_recorded} frames | {fps:.1f} FPS</span>'
        )
    
    @property
    def tag_size(self) -> float:
        """Get the current tag size in mm."""
        return self._apriltag_config.tag_size_mm
    
    @tag_size.setter
    def tag_size(self, value: float):
        """Set the tag size in mm."""
        if value > 0:
            self._apriltag_config.tag_size_mm = value
            self._tag_size.value = value
    
    def show(self):
        """Display the tracker UI."""
        display(self._main_ui)
