#include "ui/AppLogo.h"

#include <cassert>

int main()
{
    const QImage logo = createAppLogoHandGlyphImage(48);
    assert(!logo.isNull());
    assert(logo.width() == 48);
    assert(logo.height() == 48);

    int visiblePixels = 0;
    int yellowPixels = 0;
    int leftYellowPixels = 0;
    int rightYellowPixels = 0;
    for (int y = 0; y < logo.height(); ++y) {
        for (int x = 0; x < logo.width(); ++x) {
            const QColor color = QColor::fromRgba(logo.pixel(x, y));
            if (color.alpha() > 0) {
                ++visiblePixels;
            }
            if (color.alpha() > 0 && color.red() > 210 && color.green() > 145 && color.blue() < 140) {
                ++yellowPixels;
                if (x < logo.width() / 2) {
                    ++leftYellowPixels;
                } else {
                    ++rightYellowPixels;
                }
            }
        }
    }

    assert(visiblePixels > 0);
    assert(yellowPixels > 250);
    assert(leftYellowPixels > 80);
    assert(rightYellowPixels > 80);

    return 0;
}
