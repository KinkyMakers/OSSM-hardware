"""Core tracking functions using AprilTag detection and template matching."""

import math
from dataclasses import dataclass
from typing import Optional

import cv2
import numpy as np
from pupil_apriltags import Detector

from .types import TrackingCircle, AprilTagConfig, DetectedTag, HybridTrackingState, TrackingSource


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


def detect_apriltags(detector: Detector, frame: np.ndarray, tag_size_mm: float = 20.0) -> list[DetectedTag]:
    """
    Detect AprilTags in a frame.
    
    Args:
        detector: AprilTag Detector instance
        frame: BGR or grayscale frame
        tag_size_mm: Physical size of the AprilTag in millimeters
        
    Returns:
        List of DetectedTag objects
    """
    # Convert to grayscale if needed
    if len(frame.shape) == 3:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    else:
        gray = frame
    
    # Run detection with iPhone 16 camera intrinsics (estimated for 1080p)
    # fx, fy: focal length in pixels (~1450 for 26mm equiv, 67° HFOV)
    # cx, cy: principal point at image center (960, 540 for 1080p)
    h, w = gray.shape[:2]
    fx = fy = 1450.0  # Approximate focal length for iPhone 16 main camera
    cx, cy = w / 2.0, h / 2.0  # Principal point at image center
    detections = detector.detect(gray, True, [fx, fy, cx, cy], tag_size_mm / 1000.0)
    
    # Convert to our DetectedTag type, including pose estimation data
    results = []
    for det in detections:
        tag = DetectedTag(
            tag_id=det.tag_id,
            center=(float(det.center[0]), float(det.center[1])),
            corners=det.corners,
            decision_margin=det.decision_margin,
            hamming=det.hamming,
            pose_R=det.pose_R,
            pose_t=det.pose_t,
            pose_err=det.pose_err,
        )
        results.append(tag)
    
    return results


def track_apriltag_frame(
    detector: Detector,
    frame: np.ndarray,
    tracked_ids: Optional[set[int]] = None,
    tag_size_mm: float = 20.0
) -> dict[int, DetectedTag]:
    """
    Detect and track AprilTags in a frame.
    
    Args:
        detector: AprilTag Detector instance
        frame: BGR frame to track in
        tracked_ids: Set of tag IDs to track, or None to track all
        tag_size_mm: Physical size of the AprilTag in millimeters
        
    Returns:
        Dictionary mapping tag_id to DetectedTag for tracked tags
    """
    detections = detect_apriltags(detector, frame, tag_size_mm)
    
    if tracked_ids is None:
        # Track all detected tags
        return {det.tag_id: det for det in detections}
    else:
        # Only track specified IDs
        return {det.tag_id: det for det in detections if det.tag_id in tracked_ids}


# =============================================================================
# Template Tracking Functions
# =============================================================================

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


# =============================================================================
# Hybrid Tracking Functions (AprilTag + OpenCV fallback)
# =============================================================================

def compute_tag_bbox(tag: DetectedTag, padding: float = 0.5) -> tuple[int, int, int, int]:
    """
    Compute a bounding box around an AprilTag with padding.
    
    Args:
        tag: DetectedTag with corners
        padding: Padding factor (0.5 = 50% larger on each side)
        
    Returns:
        Bounding box as (x, y, width, height)
    """
    corners = tag.corners
    x_coords = [c[0] for c in corners]
    y_coords = [c[1] for c in corners]
    
    x_min, x_max = min(x_coords), max(x_coords)
    y_min, y_max = min(y_coords), max(y_coords)
    
    width = x_max - x_min
    height = y_max - y_min
    
    # Add padding
    pad_x = width * padding
    pad_y = height * padding
    
    x = int(x_min - pad_x)
    y = int(y_min - pad_y)
    w = int(width + 2 * pad_x)
    h = int(height + 2 * pad_y)
    
    return (x, y, w, h)


