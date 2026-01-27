"""Core tracking functions using AprilTag detection and template matching."""

import math
from typing import Optional

import cv2
import numpy as np
from pupil_apriltags import Detector

from .types import CalibrationData, TrackingCircle, AprilTagConfig, DetectedTag


# =============================================================================
# AprilTag Detection Functions
# =============================================================================

def create_apriltag_detector(config: Optional[AprilTagConfig] = None) -> Detector:
    """
    Create an AprilTag detector with the given configuration.
    
    Args:
        config: AprilTagConfig with detection parameters, or None for defaults
        
    Returns:
        Configured Detector instance
    """
    if config is None:
        config = AprilTagConfig()
    
    return Detector(
        families=config.family,
        nthreads=config.nthreads,
        quad_decimate=config.quad_decimate,
        quad_sigma=config.quad_sigma,
        refine_edges=config.refine_edges,
        decode_sharpening=config.decode_sharpening,
    )


def detect_apriltags(detector: Detector, frame: np.ndarray) -> list[DetectedTag]:
    """
    Detect AprilTags in a frame.
    
    Args:
        detector: AprilTag Detector instance
        frame: BGR or grayscale frame
        
    Returns:
        List of DetectedTag objects
    """
    # Convert to grayscale if needed
    if len(frame.shape) == 3:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    else:
        gray = frame
    
    # Run detection
    detections = detector.detect(gray)
    
    # Convert to our DetectedTag type
    results = []
    for det in detections:
        tag = DetectedTag(
            tag_id=det.tag_id,
            center=(float(det.center[0]), float(det.center[1])),
            corners=det.corners,
            decision_margin=det.decision_margin,
            hamming=det.hamming,
        )
        results.append(tag)
    
    return results


def track_apriltag_frame(
    detector: Detector,
    frame: np.ndarray,
    tracked_ids: Optional[set[int]] = None
) -> dict[int, DetectedTag]:
    """
    Detect and track AprilTags in a frame.
    
    Args:
        detector: AprilTag Detector instance
        frame: BGR frame to track in
        tracked_ids: Set of tag IDs to track, or None to track all
        
    Returns:
        Dictionary mapping tag_id to DetectedTag for tracked tags
    """
    detections = detect_apriltags(detector, frame)
    
    if tracked_ids is None:
        # Track all detected tags
        return {det.tag_id: det for det in detections}
    else:
        # Only track specified IDs
        return {det.tag_id: det for det in detections if det.tag_id in tracked_ids}


# =============================================================================
# Calibration and Position Functions
# =============================================================================

def project_to_axis(point: tuple[int, int], calibration: CalibrationData) -> float:
    """Project a point onto the calibration axis and return position in mm."""
    # Vector from calibration point1 to the tracked point
    px = point[0] - calibration.point1[0]
    py = point[1] - calibration.point1[1]
    
    # Unit vector along calibration line
    angle = calibration.angle
    ux = math.cos(angle)
    uy = math.sin(angle)
    
    # Project onto the line (dot product)
    projection_pixels = px * ux + py * uy
    
    # Convert to mm
    return projection_pixels * calibration.mm_per_pixel


def extract_template(frame: np.ndarray, center: tuple[int, int], 
                     radius: int) -> Optional[np.ndarray]:
    """
    Extract a template from the frame at the given position.
    
    Args:
        frame: BGR frame
        center: Center point (x, y)
        radius: Radius of the region to extract
        
    Returns:
        Grayscale template image or None if extraction failed
    """
    h, w = frame.shape[:2]
    
    # Calculate bounding box with padding
    x1 = max(0, center[0] - radius)
    y1 = max(0, center[1] - radius)
    x2 = min(w, center[0] + radius)
    y2 = min(h, center[1] + radius)
    
    # Ensure we have a valid region
    if x2 - x1 < 10 or y2 - y1 < 10:
        return None
    
    # Extract and convert to grayscale
    region = frame[y1:y2, x1:x2]
    gray = cv2.cvtColor(region, cv2.COLOR_BGR2GRAY)
    
    return gray


