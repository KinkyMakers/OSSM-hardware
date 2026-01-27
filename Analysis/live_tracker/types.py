"""Data types for the live tracker."""

import math
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Any

import numpy as np


@dataclass
class AprilTagConfig:
    """Configuration for AprilTag detection."""
    family: str = "tagStandard41h12"
    nthreads: int = 2
    quad_decimate: float = 1.0  # Full resolution for accuracy
    quad_sigma: float = 0.0  # No blur by default
    refine_edges: bool = True  # Better corner accuracy
    decode_sharpening: float = 0.25  # Helps with small tags
    tag_size_mm: float = 20.0  # Physical size of the printed tag (outer edge to outer edge)


@dataclass
class DetectedTag:
    """Stores a detected AprilTag's information."""
    tag_id: int
    center: tuple[float, float]  # (x, y) in pixels
    corners: np.ndarray  # 4x2 array of corner coordinates
    decision_margin: float  # Detection quality metric
    hamming: int  # Number of bit errors corrected
    
    @property
    def center_int(self) -> tuple[int, int]:
        """Get center as integer coordinates."""
        return (int(self.center[0]), int(self.center[1]))
    
    @property
    def corners_int(self) -> list[tuple[int, int]]:
        """Get corners as integer coordinates."""
        return [(int(c[0]), int(c[1])) for c in self.corners]


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


class TrackerStep(Enum):
    """Step-based flow for the tracker UI."""
    CAMERA = 1
    CALIBRATE = 2
    POINTS = 3
    TRACK = 4
    EXPORT = 5
