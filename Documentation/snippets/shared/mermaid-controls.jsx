import { useEffect, useCallback } from "react";

export const MermaidControls = () => {
  const attachInteractionBehavior = useCallback((container) => {
    // Find the .mermaid wrapper that has the CSS transform
    const mermaidWrapper = container.querySelector(".mermaid");
    if (!mermaidWrapper) return () => {};

    let isDragging = false;
    let startX = 0;
    let startY = 0;

    // Current transform state
    let translateX = 0;
    let translateY = 0;
    let scale = 1;

    // Pinch zoom state
    let initialPinchDistance = 0;
    let initialPinchScale = 1;
    let initialPinchTranslate = { x: 0, y: 0 };

    // Parse current transform from the element
    const parseTransform = () => {
      const style = mermaidWrapper.style.transform;
      const translateMatch = style.match(/translate\(([^,]+),\s*([^)]+)\)/);
      const scaleMatch = style.match(/scale\(([^)]+)\)/);

      if (translateMatch) {
        translateX = parseFloat(translateMatch[1]) || 0;
        translateY = parseFloat(translateMatch[2]) || 0;
      }
      if (scaleMatch) {
        scale = parseFloat(scaleMatch[1]) || 1;
      }
    };

    // Apply transform to the element
    const applyTransform = (animate = false) => {
      mermaidWrapper.style.transition = animate
        ? "transform 0.15s ease-out"
        : "none";
      mermaidWrapper.style.transform = `translate(${translateX}px, ${translateY}px) scale(${scale})`;
    };

    // Calculate distance between two touch points
    const getTouchDistance = (touches) => {
      const dx = touches[0].clientX - touches[1].clientX;
      const dy = touches[0].clientY - touches[1].clientY;
      return Math.sqrt(dx * dx + dy * dy);
    };

    // Get center point between two touches
    const getTouchCenter = (touches) => ({
      x: (touches[0].clientX + touches[1].clientX) / 2,
      y: (touches[0].clientY + touches[1].clientY) / 2,
    });

    // Zoom at a specific point
    const zoomAtPoint = (clientX, clientY, zoomFactor, animate = true) => {
      const rect = container.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      // Point relative to container center
      const pointX = clientX - centerX;
      const pointY = clientY - centerY;

      const newScale = Math.max(0.5, Math.min(5, scale * zoomFactor));
      const scaleRatio = newScale / scale;

      // Adjust translate to zoom toward the point
      translateX = pointX - (pointX - translateX) * scaleRatio;
      translateY = pointY - (pointY - translateY) * scaleRatio;
      scale = newScale;

      applyTransform(animate);
    };

    // Mouse handlers
    const handleMouseDown = (e) => {
      if (e.target.closest('[data-component-name="mermaid-controls-wrapper"]'))
        return;

      isDragging = true;
      startX = e.clientX;
      startY = e.clientY;
      parseTransform();
      container.style.cursor = "grabbing";
      e.preventDefault();
    };

    const handleMouseMove = (e) => {
      if (!isDragging) return;

      const deltaX = e.clientX - startX;
      const deltaY = e.clientY - startY;

      translateX += deltaX;
      translateY += deltaY;

      applyTransform(false);

      startX = e.clientX;
      startY = e.clientY;
    };

    const handleMouseUp = () => {
      isDragging = false;
      container.style.cursor = "grab";
    };

    // Double-click to zoom in
    const handleDoubleClick = (e) => {
      if (e.target.closest('[data-component-name="mermaid-controls-wrapper"]'))
        return;

      e.preventDefault();
      parseTransform();
      zoomAtPoint(e.clientX, e.clientY, 1.5, true);
    };

    // Touch handlers
    const handleTouchStart = (e) => {
      if (e.target.closest('[data-component-name="mermaid-controls-wrapper"]'))
        return;

      parseTransform();

      if (e.touches.length === 1) {
        isDragging = true;
        startX = e.touches[0].clientX;
        startY = e.touches[0].clientY;
        e.preventDefault();
      } else if (e.touches.length === 2) {
        isDragging = false;
        initialPinchDistance = getTouchDistance(e.touches);
        initialPinchScale = scale;
        initialPinchTranslate = { x: translateX, y: translateY };
        e.preventDefault();
      }
    };

    const handleTouchMove = (e) => {
      if (e.target.closest('[data-component-name="mermaid-controls-wrapper"]'))
        return;

      if (e.touches.length === 1 && isDragging) {
        const deltaX = e.touches[0].clientX - startX;
        const deltaY = e.touches[0].clientY - startY;

        translateX += deltaX;
        translateY += deltaY;

        applyTransform(false);

        startX = e.touches[0].clientX;
        startY = e.touches[0].clientY;
        e.preventDefault();
      } else if (e.touches.length === 2 && initialPinchDistance > 0) {
        const currentDistance = getTouchDistance(e.touches);
        const pinchScale = currentDistance / initialPinchDistance;
        const newScale = Math.max(
          0.5,
          Math.min(5, initialPinchScale * pinchScale)
        );

        const center = getTouchCenter(e.touches);
        const rect = container.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;

        // Point relative to container center
        const pointX = center.x - centerX;
        const pointY = center.y - centerY;

        const scaleRatio = newScale / initialPinchScale;

        // Adjust translate to zoom toward the pinch center
        translateX =
          pointX - (pointX - initialPinchTranslate.x) * scaleRatio;
        translateY =
          pointY - (pointY - initialPinchTranslate.y) * scaleRatio;
        scale = newScale;

        applyTransform(false);
        e.preventDefault();
      }
    };

    const handleTouchEnd = (e) => {
      if (e.touches.length === 0) {
        isDragging = false;
        initialPinchDistance = 0;
      } else if (e.touches.length === 1) {
        // Switched from pinch to single touch
        initialPinchDistance = 0;
        isDragging = true;
        startX = e.touches[0].clientX;
        startY = e.touches[0].clientY;
        parseTransform();
      }
    };

    // Wheel zoom
    const handleWheel = (e) => {
      if (e.target.closest('[data-component-name="mermaid-controls-wrapper"]'))
        return;

      e.preventDefault();
      parseTransform();
      const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
      zoomAtPoint(e.clientX, e.clientY, zoomFactor, false);
    };

    // Initialize
    parseTransform();
    container.style.cursor = "grab";

    // Attach event listeners to container
    container.addEventListener("mousedown", handleMouseDown);
    document.addEventListener("mousemove", handleMouseMove);
    document.addEventListener("mouseup", handleMouseUp);
    container.addEventListener("dblclick", handleDoubleClick);
    container.addEventListener("wheel", handleWheel, { passive: false });

    container.addEventListener("touchstart", handleTouchStart, {
      passive: false,
    });
    container.addEventListener("touchmove", handleTouchMove, { passive: false });
    container.addEventListener("touchend", handleTouchEnd);

    // Return cleanup function
    return () => {
      container.removeEventListener("mousedown", handleMouseDown);
      document.removeEventListener("mousemove", handleMouseMove);
      document.removeEventListener("mouseup", handleMouseUp);
      container.removeEventListener("dblclick", handleDoubleClick);
      container.removeEventListener("wheel", handleWheel);

      container.removeEventListener("touchstart", handleTouchStart);
      container.removeEventListener("touchmove", handleTouchMove);
      container.removeEventListener("touchend", handleTouchEnd);

      container.style.cursor = "";
    };
  }, []);

  useEffect(() => {
    const cleanupFunctions = new Map();

    // Attach behavior to a container once its .mermaid wrapper has an SVG
    const setupContainer = (container) => {
      // Skip if already set up
      if (cleanupFunctions.has(container)) return;

      const mermaidWrapper = container.querySelector(".mermaid");
      if (!mermaidWrapper) return;

      const svg = mermaidWrapper.querySelector("svg");
      if (!svg) return;

      const cleanup = attachInteractionBehavior(container);
      cleanupFunctions.set(container, cleanup);
    };

    // Check all existing containers
    const checkContainers = () => {
      const containers = document.querySelectorAll(
        '[data-component-name="mermaid-container"]'
      );
      containers.forEach(setupContainer);
    };

    // Initial check
    checkContainers();

    // Watch for DOM changes (new containers or SVGs being added)
    const observer = new MutationObserver((mutations) => {
      let shouldCheck = false;

      for (const mutation of mutations) {
        if (mutation.addedNodes.length > 0) {
          shouldCheck = true;
          break;
        }
      }

      if (shouldCheck) {
        checkContainers();
      }
    });

    // Observe the entire document for changes
    observer.observe(document.body, {
      childList: true,
      subtree: true,
    });

    return () => {
      observer.disconnect();
      cleanupFunctions.forEach((cleanup) => cleanup());
      cleanupFunctions.clear();
    };
  }, [attachInteractionBehavior]);

  return null;
};