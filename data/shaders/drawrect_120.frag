#version 120

// drawrect_120: fragment shader specialized by preprocessor defines.
// The CPU bucketizes each frame's draw commands and picks the variant that
// matches; each variant only contains the code it needs, so the ARB_fp
// indirection / ALU budgets on ancient hardware (Intel GMA) are easier to fit.
//
// Supported defines (injected by OpenGLShaderProgram):
//   HAS_PEEL       - include depth-peel test (active during transparency iters > 0)
//   HAS_MASK       - include the mask sampler and mask discard test
//   HAS_TTF        - include the TTF glyph colouring path
//   HAS_CROSSHATCH - include the crosshatch discard
//   HAS_PALETTE    - include the palette remap (needed when paletteCount >= 1)
//   NO_TEXTURE    - skip the main colour texture fetch (fill/filter rects)

#if !defined(NO_TEXTURE) || defined(HAS_MASK)
uniform sampler3D uTexture;
#endif

#ifdef HAS_PALETTE
uniform sampler2D uPaletteTex;
#endif

#ifdef HAS_PEEL
uniform sampler2D uPeelingTex;
#endif

varying vec4 fTexColour;
varying vec4 fTexMask;
varying vec4 fPalettes;  // .xyz=palette rows, .w=hintThresh
varying vec3 fPeelPos;
varying vec4 fMisc1;     // .x=paletteCount, .y=fColour, .z=fZoom, .w=fScreenHeight
varying vec4 fMisc2;     // .xy=pre-divided atlas layer coords, .zw=fPosition
varying vec4 fFlagBits;  // (noTexture, ttfText, crossHatch, mask) as 0.0/1.0

void main()
{
    float fColour = fMisc1.y;
    float fZoom = fMisc1.z;
    float fScreenHeight = fMisc1.w;
    vec2 fPosition = fMisc2.zw;

    vec2 fragCoord = vec2(floor(gl_FragCoord.x), fScreenHeight - floor(gl_FragCoord.y) - 1.0);
    vec2 position = (fragCoord - fPosition) * fZoom;

    // --- PHASE 1: all texture fetches whose coords depend only on varyings ---
#ifdef HAS_PEEL
    float peelSample = texture2D(uPeelingTex, fPeelPos.xy).r;
#endif

#ifndef NO_TEXTURE
    float fTexColourLayer = fMisc2.x;
    float colourU = (fTexColour.x + position.x) / fTexColour.z;
    float colourV = (fTexColour.y + position.y) / fTexColour.w;
    int colourTexel = int(texture3D(uTexture, vec3(colourU, colourV, fTexColourLayer)).r * 255.0);
#endif

#ifdef HAS_MASK
    float fTexMaskLayer = fMisc2.y;
    float maskU = (fTexMask.x + position.x) / fTexMask.z;
    float maskV = (fTexMask.y + position.y) / fTexMask.w;
    int maskTexel = int(texture3D(uTexture, vec3(maskU, maskV, fTexMaskLayer)).r * 255.0);
#endif

#ifdef HAS_PEEL
    if (peelSample == 0.0 || fPeelPos.z >= peelSample)
    {
        discard;
    }
#endif

    // --- Compute pre-palette texel value ---
    int texel;
#ifdef NO_TEXTURE
    // Fill/filter rect: the index IS fColour, no texture sample needed.
    texel = int(fColour);
#else
    texel = colourTexel;
    if (texel == 0)
    {
        discard;
    }
  #ifdef HAS_TTF
    // TTF glyph colouring: threshold by hinting level, encode solid-vs-AA in LSB.
    int hint_thresh = int(fPalettes.w);
    if (hint_thresh > 0)
    {
        bool solidColor = texel > 180;
        texel = (texel > hint_thresh) ? int(fColour) : 0;
        texel = texel * 256;
        if (solidColor)
        {
            texel += 1;
        }
    }
    else
    {
        texel = int(fColour);
    }
  #else
    // Normal sprite: sampled index offset by the remap colour.
    texel += int(fColour);
  #endif
#endif

#ifdef HAS_PALETTE
    // --- PHASE 2: single unconditional palette fetch ---
    // sel3/sel2/sel1 pick which palette row (if any) this texel falls into.
    // Mutually exclusive by construction; sel3 has priority, then sel2, then sel1.
    float ftexel = float(texel);
    float pc = fMisc1.x;
    float sel3 = step(3.0, pc) * step(46.0, ftexel) * step(ftexel, 57.0);
    float sel2 = step(2.0, pc) * step(202.0, ftexel) * step(ftexel, 213.0) * (1.0 - sel3);
    float sel1 = step(1.0, pc) * (1.0 - sel3) * (1.0 - sel2);
    float row = sel3 * fPalettes.z + sel2 * fPalettes.y + sel1 * fPalettes.x;
    float offset = sel3 * 197.0 + sel2 * 41.0;
    float useRemap = sel3 + sel2 + sel1;
    float remapped = texture2D(uPaletteTex, vec2(ftexel + offset, row) / 256.0).r * 255.0;
    texel = int(mix(ftexel, remapped, useRemap));
#endif

    if (texel == 0)
    {
        discard;
    }

#ifdef HAS_CROSSHATCH
    if (mod(floor(position.x) + floor(position.y), 2.0) >= 0.5)
    {
        discard;
    }
#endif

#ifdef HAS_MASK
    if (maskTexel == 0)
    {
        discard;
    }
#endif

    gl_FragColor = vec4(float(texel) / 255.0, 0.0, 0.0, 1.0);
}
