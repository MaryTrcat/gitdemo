#include "GestureScorer.h"

#include <algorithm>
#include <cmath>

static double clampDouble(double value, double lower, double upper)
{
    return std::max(lower, std::min(value, upper));
}

static double pointDistance(const HandKeypoint& a, const HandKeypoint& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static std::vector<HandKeypoint> normalizeHand(const std::vector<HandKeypoint>& points)
{
    if (points.size() < 21) {
        return points;
    }

    const HandKeypoint origin = points[0];
    double scale = pointDistance(points[0], points[9]);
    if (scale < 0.0001) {
        scale = pointDistance(points[5], points[17]);
    }
    if (scale < 0.0001) {
        scale = 1.0;
    }

    std::vector<HandKeypoint> normalized;
    normalized.reserve(points.size());
    for (const HandKeypoint& point : points) {
        normalized.push_back({
            (point.x - origin.x) / scale,
            (point.y - origin.y) / scale,
            (point.z - origin.z) / scale
        });
    }
    return normalized;
}

static double handSpread(const std::vector<HandKeypoint>& points)
{
    if (points.size() < 21) {
        return 0.0;
    }
    double minX = points[0].x;
    double maxX = points[0].x;
    double minY = points[0].y;
    double maxY = points[0].y;
    for (const HandKeypoint& point : points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    const double width = maxX - minX;
    const double height = maxY - minY;
    return std::sqrt(width * width + height * height);
}

ScoreResult GestureScorer::score(const std::vector<HandKeypoint>& detected,
                                 const std::vector<HandKeypoint>& reference,
                                 double stabilityDeviation) const
{
    ScoreResult result;
    if (detected.size() == 21 && reference.size() == 21) {
        const double spread = handSpread(detected);
        if (spread < 0.12) {
            result.positionScore = 20.0;
            result.angleScore = 15.0;
            result.stabilityScore = stabilityScore(stabilityDeviation);
            result.totalScore = 25.0;
            result.feedback = "未检测到有效手势：手部关键点过于集中，请完整露出手掌并做出目标动作。";
            return result;
        }

        const std::vector<HandKeypoint> normalizedDetected = normalizeHand(detected);
        const std::vector<HandKeypoint> normalizedReference = normalizeHand(reference);
        result.positionScore = positionScore(normalizedDetected, normalizedReference);
        result.angleScore = angleScore(normalizedDetected, normalizedReference);
        result.stabilityScore = stabilityScore(stabilityDeviation);
        result.totalScore = result.positionScore * 0.42
            + result.angleScore * 0.43
            + result.stabilityScore * 0.15;

        if (result.totalScore >= 90.0) {
            result.feedback = "动作与目标手势匹配度较高，姿态清晰，完成度较好。";
        } else if (result.positionScore < 75.0) {
            result.feedback = "动作不符合目标手势：手掌和指尖整体位置偏差较大，请对照标准视频调整。";
        } else if (result.angleScore < 75.0) {
            result.feedback = "动作不符合目标手势：手指弯曲角度或手型差异较大，请重新摆出目标动作。";
        } else {
            result.feedback = "已检测到有效手势，但与目标动作仍有偏差，请继续对照标准视频练习。";
        }
        return result;
    }

    const std::vector<HandKeypoint> normalizedDetected = normalizeHand(detected);
    const std::vector<HandKeypoint> normalizedReference = normalizeHand(reference);
    result.positionScore = positionScore(normalizedDetected, normalizedReference);
    result.angleScore = angleScore(normalizedDetected, normalizedReference);
    result.stabilityScore = stabilityScore(stabilityDeviation);
    result.totalScore = result.positionScore * 0.45
        + result.angleScore * 0.35
        + result.stabilityScore * 0.20;

    if (result.totalScore >= 90.0) {
        result.feedback = "动作完成度很好，请继续保持。";
    } else if (result.positionScore < 70.0) {
        result.feedback = "手部整体位置偏差较大，请对照标准动作调整手掌和指尖位置。";
    } else if (result.angleScore < 70.0) {
        result.feedback = "手指弯曲角度不够准确，请重点观察各手指关节。";
    } else if (result.stabilityScore < 70.0) {
        result.feedback = "动作保持不够稳定，请放慢速度并固定手型。";
    } else {
        result.feedback = "动作基本正确，仍有轻微偏差，可以继续练习。";
    }

    return result;
}

double GestureScorer::positionScore(const std::vector<HandKeypoint>& detected,
                                    const std::vector<HandKeypoint>& reference) const
{
    if (detected.size() != reference.size() || detected.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (size_t i = 0; i < detected.size(); ++i) {
        total += distance(detected[i], reference[i]);
    }

    const double averageDistance = total / static_cast<double>(detected.size());
    return clampScore(100.0 - averageDistance * 38.0);
}

double GestureScorer::angleScore(const std::vector<HandKeypoint>& detected,
                                 const std::vector<HandKeypoint>& reference) const
{
    if (detected.size() != reference.size() || detected.size() < 21) {
        return 0.0;
    }

    const int joints[][3] = {
        {0, 1, 2}, {1, 2, 3}, {2, 3, 4},
        {0, 5, 6}, {5, 6, 7}, {6, 7, 8},
        {0, 9, 10}, {9, 10, 11}, {10, 11, 12},
        {0, 13, 14}, {13, 14, 15}, {14, 15, 16},
        {0, 17, 18}, {17, 18, 19}, {18, 19, 20}
    };

    auto angleAt = [](const std::vector<HandKeypoint>& points, int a, int b, int c) {
        const double ux = points[a].x - points[b].x;
        const double uy = points[a].y - points[b].y;
        const double uz = points[a].z - points[b].z;
        const double vx = points[c].x - points[b].x;
        const double vy = points[c].y - points[b].y;
        const double vz = points[c].z - points[b].z;
        const double dot = ux * vx + uy * vy + uz * vz;
        const double lenU = std::sqrt(ux * ux + uy * uy + uz * uz);
        const double lenV = std::sqrt(vx * vx + vy * vy + vz * vz);
        if (lenU == 0.0 || lenV == 0.0) {
            return 0.0;
        }
        const double cosValue = clampDouble(dot / (lenU * lenV), -1.0, 1.0);
        return std::acos(cosValue) * 180.0 / 3.14159265358979323846;
    };

    double totalDeviation = 0.0;
    for (const auto& joint : joints) {
        const double detectedAngle = angleAt(detected, joint[0], joint[1], joint[2]);
        const double referenceAngle = angleAt(reference, joint[0], joint[1], joint[2]);
        totalDeviation += std::abs(detectedAngle - referenceAngle);
    }

    const double averageDeviation = totalDeviation / 15.0;
    return clampScore(100.0 - averageDeviation * 1.8);
}

double GestureScorer::stabilityScore(double stabilityDeviation) const
{
    return clampScore(100.0 - stabilityDeviation * 250.0);
}

double GestureScorer::distance(const HandKeypoint& a, const HandKeypoint& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double GestureScorer::clampScore(double value)
{
    return clampDouble(value, 0.0, 100.0);
}
