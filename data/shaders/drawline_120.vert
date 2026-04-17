#version 120

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(4194304.0);  // 1 << 22 = 4194304

uniform vec2 uScreenSize;

attribute vec4 vBounds;
attribute float vColour;
attribute float vDepth;

attribute mat4x2 vVertMat;

varying float fColour;

void main()
{
    vec2 pos = vVertMat * vBounds;

    // Transform screen coordinates to viewport coordinates
    pos = (pos * (2.0 / uScreenSize)) - 1.0;
    pos.y *= -1.0;
    float depth = 1.0 - (vDepth + 1.0) * DEPTH_INCREMENT;

    fColour = vColour;

    gl_Position = vec4(pos, depth, 1.0);
}
