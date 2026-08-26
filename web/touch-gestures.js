/**
 * Caesura Web Player - Mobile Touch Gesture Recognizer
 *
 * Recognized Gestures:
 * 1. Two-finger tap:
 *    - 2 fingers touch and release within 300ms, travel < 20px.
 *    - Action: open system menu / toggle backlog history.
 * 2. Three-finger hold:
 *    - 3 fingers held down for >= 200ms without excessive movement.
 *    - Action: fast-forward skip mode (active while fingers are held).
 * 3. Swipe-down:
 *    - 1 finger vertical drag down (dy >= 50px, |dy| >= 1.5 * |dx|).
 *    - Action: hide dialogue box / UI overlay.
 * 4. Swipe-up:
 *    - 1 finger vertical drag up (dy <= -50px, |dy| >= 1.5 * |dx|).
 *    - Action: open backlog / history view.
 */

export class TouchGestureDetector {
  constructor(options = {}) {
    this.options = {
      twoFingerTapMaxDurationMs: options.twoFingerTapMaxDurationMs ?? 300,
      twoFingerTapMaxMovePx: options.twoFingerTapMaxMovePx ?? 20,
      threeFingerHoldMinDurationMs: options.threeFingerHoldMinDurationMs ?? 200,
      threeFingerHoldMaxMovePx: options.threeFingerHoldMaxMovePx ?? 25,
      swipeMinDistancePx: options.swipeMinDistancePx ?? 50,
      swipeDirectionRatio: options.swipeDirectionRatio ?? 1.5,
      ...options
    };

    this.callbacks = {
      onTwoFingerTap: options.onTwoFingerTap || null,
      onThreeFingerHold: options.onThreeFingerHold || null,
      onSwipeDown: options.onSwipeDown || null,
      onSwipeUp: options.onSwipeUp || null,
      onTap: options.onTap || null
    };

    this.element = null;
    this.touches = new Map(); // identifier -> { id, startX, startY, currentX, currentY, startTime, moved }
    this.threeFingerHoldTimer = null;
    this.threeFingerHoldActive = false;
    this.twoFingerGesture = null; // { startTime, startPoints: [], maxTravel: 0 }

    this._boundOnTouchStart = this.onTouchStart.bind(this);
    this._boundOnTouchMove = this.onTouchMove.bind(this);
    this._boundOnTouchEnd = this.onTouchEnd.bind(this);
    this._boundOnTouchCancel = this.onTouchCancel.bind(this);
  }

  /**
   * Set or update callback handlers.
   */
  setCallbacks(callbacks = {}) {
    Object.assign(this.callbacks, callbacks);
    return this;
  }

  /**
   * Attach gesture listeners to a DOM element.
   */
  attach(element) {
    if (!element) return this;
    if (this.element) this.detach();
    this.element = element;
    element.addEventListener('touchstart', this._boundOnTouchStart, { passive: false });
    element.addEventListener('touchmove', this._boundOnTouchMove, { passive: false });
    element.addEventListener('touchend', this._boundOnTouchEnd, { passive: false });
    element.addEventListener('touchcancel', this._boundOnTouchCancel, { passive: false });
    return this;
  }

  /**
   * Detach gesture listeners from the attached element.
   */
  detach() {
    if (!this.element) return this;
    this.element.removeEventListener('touchstart', this._boundOnTouchStart);
    this.element.removeEventListener('touchmove', this._boundOnTouchMove);
    this.element.removeEventListener('touchend', this._boundOnTouchEnd);
    this.element.removeEventListener('touchcancel', this._boundOnTouchCancel);
    this.reset();
    this.element = null;
    return this;
  }

