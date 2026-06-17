#include "models/Gesture.h"

#include <cassert>

int main()
{
    Gesture gesture;
    gesture.name = "你好";
    gesture.category = "常用词";
    gesture.difficulty = 1;

    assert(gestureDisplayText(gesture) == "你好  难度:1");
    assert(!gestureDisplayText(gesture).contains("["));
    assert(!gestureDisplayText(gesture).contains("常用词"));

    return 0;
}
