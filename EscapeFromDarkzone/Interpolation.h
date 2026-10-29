#pragma once
#include <cmath>

// 기존 60FPS에서 프레임당 8% 보간하던 감각을 경과 시간 기준으로 유지한다.
inline float ServerPositionBlend(float elapsedSeconds)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) return 0.0f;
    constexpr float responsePerSecond = 5.00289654f;
    return -std::expm1(-responsePerSecond * elapsedSeconds);
}
