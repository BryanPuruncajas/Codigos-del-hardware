#pragma once
#include <math.h>

namespace BalloonCourse {
// Arena coordinates: +X from waypoint 1 to 2, +Y from 1 to 4.
// Headings are CCW-positive, matching the existing yaw convention.
struct Point { float x; float y; };
constexpr Point points[] = {{0, 0}, {4, 0}, {4, 2}, {0, 2}};
constexpr int rectangle[] = {1, 2, 3, 4, 1};
constexpr int infinity[] = {1, 3, 4, 2, 1};
inline int waypoint(int track, int contactIndex) {
    if (contactIndex < 0 || contactIndex > 4) return 0;
    return track == 2 ? infinity[contactIndex] : rectangle[contactIndex];
}
inline float heading(int track, int legIndex) {
    if (legIndex < 0 || legIndex > 3) return NAN;
    const Point a = points[waypoint(track, legIndex) - 1];
    const Point b = points[waypoint(track, legIndex + 1) - 1];
    return atan2f(b.y - a.y, b.x - a.x);
}
inline bool smallerNextTarget(float area, float visitedArea) {
    return isfinite(area) && isfinite(visitedArea) && area > 0 &&
           visitedArea > 0 && area < visitedArea * 0.5f;
}
}
