#version 120

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(4194304.0);  // 1 << 22 = 4194304

uniform ivec2 uScreenSize;
uniform int uAtlasLayerCount;

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

varying vec4 fTexColour;
varying vec4 fTexMask;
varying vec4 fPalettes;  // .xyz=palette rows, .w=hintThresh
varying vec3 fPeelPos;
varying vec4 fMisc1;     // .x=paletteCount, .y=fColour, .z=fZoom, .w=fScreenHeight
varying vec4 fMisc2;     // .xy=pre-divided atlas layer coords, .zw=fPosition
varying vec4 fFlagBits;  // (noTexture, ttfText, crossHatch, mask) as 0.0/1.0

void main()
{
    // Clamp position by vClip, correcting interpolated values for the clipping
    vec2 m = clamp(
        ((vVertMat * vClip) - (vVertMat * vBounds)) / (vBounds.zw - vBounds.xy) + vVertVec, 0.0, 1.0);
    vec2 pos = mix(vBounds.xy, vBounds.zw, m);
    fTexColour = vTexColourCoords;
    fTexMask = vTexMaskCoords;

    // Pre-divide atlas layer indices so the fragment shader doesn't have to
    float invLayers = 1.0 / float(uAtlasLayerCount);
    float texColourLayer = (vTexColourAtlas + 0.5) * invLayers;
    float texMaskLayer = (vTexMaskAtlas + 0.5) * invLayers;
    fMisc2 = vec4(texColourLayer, texMaskLayer, vBounds.xy);

    // Transform screen coordinates to texture coordinates
    float depth = 1.0 - (vDepth + 1.0) * DEPTH_INCREMENT;
    pos = pos / vec2(uScreenSize);
    pos.y = pos.y * -1.0 + 1.0;
    fPeelPos = vec3(pos, depth * 0.5 + 0.5);

    // Pre-decode flag bits once per vertex; the fragment shader just compares these.
    float noTextureBit = mod(floor(vFlags / FLAG_NO_TEXTURE), 2.0);
    float ttfTextBit = mod(floor(vFlags / FLAG_TTF_TEXT), 2.0);
    float crossHatchBit = mod(floor(vFlags / FLAG_CROSS_HATCH), 2.0);
    float maskBit = mod(floor(vFlags / FLAG_MASK), 2.0);
    fFlagBits = vec4(noTextureBit, ttfTextBit, crossHatchBit, maskBit);

    float paletteCount = mod(vFlags, 4.0);
    float hintThresh = mod(floor(vFlags / 256.0), 256.0);
    fPalettes = vec4(vPalettes, hintThresh);
    fMisc1 = vec4(paletteCount, vColour, vZoom, float(uScreenSize.y));

    // Transform texture coordinates to viewport coordinates
    pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, depth, 1.0);
}
