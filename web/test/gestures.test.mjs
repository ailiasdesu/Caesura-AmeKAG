// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { TouchGestureDetector } from '../touch-gestures.js'

describe('Mobile Touch Gesture Recognizer (M5)', () => {
  let detector
  let element

  beforeEach(() => {
    vi.useFakeTimers()
    element = document.createElement('div')
    document.body.appendChild(element)
    detector = new TouchGestureDetector()
    detector.attach(element)
  })

  afterEach(() => {
    detector.detach()
    if (element.parentElement) {
      element.parentElement.removeChild(element)
    }
    vi.useRealTimers()
  })

  describe('Two-Finger Tap', () => {
    it('detects two-finger tap released within timeout with minimal movement', () => {
      const onTwoFingerTap = vi.fn()
      detector.setCallbacks({ onTwoFingerTap })

      // Two fingers touch down at (100, 150) and (200, 150)
      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      vi.advanceTimersByTime(100)

      // Both fingers lift
      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 102, clientY: 152 },
        { identifier: 1, clientX: 201, clientY: 149 }
      ])

      expect(onTwoFingerTap).toHaveBeenCalledTimes(1)
      const eventArg = onTwoFingerTap.mock.calls[0][0]
      expect(eventArg.x).toBe(150) // centroid (100 + 200) / 2
      expect(eventArg.y).toBe(150)
    })

    it('rejects two-finger tap if fingers move beyond threshold', () => {
      const onTwoFingerTap = vi.fn()
      detector.setCallbacks({ onTwoFingerTap })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      // Move finger 0 significantly (35px > 20px threshold)
      detector.simulateTouch('touchmove', [
        { identifier: 0, clientX: 135, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 135, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      expect(onTwoFingerTap).not.toHaveBeenCalled()
    })

    it('rejects two-finger tap if held longer than max duration', () => {
      const onTwoFingerTap = vi.fn()
      detector.setCallbacks({ onTwoFingerTap })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      vi.advanceTimersByTime(400) // 400ms > 300ms threshold

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 100, clientY: 150 },
        { identifier: 1, clientX: 200, clientY: 150 }
      ])

      expect(onTwoFingerTap).not.toHaveBeenCalled()
    })
  })

  describe('Three-Finger Hold (Skip Mode)', () => {
    it('activates skip mode after 3 fingers are held for >= 200ms', () => {
      const onThreeFingerHold = vi.fn()
      detector.setCallbacks({ onThreeFingerHold })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 50, clientY: 100 },
        { identifier: 1, clientX: 100, clientY: 100 },
        { identifier: 2, clientX: 150, clientY: 100 }
      ])

      expect(onThreeFingerHold).not.toHaveBeenCalled()

      vi.advanceTimersByTime(200)

      expect(onThreeFingerHold).toHaveBeenCalledTimes(1)
      expect(onThreeFingerHold).toHaveBeenCalledWith(
        expect.objectContaining({ active: true, x: 100, y: 100 })
      )

      // Releasing fingers ends skip mode
      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 50, clientY: 100 }
      ])

      expect(onThreeFingerHold).toHaveBeenCalledTimes(2)
      expect(onThreeFingerHold).toHaveBeenLastCalledWith(
        expect.objectContaining({ active: false })
      )
    })

    it('cancels hold timer if any finger lifts before 200ms', () => {
      const onThreeFingerHold = vi.fn()
      detector.setCallbacks({ onThreeFingerHold })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 50, clientY: 100 },
        { identifier: 1, clientX: 100, clientY: 100 },
        { identifier: 2, clientX: 150, clientY: 100 }
      ])

      vi.advanceTimersByTime(100)

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 50, clientY: 100 }
      ])

      vi.advanceTimersByTime(150)

      expect(onThreeFingerHold).not.toHaveBeenCalled()
    })
  })

  describe('Vertical Swipes (SwipeDown & SwipeUp)', () => {
    it('detects single finger swipe down (dy >= 50px, vertical dominance)', () => {
      const onSwipeDown = vi.fn()
      detector.setCallbacks({ onSwipeDown })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 100 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchmove', [
        { identifier: 0, clientX: 105, clientY: 180 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 105, clientY: 190 }
      ])

      expect(onSwipeDown).toHaveBeenCalledTimes(1)
      const eventArg = onSwipeDown.mock.calls[0][0]
      expect(eventArg.startY).toBe(100)
      expect(eventArg.endY).toBe(190)
      expect(eventArg.deltaY).toBe(90)
      expect(eventArg.distance).toBe(90)
    })

    it('detects single finger swipe up (dy <= -50px, vertical dominance)', () => {
      const onSwipeUp = vi.fn()
      detector.setCallbacks({ onSwipeUp })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 200 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchmove', [
        { identifier: 0, clientX: 98, clientY: 110 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 98, clientY: 110 }
      ])

      expect(onSwipeUp).toHaveBeenCalledTimes(1)
      const eventArg = onSwipeUp.mock.calls[0][0]
      expect(eventArg.startY).toBe(200)
      expect(eventArg.endY).toBe(110)
      expect(eventArg.deltaY).toBe(-90)
      expect(eventArg.distance).toBe(90)
    })

    it('ignores horizontal movement as vertical swipe', () => {
      const onSwipeDown = vi.fn()
      const onSwipeUp = vi.fn()
      detector.setCallbacks({ onSwipeDown, onSwipeUp })

      detector.simulateTouch('touchstart', [
        { identifier: 0, clientX: 100, clientY: 100 }
      ])

      vi.advanceTimersByTime(50)

      // Move horizontally (dx = 100, dy = 10)
      detector.simulateTouch('touchmove', [
        { identifier: 0, clientX: 200, clientY: 110 }
      ])

      vi.advanceTimersByTime(50)

      detector.simulateTouch('touchend', [
        { identifier: 0, clientX: 200, clientY: 110 }
      ])

      expect(onSwipeDown).not.toHaveBeenCalled()
      expect(onSwipeUp).not.toHaveBeenCalled()
    })
  })
})