def initialize_tracker(circle: TrackingCircle, frame: np.ndarray) -> bool:
    """
    Initialize tracking for a circle by capturing its actual appearance as a template.
    
    Args:
        circle: TrackingCircle object to initialize (modified in place)
        frame: BGR frame to capture the template from
        
    Returns:
        True if initialization succeeded, False otherwise
    """
    try:
        # Extract the actual appearance of the object as our template
        template = extract_template(frame, circle.center, circle.radius)
        
        if template is None:
            circle.tracker_initialized = False
            return False
        
        # Store the template in the tracker field (repurposed)
        circle.tracker = template
        circle.tracker_initialized = True
        
        return True
    except Exception as e:
        print(f"Template capture error: {e}")
        circle.tracker_initialized = False
        return False


def track_frame(frame: np.ndarray, circle: TrackingCircle, 
                prev_frame_center: tuple[int, int]) -> Optional[tuple[int, int]]:
    """
    Track a point in a single frame using template matching with the captured appearance.
    
    This uses the actual appearance of the object (captured at initialization) to find
    it in subsequent frames. Much more robust than using a synthetic template.
    
    Args:
        frame: BGR frame to track in
        circle: TrackingCircle with captured template
        prev_frame_center: Previous center position in FRAME coordinates
        
    Returns:
        New center position (x, y) in FRAME coordinates, or None if tracking failed
    """
    if not getattr(circle, 'tracker_initialized', False):
        return None
    
    template = getattr(circle, 'tracker', None)
    if template is None:
        return None
    
    # Convert frame to grayscale
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
    # Define search region around previous position (3x the template size)
    template_h, template_w = template.shape[:2]
    search_margin = max(template_w, template_h) * 2
    
    h, w = gray.shape[:2]
    x1 = max(0, prev_frame_center[0] - search_margin)
    y1 = max(0, prev_frame_center[1] - search_margin)
    x2 = min(w, prev_frame_center[0] + search_margin)
    y2 = min(h, prev_frame_center[1] + search_margin)
    
    search_region = gray[y1:y2, x1:x2]
    
    # Ensure search region is large enough for template
    if search_region.shape[0] < template_h or search_region.shape[1] < template_w:
        return None
    
    # Perform template matching with normalized cross-correlation
    result = cv2.matchTemplate(search_region, template, cv2.TM_CCOEFF_NORMED)
    _, max_val, _, max_loc = cv2.minMaxLoc(result)
    
    # Higher threshold for better matching (the actual object should match well)
    if max_val > 0.5:
        # Convert back to full frame coordinates
        match_x = x1 + max_loc[0] + template_w // 2
        match_y = y1 + max_loc[1] + template_h // 2
        return (match_x, match_y)
    
    # If match quality is low, tracking may be lost
    return None


def track_frame_with_fallback(frame: np.ndarray, circle: TrackingCircle,
                               prev_center: tuple[int, int]) -> tuple[tuple[int, int], bool]:
    """
    Track with template matching, with fallback behavior.
    
    Args:
        frame: BGR frame to track in
        circle: TrackingCircle with template
        prev_center: Previous center position in FRAME coordinates for fallback
        
    Returns:
        Tuple of (new_center, tracking_was_successful)
    """
    new_center = track_frame(frame, circle, prev_center)
    
    if new_center is not None:
        return (new_center, True)
    
    # Template matching failed - return previous position
    return (prev_center, False)


def list_cameras(max_cameras: int = 10) -> list[tuple[str, int]]:
    """Find available camera indices."""
    available = []
    for i in range(max_cameras):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            ret, _ = cap.read()
            if ret:
                backend = cap.getBackendName()
                available.append((f"Camera {i} ({backend})", i))
            cap.release()
    return available if available else [("No cameras found", -1)]