  /**
   * Reset internal tracking state.
   */
  reset() {
    if (this.threeFingerHoldTimer) {
      clearTimeout(this.threeFingerHoldTimer);
      this.threeFingerHoldTimer = null;
    }
    if (this.threeFingerHoldActive) {
      this.threeFingerHoldActive = false;
      if (typeof this.callbacks.onThreeFingerHold === 'function') {
        this.callbacks.onThreeFingerHold({ active: false, timestamp: Date.now() });
      }
    }
    this.touches.clear();
    this.twoFingerGesture = null;
  }

  onTouchStart(event) {
    const now = Date.now();
    const changed = event.changedTouches || [event];

    for (let i = 0; i < changed.length; i++) {
      const t = changed[i];
      const id = t.identifier ?? i;
      this.touches.set(id, {
        id,
        startX: t.clientX,
        startY: t.clientY,
        currentX: t.clientX,
        currentY: t.clientY,
        startTime: now,
        moved: false,
        maxTravel: 0
      });
    }

    const count = this.touches.size;

    // Multi-touch gestures should prevent default pinch-zoom or scrolling
    if (count > 1 && event.cancelable) {
      event.preventDefault();
    }

    // Two-finger tap candidate setup
    if (count === 2) {
      const points = Array.from(this.touches.values());
      this.twoFingerGesture = {
        startTime: now,
        startPoints: points.map(p => ({ x: p.startX, y: p.startY })),
        maxTravel: 0
      };
    } else if (count !== 2) {
      this.twoFingerGesture = null;
    }

    // Three-finger hold setup
    if (count === 3) {
      if (this.threeFingerHoldTimer) clearTimeout(this.threeFingerHoldTimer);
      this.threeFingerHoldTimer = setTimeout(() => {
        if (this.touches.size === 3) {
          // Verify none exceeded hold max movement
          let maxTravel = 0;
          for (const touch of this.touches.values()) {
            const dx = touch.currentX - touch.startX;
            const dy = touch.currentY - touch.startY;
            const dist = Math.hypot(dx, dy);
            if (dist > maxTravel) maxTravel = dist;
          }

          if (maxTravel <= this.options.threeFingerHoldMaxMovePx) {
            this.threeFingerHoldActive = true;
            if (typeof this.callbacks.onThreeFingerHold === 'function') {
              const points = Array.from(this.touches.values());
              const centerX = points.reduce((s, p) => s + p.currentX, 0) / 3;
              const centerY = points.reduce((s, p) => s + p.currentY, 0) / 3;
              this.callbacks.onThreeFingerHold({
                active: true,
                x: centerX,
                y: centerY,
                timestamp: Date.now()
              });
            }
          }
        }
      }, this.options.threeFingerHoldMinDurationMs);
    } else {
      if (this.threeFingerHoldTimer) {
        clearTimeout(this.threeFingerHoldTimer);
        this.threeFingerHoldTimer = null;
      }
    }
  }

  onTouchMove(event) {
    const changed = event.changedTouches || [event];

    for (let i = 0; i < changed.length; i++) {
      const t = changed[i];
      const id = t.identifier ?? i;
      const record = this.touches.get(id);
      if (record) {
        record.currentX = t.clientX;
        record.currentY = t.clientY;
        const dx = t.clientX - record.startX;
        const dy = t.clientY - record.startY;
        const dist = Math.hypot(dx, dy);
        if (dist > record.maxTravel) record.maxTravel = dist;
        if (dist > 10) record.moved = true;
      }
    }

    if (this.twoFingerGesture) {
      for (const touch of this.touches.values()) {
        if (touch.maxTravel > this.twoFingerGesture.maxTravel) {
          this.twoFingerGesture.maxTravel = touch.maxTravel;
        }
      }
    }

    if (this.touches.size > 1 && event.cancelable) {
      event.preventDefault();
    }
  }

