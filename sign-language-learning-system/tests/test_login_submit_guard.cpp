#include "ui/LoginSubmitGuard.h"

#include <cassert>

int main()
{
    LoginSubmitGuard guard;

    assert(guard.tryBegin());
    assert(guard.isLocked());
    assert(!guard.tryBegin());

    guard.unlock();
    assert(!guard.isLocked());
    assert(guard.tryBegin());

    return 0;
}
