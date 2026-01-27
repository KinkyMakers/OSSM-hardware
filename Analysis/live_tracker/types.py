"""Data types for the live tracker."""

import math
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Any


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
