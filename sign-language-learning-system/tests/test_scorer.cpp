#include "../src/ai/GestureScorer.h"

#include <cassert>
#include <cmath>
#include <vector>

static std::vector<HandKeypoint> makeHand(double offset)
{
    return {
        {0.50 + offset, 0.70 + offset, 0.00},
        {0.46 + offset, 0.64 + offset, 0.01}, {0.43 + offset, 0.58 + offset, 0.01}, {0.40 + offset, 0.52 + offset, 0.01}, {0.37 + offset, 0.46 + offset, 0.01},
        {0.47 + offset, 0.58 + offset, -0.01}, {0.46 + offset, 0.49 + offset, -0.01}, {0.45 + offset, 0.40 + offset, -0.01}, {0.44 + offset, 0.31 + offset, -0.01},
        {0.52 + offset, 0.57 + offset, -0.01}, {0.52 + offset, 0.47 + offset, -0.01}, {0.52 + offset, 0.37 + offset, -0.01}, {0.52 + offset, 0.27 + offset, -0.01},
        {0.57 + offset, 0.59 + offset, -0.01}, {0.59 + offset, 0.50 + offset, -0.01}, {0.61 + offset, 0.42 + offset, -0.01}, {0.62 + offset, 0.34 + offset, -0.01},
        {0.62 + offset, 0.63 + offset, -0.01}, {0.66 + offset, 0.56 + offset, -0.01}, {0.69 + offset, 0.50 + offset, -0.01}, {0.71 + offset, 0.44 + offset, -0.01}
    };
}

static std::vector<HandKeypoint> makeInvalidHand()
{
    return {
        {0.50, 0.50, 0.00},
        {0.51, 0.50, 0.00}, {0.52, 0.50, 0.00}, {0.53, 0.50, 0.00}, {0.54, 0.50, 0.00},
        {0.51, 0.51, 0.00}, {0.52, 0.51, 0.00}, {0.53, 0.51, 0.00}, {0.54, 0.51, 0.00},
        {0.51, 0.52, 0.00}, {0.52, 0.52, 0.00}, {0.53, 0.52, 0.00}, {0.54, 0.52, 0.00},
        {0.51, 0.53, 0.00}, {0.52, 0.53, 0.00}, {0.53, 0.53, 0.00}, {0.54, 0.53, 0.00},
        {0.51, 0.54, 0.00}, {0.52, 0.54, 0.00}, {0.53, 0.54, 0.00}, {0.54, 0.54, 0.00}
    };
}

int main()
{
    GestureScorer scorer;

    const auto reference = makeHand(0.0);
    const auto perfect = makeHand(0.0);
    const auto shifted = makeHand(0.25);

    const ScoreResult perfectScore = scorer.score(perfect, reference, 0.02);
    const ScoreResult shiftedScore = scorer.score(shifted, reference, 0.20);

    assert(perfectScore.totalScore > 80.0);
    assert(perfectScore.positionScore > shiftedScore.positionScore);
    assert(perfectScore.stabilityScore > shiftedScore.stabilityScore);
    assert(!perfectScore.feedback.empty());

    const auto invalid = makeInvalidHand();
    const ScoreResult invalidScore = scorer.score(invalid, reference, 0.02);
    assert(invalidScore.totalScore < 60.0);
    assert(invalidScore.feedback.find("未检测到有效手势") != std::string::npos
        || invalidScore.feedback.find("不符合目标手势") != std::string::npos);

    return 0;
}