def init_opencv_tracker(state: HybridTrackingState, frame: np.ndarray, 
                        bbox: tuple[int, int, int, int]) -> bool:
    """
    Initialize or reinitialize the OpenCV tracker with a bounding box.
    
    IMPORTANT: This creates a FRESH tracker each time, using the current frame's
    pixel content within the bbox as the template to track. Call this whenever
    AprilTag provides ground truth to update what pixels OpenCV should track.
    
    Also stores the template image for template matching fallback.
    
    Uses CSRT tracker for good accuracy at reasonable speed.
    
    Args:
        state: HybridTrackingState to update
        frame: Current BGR frame
        bbox: Bounding box (x, y, width, height)
        
    Returns:
        True if initialization succeeded
    """
    try:
        # Clamp bbox to frame bounds
        h, w = frame.shape[:2]
        x, y, bw, bh = bbox
        x = max(0, x)
        y = max(0, y)
        bw = min(bw, w - x)
        bh = min(bh, h - y)
        
        # Ensure minimum size for tracker
        if bw < 10 or bh < 10:
            state.opencv_initialized = False
            return False
        
        clamped_bbox = (x, y, bw, bh)
        
        # Store the template image for template matching fallback
        # This captures the CURRENT visual appearance of the tag
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        state.template_image = gray[y:y+bh, x:x+bw].copy()
        
        # Always create a FRESH tracker - this captures the current visual appearance
        state.opencv_tracker = None  # Clear old tracker
        
        # Try different tracker APIs based on OpenCV version
        tracker = None
        try:
            tracker = cv2.TrackerCSRT_create()
        except AttributeError:
            try:
                tracker = cv2.legacy.TrackerCSRT_create()
            except AttributeError:
                try:
                    tracker = cv2.TrackerKCF_create()
                except AttributeError:
                    try:
                        tracker = cv2.legacy.TrackerKCF_create()
                    except AttributeError:
                        # No tracker available - will rely on template matching
                        state.opencv_initialized = False
                        state.last_bbox = clamped_bbox
                        return state.template_image is not None
        
        if tracker is None:
            state.opencv_initialized = False
            state.last_bbox = clamped_bbox
            return state.template_image is not None
        
        # Initialize tracker with the current frame and bbox
        success = tracker.init(frame, clamped_bbox)
        
        if success:
            state.opencv_tracker = tracker
            state.opencv_initialized = True
            state.last_bbox = clamped_bbox
            return True
        else:
            # Tracker failed but we still have template for fallback
            state.opencv_initialized = False
            state.last_bbox = clamped_bbox
            return state.template_image is not None
        
    except Exception as e:
        print(f"OpenCV tracker init error: {e}")
        state.opencv_initialized = False
        return False


def update_opencv_tracker(state: HybridTrackingState, frame: np.ndarray) -> Optional[tuple[float, float]]:
    """
    Update the OpenCV tracker and return the new center position.
    
    The tracker follows the visual appearance captured during init_opencv_tracker.
    Falls back to template matching in a larger search area if CSRT/KCF fails.
    
    Args:
        state: HybridTrackingState with initialized tracker or template_image
        frame: Current BGR frame
        
    Returns:
        Center position (x, y) or None if tracking failed
    """
    result = None
    
    # Try CSRT/KCF tracker first if available
    if state.opencv_initialized and state.opencv_tracker is not None:
        try:
            success, bbox = state.opencv_tracker.update(frame)
            
            if success:
                x, y, w, h = bbox
                # Validate bbox is reasonable
                frame_h, frame_w = frame.shape[:2]
                if 0 <= x and 0 <= y and x + w <= frame_w and y + h <= frame_h and w >= 5 and h >= 5:
                    center_x = x + w / 2.0
                    center_y = y + h / 2.0
                    state.last_bbox = (int(x), int(y), int(w), int(h))
                    result = (center_x, center_y)
                
        except Exception as e:
            pass  # Will try template matching fallback
    
    # If CSRT/KCF failed or unavailable, try template matching with larger search area
    if result is None and state.template_image is not None and state.last_bbox is not None:
        result = _template_match_fallback(state, frame)
    
    return result


