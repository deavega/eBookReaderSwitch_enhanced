#include "TouchGesture.hpp"
#include <cmath>

// Tuned for the Switch's 1280x720 touch panel.
static const float TAP_MAX_TRAVEL   = 28.0f; // stay under this -> it's a tap
static const float SWIPE_MIN_TRAVEL = 90.0f; // dominant-axis travel -> a swipe

void TouchGesture::update(const HidTouchScreenState &state) {
    _has_gesture = false;

    if (state.count > 0) {
        float x = (float) state.touches[0].x;
        float y = (float) state.touches[0].y;

        if (!_active) {
            // Finger just went down: start tracking.
            _active = true;
            _start_x = _last_x = x;
            _start_y = _last_y = y;
            _max_dist = 0;
            _pending_dx = _pending_dy = 0;
        } else {
            // Finger moved: accumulate the per-frame delta for panning and
            // remember how far it has strayed from the start (tap vs swipe).
            _pending_dx += x - _last_x;
            _pending_dy += y - _last_y;
            _last_x = x;
            _last_y = y;

            float ox = x - _start_x, oy = y - _start_y;
            float dist = sqrtf(ox * ox + oy * oy);
            if (dist > _max_dist) _max_dist = dist;
        }
    } else if (_active) {
        // Finger just lifted: classify the completed gesture.
        _active = false;

        float dx = _last_x - _start_x;
        float dy = _last_y - _start_y;

        TouchGestureResult r;
        r.x  = (int) _start_x;
        r.y  = (int) _start_y;
        r.dx = dx;
        r.dy = dy;

        if (_max_dist < TAP_MAX_TRAVEL) {
            r.type = TouchGestureTap;
        } else if (fabsf(dx) >= fabsf(dy) && fabsf(dx) >= SWIPE_MIN_TRAVEL) {
            r.type = (dx < 0) ? TouchGestureSwipeLeft : TouchGestureSwipeRight;
        } else if (fabsf(dy) > fabsf(dx) && fabsf(dy) >= SWIPE_MIN_TRAVEL) {
            r.type = (dy < 0) ? TouchGestureSwipeUp : TouchGestureSwipeDown;
        } else {
            // Ambiguous short drift: treat as a tap where it began.
            r.type = TouchGestureTap;
        }

        _gesture = r;
        _has_gesture = true;
        _pending_dx = _pending_dy = 0;
    }
}

bool TouchGesture::consume_drag(float &out_dx, float &out_dy) {
    if (!_active || (_pending_dx == 0 && _pending_dy == 0))
        return false;

    out_dx = _pending_dx;
    out_dy = _pending_dy;
    _pending_dx = _pending_dy = 0;
    return true;
}

bool TouchGesture::consume_gesture(TouchGestureResult &out) {
    if (!_has_gesture) return false;

    out = _gesture;
    _has_gesture = false;
    return true;
}
