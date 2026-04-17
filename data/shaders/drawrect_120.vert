#version 120

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(4194304.0);  // 1 << 22 = 4194304

uniform ivec2 uScreenSize;

// Flag bit values (must match drawrect_120.frag)
const float FLAG_NO_TEXTURE = 4.0;   // 1 << 2
const float FLAG_MASK = 8.0;         // 1 << 3
const float FLAG_CROSS_HATCH = 16.0; // 1 << 4
const float FLAG_TTF_TEXT = 32.0;    // 1 << 5

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

varying vec4 fPalettes;  // .xyz=palette rows, .w=hintThresh
varying vec4 fMisc1;     // .x=paletteCount, .y=fColour, .z=fZoom, .w=fScreenHeight
varying vec4 fMisc2;     // .xy=unused, .zw=fPosition

void main()
{
    // Clamp position by vClip, correcting interpolated values for the clipping
    vec2 m = clamp(
        ((vVertMat * vClip) - (vVertMat * vBounds)) / (vBounds.zw - vBounds.xy) + vVertVec, 0.0, 1.0);
    vec2 pos = mix(vBounds.xy, vBounds.zw, m);

    fMisc2 = vec4(0.0, 0.0, vBounds.xy);

    // Pre-compute texture coordinates per-vertex so the fragment shader
    // receives them via gl_TexCoord[] (T-registers on i915).  This avoids
    // per-fragment ALU -> TEX dependency chains that blow the indirect-
    // texture-lookup budget on Intel GMA 945.
    float fZoomVal = vZoom;
    vec2 fPosition = vBounds.xy;
    vec2 fragAtVertex = pos;  // screen-space vertex position
    vec2 vertPosition = (fragAtVertex - fPosition) * fZoomVal;

    // Colour atlas UV
    float colourU = (vTexColourCoords.x + vertPosition.x) / vTexColourCoords.z;
    float colourV = (vTexColourCoords.y + vertPosition.y) / vTexColourCoords.w;
    gl_TexCoord[0] = vec4(colourU, colourV, 0.0, 0.0);

    // Mask atlas UV
    float maskU = (vTexMaskCoords.x + vertPosition.x) / vTexMaskCoords.z;
    float maskV = (vTexMaskCoords.y + vertPosition.y) / vTexMaskCoords.w;
    gl_TexCoord[1] = vec4(maskU, maskV, 0.0, 0.0);

    // Transform screen coordinates to texture coordinates
    float depth = 1.0 - (vDepth + 1.0) * DEPTH_INCREMENT;
    pos = pos / vec2(uScreenSize);
    pos.y = pos.y * -1.0 + 1.0;

    // Peel position (screen UV + depth)
    gl_TexCoord[2] = vec4(pos, depth * 0.5 + 0.5, 0.0);

    float paletteCount = mod(vFlags, 4.0);
    float hintThresh = mod(floor(vFlags / 256.0), 256.0);
    fPalettes = vec4(vPalettes, hintThresh);
    fMisc1 = vec4(paletteCount, vColour, vZoom, float(uScreenSize.y));

    // Transform texture coordinates to viewport coordinates
    pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, depth, 1.0);
}
