#pragma once

#include <string>
#include <vector>

struct HandKeypoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ScoreResult {
    double positionScore = 0.0;
    double angleScore = 0.0;
    double stabilityScore = 0.0;
    double totalScore = 0.0;
    std::string feedback;
};

class GestureScorer {
public:
    ScoreResult score(const std::vector<HandKeypoint>& detected,
                      const std::vector<HandKeypoint>& reference,
                      double stabilityDeviation) const;

private:
    double positionScore(const std::vector<HandKeypoint>& detected,
                         const std::vector<HandKeypoint>& reference) const;
    double angleScore(const std::vector<HandKeypoint>& detected,
                      const std::vector<HandKeypoint>& reference) const;
    double stabilityScore(double stabilityDeviation) const;
    static double distance(const HandKeypoint& a, const HandKeypoint& b);
    static double clampScore(double value);
};

