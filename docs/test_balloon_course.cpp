#include "../firmware/src/mission/BalloonCourse.h"
#include <cassert>
#include <cstdio>

int main() {
    const int rectangle[] = {1,2,3,4,1}, infinity[] = {1,3,4,2,1};
    const float pi = acosf(-1.0f);
    const float diagonal = atan2f(2.0f,4.0f);
    const float rectangleHeadings[] = {0,pi/2,pi,-pi/2};
    const float infinityHeadings[] = {diagonal,pi,-diagonal,pi};
    for (int i=0;i<5;++i) {
        assert(BalloonCourse::waypoint(1,i)==rectangle[i]);
        assert(BalloonCourse::waypoint(2,i)==infinity[i]);
    }
    for (int i=0;i<4;++i) {
        assert(fabsf(BalloonCourse::heading(1,i)-rectangleHeadings[i])<1e-5f);
        assert(fabsf(BalloonCourse::heading(2,i)-infinityHeadings[i])<1e-5f);
    }
    assert(isnan(BalloonCourse::heading(1,4)));
    assert(BalloonCourse::waypoint(2,-1)==0);
    assert(!BalloonCourse::smallerNextTarget(10000,10000));
    assert(!BalloonCourse::smallerNextTarget(0,10000));
    assert(!BalloonCourse::smallerNextTarget(5000,10000));
    assert(BalloonCourse::smallerNextTarget(1200,10000));
    assert(!BalloonCourse::smallerNextTarget(NAN,10000));
    puts("PASS: Track 1/2 sequences, all leg headings, closure and near-target rejection");
}
