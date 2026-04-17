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

#ifndef NO_TEXTURE
uniform sampler2D uTexColour;
#endif

#ifdef HAS_MASK
uniform sampler2D uTexMask;
#endif

#ifdef HAS_PALETTE
uniform sampler2D uPaletteTex;
#endif

#ifdef HAS_PEEL
uniform sampler2D uPeelingTex;
#endif

varying vec4 fPalettes;  // .xyz=palette rows, .w=hintThresh
varying vec4 fMisc1;     // .x=paletteCount, .y=fColour, .z=fZoom, .w=fScreenHeight
varying vec4 fMisc2;     // .xy=unused, .zw=fPosition
// gl_TexCoord[0].xy = colour UV   (pre-computed in vertex shader)
// gl_TexCoord[1].xy = mask UV     (pre-computed in vertex shader)
// gl_TexCoord[2].xyz = peel pos   (pre-computed in vertex shader)

void main()
{
    float fColour = fMisc1.y;
    float fZoom = fMisc1.z;
    float fScreenHeight = fMisc1.w;
    vec2 fPosition = fMisc2.zw;

    // --- All texture coordinates pre-computed in vertex shader via gl_TexCoord[] ---
    // These map to T-registers on i915, giving "direct" texture lookups (0 indirect cost).
#ifdef HAS_PEEL
    float peelSample = texture2D(uPeelingTex, gl_TexCoord[2].xy).r;
#endif

#ifndef NO_TEXTURE
    int colourTexel = int(texture2D(uTexColour, gl_TexCoord[0].xy).r * 255.0);
#endif

#ifdef HAS_MASK
    int maskTexel = int(texture2D(uTexMask, gl_TexCoord[1].xy).r * 255.0);
#endif

#ifdef HAS_PEEL
    if (peelSample == 0.0 || gl_TexCoord[2].z >= peelSample)
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
    // Reconstruct screen-space position for crosshatch pattern
    vec2 fragCoord = vec2(floor(gl_FragCoord.x), fScreenHeight - floor(gl_FragCoord.y) - 1.0);
    vec2 position = (fragCoord - fPosition) * fZoom;
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
