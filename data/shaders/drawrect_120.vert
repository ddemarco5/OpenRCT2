#version 120

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(4194304.0);  // 1 << 22 = 4194304

uniform ivec2 uScreenSize;

attribute vec4 vClip;
attribute float vTexColourAtlas;
attribute vec4 vTexColourCoords;
attribute float vTexMaskAtlas;
attribute vec4 vTexMaskCoords;
attribute vec3 vPalettes;
attribute float vFlags;
attribute float vColour;
attribute vec4 vBounds;
attribute float vDepth;
attribute float vZoom;

attribute mat4x2 vVertMat;
attribute vec2 vVertVec;

varying vec2 fPosition;
varying vec3 fPeelPos;
varying float fFlags;
varying float fColour;
varying vec4 fTexColour;
varying vec4 fTexMask;
varying vec3 fPalettes;
varying float fZoom;
varying float fTexColourAtlas;
varying float fTexMaskAtlas;
varying float fScreenHeight;

void main()
{
    // Clamp position by vClip, correcting interpolated values for the clipping
    vec2 m = clamp(
        ((vVertMat * vClip) - (vVertMat * vBounds)) / (vBounds.zw - vBounds.xy) + vVertVec, 0.0, 1.0);
    vec2 pos = mix(vBounds.xy, vBounds.zw, m);
    fTexColour = vTexColourCoords;
    fTexMask = vTexMaskCoords;

    fPosition = vBounds.xy;
    fZoom = vZoom;
    fTexColourAtlas = vTexColourAtlas;
    fTexMaskAtlas = vTexMaskAtlas;

    // Transform screen coordinates to texture coordinates
    float depth = 1.0 - (vDepth + 1.0) * DEPTH_INCREMENT;
    pos = pos / vec2(uScreenSize);
    pos.y = pos.y * -1.0 + 1.0;
    fPeelPos = vec3(pos, depth * 0.5 + 0.5);

    fFlags = vFlags;
    fColour = vColour;
    fPalettes = vPalettes;

    fScreenHeight = float(uScreenSize.y);

    // Transform texture coordinates to viewport coordinates
    pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, depth, 1.0);
}
