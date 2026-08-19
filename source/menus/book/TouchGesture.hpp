#ifndef EBOOK_READER_TOUCH_GESTURE_HPP
#define EBOOK_READER_TOUCH_GESTURE_HPP

#include <switch.h>

typedef enum {
    TouchGestureNone = 0,
    TouchGestureTap,
    TouchGestureSwipeLeft,
    TouchGestureSwipeRight,
    TouchGestureSwipeUp,
    TouchGestureSwipeDown
} TouchGestureType;

struct TouchGestureResult {
    TouchGestureType type = TouchGestureNone;
    int x = 0;      // where the finger first went down
    int y = 0;
    float dx = 0;   // total horizontal travel (end - start)
    float dy = 0;   // total vertical travel   (end - start)
};

// Recognises taps and directional swipes from the Switch touch screen and
// exposes per-frame drag deltas so a zoomed-in page can be panned while a
// finger is held down. Only the primary finger (touches[0]) is tracked, which
// is all a reader needs. Call update() exactly once per frame.
class TouchGesture {
    public:
        void update(const HidTouchScreenState &state);

        // True while a finger is currently on the screen.
        bool is_active() const { return _active; }

        // Movement (pixels) of the primary finger since the previous frame.
        // Returns false when there was no movement this frame; clears the
        // accumulated delta when it returns true.
        bool consume_drag(float &out_dx, float &out_dy);

        // True on the single frame the finger is lifted, filling `out` with the
        // recognised gesture (a tap or a directional swipe).
        bool consume_gesture(TouchGestureResult &out);

    private:
        bool  _active = false;
        float _start_x = 0, _start_y = 0;
        float _last_x = 0,  _last_y = 0;
        float _max_dist = 0;      // furthest distance travelled from the start
        float _pending_dx = 0;    // unread per-frame drag for panning
        float _pending_dy = 0;

        bool _has_gesture = false;
        TouchGestureResult _gesture;
};

#endif
