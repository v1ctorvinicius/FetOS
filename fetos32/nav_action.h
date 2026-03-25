#include "event.h"
#include "button_gesture.h"

#pragma once

enum NavAction {
  NAV_NONE,
  NAV_NEXT,
  NAV_PREV,
  NAV_SELECT,
  NAV_BACK
};

NavAction map_gesture_to_nav(Event* e);