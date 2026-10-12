#pragma once
#include "display_policy.h"

namespace ICSMDisplay {
struct SceneResolution { Size input, output; };

// The recipe describes 3D pixels, independently of the native UI canvas.
// Fit smaller windows without exceeding either the recipe or the render target.
// Keep Source's truncation rule for the accepted high/480p scene scale.
inline SceneResolution ScenePlan(Size recipe, float percent, Size ui) {
    if (!recipe.width || !recipe.height || !ui.width || !ui.height)
        return {{0, 0}, {0, 0}};
    const double fit = (std::min)(1.0, (std::min)(double(ui.width) / recipe.width,
                                                               double(ui.height) / recipe.height));
    Size output = {(std::max)(1u, unsigned(std::floor(recipe.width * fit))),
                   (std::max)(1u, unsigned(std::floor(recipe.height * fit)))};
    const float scale = (std::max)(0.0f, (std::min)(1.0f, percent / 100.0f));
    Size input = {(std::max)(1u, unsigned(output.width * scale)),
                  (std::max)(1u, unsigned(output.height * scale))};
    return {input, output};
}
}
