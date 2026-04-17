#version 120

// Allows for about 8 million draws per frame
// Note: 1 << 22 = 4194304, calculated manually for GLSL 1.20 compatibility (no bitwise ops)
const float DEPTH_INCREMENT = 1.0 / 4194304.0;

uniform vec2 uScreenSize;

// clang-format off
attribute vec4 vBounds;
attribute float vColour;
attribute float vDepth;
attribute vec4 vVertMat;  // Stored as vec4, used as mat4x2
// clang-format on

varying float fColour;

void main()
{
    // Reconstruct mat4x2 from vec4
    mat4x2 vVertMatMtx = mat4x2(
        vVertMat.xy,
        vVertMat.zw,
        vec2(0.0, 0.0),
        vec2(0.0, 0.0)
    );
    
    vec2 pos = vVertMatMtx * vBounds;

    // Transform screen coordinates to viewport coordinates
    pos = (pos * (2.0 / uScreenSize)) - 1.0;
    pos.y *= -1.0;
    float depth = 1.0 - (vDepth + 1.0) * DEPTH_INCREMENT;

    fColour = vColour;

    gl_Position = vec4(pos, depth, 1.0);
}
