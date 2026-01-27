"""Data types for the live tracker."""

import math
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional, Any, Union

import numpy as np


class InputSourceType(Enum):
    """Type of video input source."""
    WEBCAM = "webcam"
    VIDEO_FILE = "video_file"


@dataclass
class InputSource:
    """Configuration for video input source."""
    source_type: InputSourceType
    # For WEBCAM: camera index (int)
    # For VIDEO_FILE: file path (str or Path)
    source: Union[int, str, Path]
    # For VIDEO_FILE: whether to loop when reaching end
    loop: bool = True
    
    @classmethod
    def webcam(cls, camera_index: int = 0) -> "InputSource":
        """Create a webcam input source."""
        return cls(source_type=InputSourceType.WEBCAM, source=camera_index)
    
    @classmethod
    def video_file(cls, path: Union[str, Path], loop: bool = True) -> "InputSource":
        """Create a video file input source."""
        return cls(source_type=InputSourceType.VIDEO_FILE, source=str(path), loop=loop)
    
    @property
    def is_webcam(self) -> bool:
        """Check if this is a webcam source."""
        return self.source_type == InputSourceType.WEBCAM
    
    @property
    def is_video_file(self) -> bool:
        """Check if this is a video file source."""
        return self.source_type == InputSourceType.VIDEO_FILE
    
    @property
    def camera_index(self) -> int:
        """Get camera index (only valid for webcam sources)."""
        if not self.is_webcam:
            raise ValueError("Not a webcam source")
        return int(self.source)
    
    @property
    def file_path(self) -> str:
        """Get file path (only valid for video file sources)."""
        if not self.is_video_file:
            raise ValueError("Not a video file source")
        return str(self.source)


@dataclass
class AprilTagConfig:
    """Configuration for AprilTag detection."""
    family: str = "tagStandard41h12"
    nthreads: int = 4
    quad_decimate: float = 1.0  # Full resolution for accuracy
    quad_sigma: float = 4  # No blur by default
    refine_edges: bool = True  # Better corner accuracy
    decode_sharpening: float = 0.25  # Helps with small tags
    tag_size_mm: float = 11.1  # Physical size of the printed tag (outer edge to outer edge)


@dataclass
class DetectedTag:
    """Stores a detected AprilTag's information."""
    tag_id: int
    center: tuple[float, float]  # (x, y) in pixels
    corners: np.ndarray  # 4x2 array of corner coordinates
    decision_margin: float  # Detection quality metric
    hamming: int  # Number of bit errors corrected
    # Pose estimation fields (only populated if estimate_tag_pose=True)
    pose_R: Optional[np.ndarray] = None  # 3x3 rotation matrix
    pose_t: Optional[np.ndarray] = None  # 3x1 translation vector (in meters)
    pose_err: Optional[float] = None  # Object-space error of the estimation
    
    @property
    def center_int(self) -> tuple[int, int]:
        """Get center as integer coordinates."""
        return (int(self.center[0]), int(self.center[1]))
    
    @property
    def corners_int(self) -> list[tuple[int, int]]:
        """Get corners as integer coordinates."""
        return [(int(c[0]), int(c[1])) for c in self.corners]
    
    @property
    def has_pose(self) -> bool:
        """Check if pose estimation data is available."""
        return self.pose_R is not None and self.pose_t is not None
    
    @property
    def distance_m(self) -> Optional[float]:
        """Get distance from camera in meters (z component of translation)."""
        if self.pose_t is not None:
            return float(self.pose_t[2][0])
        return None
    
    @property
    def distance_mm(self) -> Optional[float]:
        """Get distance from camera in millimeters."""
        if self.pose_t is not None:
            return float(self.pose_t[2][0]) * 1000.0
        return None


@dataclass
class TrackingCircle:
    """Stores the tracking circle parameters and tracker state."""
    center: tuple[int, int]
    radius: int = 20
    # OpenCV tracker instance (Any to avoid cv2 import in types)
    tracker: Any = None
    # Whether this tracker has been initialized with a frame
    tracker_initialized: bool = False


@dataclass 
class MultiTrackingCircles:
    """Stores multiple tracking circles for averaging."""
    circles: list[TrackingCircle] = field(default_factory=list)
    
    @property
    def average_center(self) -> tuple[int, int]:
        """Get the average center of all circles."""
        if not self.circles:
            raise ValueError("No circles to average")
        avg_x = sum(c.center[0] for c in self.circles) / len(self.circles)
        avg_y = sum(c.center[1] for c in self.circles) / len(self.circles)
        return (int(avg_x), int(avg_y))
    
    @property
    def average_radius(self) -> int:
        """Get the average radius of all circles."""
        if not self.circles:
            raise ValueError("No circles to average")
        return int(sum(c.radius for c in self.circles) / len(self.circles))


class TrackerMode(Enum):
    """State machine for tracker modes (legacy, kept for compatibility)."""
    IDLE = "idle"
    CALIBRATING = "calibrating"
    SELECTING_POINTS = "selecting_points"
    TRACKING = "tracking"


class TrackingSource(Enum):
    """Source of tracking data for a frame."""
    APRILTAG = "apriltag"      # Ground truth from AprilTag detection
    OPENCV = "opencv"          # Interpolated from OpenCV tracker
    LOST = "lost"              # No tracking available


@dataclass
class HybridTrackingState:
    """
    State for hybrid AprilTag + OpenCV tracking.
    
    AprilTag is ground truth. When detected, it overwrites OpenCV tracker.
    When AprilTag is lost, OpenCV tracker interpolates between detections.
    """
    # OpenCV tracker instance (CSRT or similar)
    opencv_tracker: Any = None
    # Whether OpenCV tracker is initialized
    opencv_initialized: bool = False
    # Last known position from AprilTag (ground truth)
    last_apriltag_center: Optional[tuple[float, float]] = None
    # Last known bounding box for the tracked region
    last_bbox: Optional[tuple[int, int, int, int]] = None  # (x, y, w, h)
    # Stored template image (grayscale) for template matching fallback
    template_image: Optional[np.ndarray] = None
    # Frames since last AprilTag detection
    frames_since_apriltag: int = 0
    # Maximum frames to interpolate before considering tracking lost
    max_interpolation_frames: int = 30
    # Last tracking source
    last_source: TrackingSource = TrackingSource.LOST
    # Tag ID being tracked (for single-tag hybrid tracking)
    tracked_tag_id: Optional[int] = None
    
    def reset(self):
        """Reset tracking state."""
        self.opencv_tracker = None
        self.opencv_initialized = False
        self.last_apriltag_center = None
        self.last_bbox = None
        self.template_image = None
        self.frames_since_apriltag = 0
        self.last_source = TrackingSource.LOST
    
    @property
    def is_tracking_lost(self) -> bool:
        """Check if tracking is considered lost."""
        return self.frames_since_apriltag > self.max_interpolation_frames


class TrackerStep(Enum):
    """Step-based flow for the tracker UI."""
    CAMERA = 1
    CALIBRATE = 2
    POINTS = 3
    TRACK = 4
    EXPORT = 5
