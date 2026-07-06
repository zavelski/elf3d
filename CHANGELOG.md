# Changelog

## 0.7.7

- Added Space + left-drag look-around that rotates the camera from its current
  world position for the duration of the gesture.
- Kept in-place look-around latched until the active drag ends, including when
  Space is released during the drag.
- Routed viewer mouse-wheel navigation directly from GLFW so dolly input is
  not deferred behind queued pointer events after captured navigation.

## 0.7.6

- Improved rendering reliability for valid scenes that use very small nested
  model scales.
- Added a practical viewer guide and a complete controls, menus, measurement,
  clipping, and navigation reference.
- Added a self-contained public documentation set for building, testing, glTF
  compatibility, rendering, and C++ API integration.
- Updated the Windows viewer package to include the user guide and reference.
