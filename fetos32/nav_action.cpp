#include "nav_action.h"

NavAction map_gesture_to_nav(Event* e) {
  if (!e || !e->has_payload) return NAV_NONE;

  switch (e->payload.value) {
    case GESTURE_TAP: return NAV_NEXT;
    case GESTURE_DOUBLE_TAP: return NAV_BACK;
    case GESTURE_LONG_PRESS: return NAV_SELECT;
    case GESTURE_TRIPLE_TAP: return NAV_PREV;
  }

  return NAV_NONE;
}