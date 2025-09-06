#pragma once
#include <algorithm>
#include <cstdint>

struct Viewport {
    int x = 0, y = 0, w = 0, h = 0;
    int scale = 1; // integer scale factor applied to logical units
};

// Compute a centered, integer-scaled viewport for pixel-perfect rendering.
// If the fb is too small, it clamps at 1x and letterboxes the rest.
inline Viewport ComputePixelPerfectViewport(int fbW, int fbH, int logicalW, int logicalH) {
    Viewport vp;
    if (logicalW <= 0 || logicalH <= 0 || fbW <= 0 || fbH <= 0) {
        vp.w = fbW; vp.h = fbH; vp.scale = 1; vp.x = 0; vp.y = 0; 
        return vp;
    }
    int sW = fbW / logicalW;  // integer division
    int sH = fbH / logicalH;
    int S  = std::max(1, std::min(sW, sH)); // pick the largest integer that fits
    vp.scale = S;

    vp.w = logicalW * S;
    vp.h = logicalH * S;
    vp.x = (fbW - vp.w) / 2;
    vp.y = (fbH - vp.h) / 2;
    return vp;
}