def _template_match_fallback(state: HybridTrackingState, frame: np.ndarray) -> Optional[tuple[float, float]]:
    """
    Fallback tracking using template matching with a large search area.
    
    Uses the stored template (captured when AprilTag was last detected) to find
    the tag in the current frame. This is slower than CSRT but more robust for 
    fast-moving objects.
    
    Args:
        state: HybridTrackingState with template_image and last_bbox set
        frame: Current BGR frame
        
    Returns:
        Center position (x, y) or None if tracking failed
    """
    # Need stored template and last known position
    if state.template_image is None or state.last_bbox is None:
        return None
    
    try:
        template = state.template_image
        th, tw = template.shape[:2]
        
        if th < 5 or tw < 5:
            return None
        
        x, y, w, h = state.last_bbox
        frame_h, frame_w = frame.shape[:2]
        
        # Define a large search area around last known position
        # Use a bigger margin for fast-moving objects
        search_margin = max(tw, th) * 8  # 8x the template size
        
        # Center search around last bbox center
        last_cx = x + w / 2
        last_cy = y + h / 2
        
        search_x1 = max(0, int(last_cx - search_margin))
        search_y1 = max(0, int(last_cy - search_margin))
        search_x2 = min(frame_w, int(last_cx + search_margin))
        search_y2 = min(frame_h, int(last_cy + search_margin))
        
        # Convert to grayscale for template matching
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        search_region = gray[search_y1:search_y2, search_x1:search_x2]
        
        # Ensure search region is larger than template
        if search_region.shape[0] <= th or search_region.shape[1] <= tw:
            return None
        
        # Template matching using normalized cross-correlation
        result = cv2.matchTemplate(search_region, template, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, max_loc = cv2.minMaxLoc(result)
        
        # Accept matches with reasonable confidence
        if max_val > 0.5:
            # Convert back to frame coordinates
            match_x = search_x1 + max_loc[0] + tw / 2.0
            match_y = search_y1 + max_loc[1] + th / 2.0
            
            # Update bbox for next frame's search
            state.last_bbox = (int(search_x1 + max_loc[0]), int(search_y1 + max_loc[1]), tw, th)
            
            return (match_x, match_y)
        
        return None
        
    except Exception as e:
        return None


@dataclass
class HybridTrackingResult:
    """Result from hybrid tracking for a single frame."""
    center: tuple[float, float]
    source: TrackingSource
    tag: Optional[DetectedTag] = None
    confidence: float = 1.0  # 1.0 for AprilTag, decreases with interpolation frames


def update_hybrid_tracking(
    state: HybridTrackingState,
    detector: Detector,
    frame: np.ndarray,
    tracked_tag_id: int,
    tag_size_mm: float = 20.0,
    bbox_padding: float = 0.3
) -> Optional[HybridTrackingResult]:
    """
    Perform hybrid tracking: AprilTag as ground truth, OpenCV as fallback.
    
    Logic:
    1. Try AprilTag detection first
    2. If AprilTag detected: use it, reinitialize OpenCV tracker with current pixels
    3. If AprilTag NOT detected: use OpenCV tracker to interpolate
    4. If both fail: return None (tracking lost)
    
    Args:
        state: HybridTrackingState to update
        detector: AprilTag Detector instance
        frame: BGR frame to track in
        tracked_tag_id: The specific tag ID to track
        tag_size_mm: Physical size of the AprilTag in millimeters
        bbox_padding: Padding factor for OpenCV tracker bbox
        
    Returns:
        HybridTrackingResult with center position and source, or None if lost
    """
    state.tracked_tag_id = tracked_tag_id
    
    # Step 1: Try AprilTag detection
    detections = detect_apriltags(detector, frame, tag_size_mm)
    target_tag = None
    
    for det in detections:
        if det.tag_id == tracked_tag_id:
            target_tag = det
            break
    
    # Step 2: If AprilTag detected, use it as ground truth
    if target_tag is not None:
        center = target_tag.center
        state.last_apriltag_center = center
        state.frames_since_apriltag = 0
        state.last_source = TrackingSource.APRILTAG
        
        # ALWAYS reinitialize OpenCV tracker with current visual appearance
        bbox = compute_tag_bbox(target_tag, bbox_padding)
        init_opencv_tracker(state, frame, bbox)
        
        return HybridTrackingResult(
            center=center,
            source=TrackingSource.APRILTAG,
            tag=target_tag,
            confidence=1.0
        )
    
    # Step 3: AprilTag not detected - try OpenCV fallback
    state.frames_since_apriltag += 1
    
    # Check if we've exceeded max interpolation frames
    if state.is_tracking_lost:
        state.last_source = TrackingSource.LOST
        return None
    
    # Try OpenCV tracker
    opencv_center = update_opencv_tracker(state, frame)
    
    if opencv_center is not None:
        state.last_source = TrackingSource.OPENCV
        confidence = max(0.1, 1.0 - (state.frames_since_apriltag / state.max_interpolation_frames))
        
        return HybridTrackingResult(
            center=opencv_center,
            source=TrackingSource.OPENCV,
            tag=None,
            confidence=confidence
        )
    
    # Step 4: OpenCV failed - hold position briefly if just lost
    if state.last_apriltag_center is not None and state.frames_since_apriltag <= 3:
        state.last_source = TrackingSource.OPENCV
        confidence = max(0.1, 0.5 - (state.frames_since_apriltag * 0.1))
        return HybridTrackingResult(
            center=state.last_apriltag_center,
            source=TrackingSource.OPENCV,
            tag=None,
            confidence=confidence
        )
    
    # Tracking lost
    state.last_source = TrackingSource.LOST
    return None


def update_hybrid_tracking_multi(
    states: dict[int, HybridTrackingState],
    detector: Detector,
    frame: np.ndarray,
    tracked_tag_ids: set[int],
    tag_size_mm: float = 20.0,
    bbox_padding: float = 0.3
) -> dict[int, HybridTrackingResult]:
    """
    Perform hybrid tracking for multiple tags.
    
    Logic:
    - AprilTag detected: Use as ground truth, reinitialize OpenCV tracker with 
      the tag's visual appearance (pixels within bbox)
    - AprilTag NOT detected: Use OpenCV tracker to follow the visual pattern
    
    Args:
        states: Dictionary mapping tag_id to HybridTrackingState
        detector: AprilTag Detector instance
        frame: BGR frame to track in
        tracked_tag_ids: Set of tag IDs to track
        tag_size_mm: Physical size of the AprilTag in millimeters
        bbox_padding: Padding factor for OpenCV tracker bbox (0.3 = 30% padding)
        
    Returns:
        Dictionary mapping tag_id to HybridTrackingResult
    """
    results = {}
    
    # Ensure we have state objects for all tracked tags
    for tag_id in tracked_tag_ids:
        if tag_id not in states:
            states[tag_id] = HybridTrackingState(tracked_tag_id=tag_id)
    
    # Detect all AprilTags once (efficient)
    detections = detect_apriltags(detector, frame, tag_size_mm)
    detected_tags = {det.tag_id: det for det in detections}
    
    # Update each tracked tag
    for tag_id in tracked_tag_ids:
        state = states[tag_id]
        state.tracked_tag_id = tag_id
        
        if tag_id in detected_tags:
            # === APRILTAG DETECTED - GROUND TRUTH ===
            tag = detected_tags[tag_id]
            center = tag.center
            state.last_apriltag_center = center
            state.frames_since_apriltag = 0
            state.last_source = TrackingSource.APRILTAG
            
            # ALWAYS reinitialize OpenCV tracker with current visual appearance
            # This updates what pixels the tracker should follow
            bbox = compute_tag_bbox(tag, bbox_padding)
            init_opencv_tracker(state, frame, bbox)
            
            results[tag_id] = HybridTrackingResult(
                center=center,
                source=TrackingSource.APRILTAG,
                tag=tag,
                confidence=1.0
            )
        else:
            # === APRILTAG NOT DETECTED - TRY OPENCV FALLBACK ===
            state.frames_since_apriltag += 1
            
            # Check if we've exceeded max interpolation frames
            if state.is_tracking_lost:
                state.last_source = TrackingSource.LOST
                continue
            
            # Try OpenCV tracker
            opencv_center = update_opencv_tracker(state, frame)
            
            if opencv_center is not None:
                # OpenCV tracker succeeded - use its position
                state.last_source = TrackingSource.OPENCV
                # Confidence decays linearly with frames since AprilTag
                confidence = max(0.1, 1.0 - (state.frames_since_apriltag / state.max_interpolation_frames))
                
                results[tag_id] = HybridTrackingResult(
                    center=opencv_center,
                    source=TrackingSource.OPENCV,
                    tag=None,
                    confidence=confidence
                )
            elif state.last_apriltag_center is not None and state.frames_since_apriltag <= 3:
                # OpenCV failed but we just lost AprilTag - hold position briefly
                # This gives the tracker a chance on the next frame
                state.last_source = TrackingSource.OPENCV
                confidence = max(0.1, 0.5 - (state.frames_since_apriltag * 0.1))
                
                results[tag_id] = HybridTrackingResult(
                    center=state.last_apriltag_center,
                    source=TrackingSource.OPENCV,
                    tag=None,
                    confidence=confidence
                )
            else:
                # Both AprilTag and OpenCV failed - tracking lost
                state.last_source = TrackingSource.LOST
    
    return results