  onTouchEnd(event) {
    const now = Date.now();
    const changed = event.changedTouches || [event];
    const endingTouches = [];

    for (let i = 0; i < changed.length; i++) {
      const t = changed[i];
      const id = t.identifier ?? i;
      const record = this.touches.get(id);
      if (record) {
        record.currentX = t.clientX;
        record.currentY = t.clientY;
        endingTouches.push(record);
        this.touches.delete(id);
      }
    }

    // 1. Three-finger hold release
    if (this.threeFingerHoldActive && this.touches.size < 3) {
      this.threeFingerHoldActive = false;
      if (typeof this.callbacks.onThreeFingerHold === 'function') {
        this.callbacks.onThreeFingerHold({ active: false, timestamp: now });
      }
    }
    if (this.threeFingerHoldTimer && this.touches.size < 3) {
      clearTimeout(this.threeFingerHoldTimer);
      this.threeFingerHoldTimer = null;
    }

    // 2. Two-finger tap detection
    if (this.twoFingerGesture && this.touches.size === 0) {
      const duration = now - this.twoFingerGesture.startTime;
      if (
        duration <= this.options.twoFingerTapMaxDurationMs &&
        this.twoFingerGesture.maxTravel <= this.options.twoFingerTapMaxMovePx
      ) {
        if (typeof this.callbacks.onTwoFingerTap === 'function') {
          const centerX = this.twoFingerGesture.startPoints.reduce((s, p) => s + p.x, 0) / 2;
          const centerY = this.twoFingerGesture.startPoints.reduce((s, p) => s + p.y, 0) / 2;
          this.callbacks.onTwoFingerTap({ x: centerX, y: centerY, timestamp: now });
        }
        this.twoFingerGesture = null;
        if (event.cancelable) event.preventDefault();
        return;
      }
      this.twoFingerGesture = null;
    }

    // 3. Single-finger vertical swipe gestures (SwipeDown / SwipeUp)
    if (endingTouches.length === 1 && this.touches.size === 0) {
      const t = endingTouches[0];
      const dx = t.currentX - t.startX;
      const dy = t.currentY - t.startY;
      const absDx = Math.abs(dx);
      const absDy = Math.abs(dy);
      const duration = Math.max(1, now - t.startTime);

      if (absDy >= this.options.swipeMinDistancePx && absDy >= absDx * this.options.swipeDirectionRatio) {
        const velocity = absDy / duration;
        if (dy > 0) {
          // Swipe Down
          if (typeof this.callbacks.onSwipeDown === 'function') {
            this.callbacks.onSwipeDown({
              startX: t.startX,
              startY: t.startY,
              endX: t.currentX,
              endY: t.currentY,
              deltaX: dx,
              deltaY: dy,
              distance: absDy,
              velocity,
              timestamp: now
            });
          }
          if (event.cancelable) event.preventDefault();
          return;
        } else {
          // Swipe Up
          if (typeof this.callbacks.onSwipeUp === 'function') {
            this.callbacks.onSwipeUp({
              startX: t.startX,
              startY: t.startY,
              endX: t.currentX,
              endY: t.currentY,
              deltaX: dx,
              deltaY: dy,
              distance: absDy,
              velocity,
              timestamp: now
            });
          }
          if (event.cancelable) event.preventDefault();
          return;
        }
      }
    }
  }

  onTouchCancel(event) {
    this.reset();
  }

  /**
   * Helper for testing/simulating touch event sequences.
   */
  simulateTouch(type, touchList = []) {
    const touches = touchList.map((t, idx) => ({
      identifier: t.identifier ?? idx,
      clientX: t.clientX ?? t.x ?? 0,
      clientY: t.clientY ?? t.y ?? 0
    }));

    const mockEvent = {
      type,
      changedTouches: touches,
      preventDefault: () => {},
      cancelable: true
    };

    if (type === 'touchstart') this.onTouchStart(mockEvent);
    else if (type === 'touchmove') this.onTouchMove(mockEvent);
    else if (type === 'touchend') this.onTouchEnd(mockEvent);
    else if (type === 'touchcancel') this.onTouchCancel(mockEvent);
  }
}
