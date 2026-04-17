#version 120

const float MASK_REMAP_COUNT = 3.0;
const float FLAG_NO_TEXTURE = 4.0;  // 1 << 2
const float FLAG_MASK = 8.0;       // 1 << 3
const float FLAG_CROSS_HATCH = 16.0; // 1 << 4
const float FLAG_TTF_TEXT = 32.0;  // 1 << 5

uniform sampler3D uTexture;
uniform int uAtlasLayerCount;
uniform sampler2D uPaletteTex;

uniform sampler2D uPeelingTex;
uniform bool uPeeling;

varying float fFlags;
varying float fColour;
varying vec4 fTexColour;
varying vec4 fTexMask;
varying vec3 fPalettes;

varying vec2 fPosition;
varying vec3 fPeelPos;
varying float fZoom;
varying float fTexColourAtlas;
varying float fTexMaskAtlas;
varying float fScreenHeight;

void main()
{
    if (uPeeling)
    {
        float peel = texture2D(uPeelingTex, fPeelPos.xy).r;
        if (peel == 0.0 || fPeelPos.z >= peel)
        {
            discard;
        }
    }

    vec2 fragCoord = vec2(floor(gl_FragCoord.x), fScreenHeight - floor(gl_FragCoord.y) - 1.0);
    vec2 position = (fragCoord - fPosition) * fZoom;

    int texel;
    int iFlags = int(fFlags);
    int noTextureFlag = iFlags / int(FLAG_NO_TEXTURE);
    if (noTextureFlag - (noTextureFlag / 2) * 2 == 0)
    {
        float colourU = (fTexColour.x + position.x) / fTexColour.z;
        float colourV = (fTexColour.y + position.y) / fTexColour.w;
        texel = int(texture3D(uTexture, vec3(colourU, colourV, (fTexColourAtlas + 0.5) / float(uAtlasLayerCount))).r * 255.0);
        if (texel == 0)
        {
            discard;
        }
        int ttfTextFlag = iFlags / int(FLAG_TTF_TEXT);
        if (ttfTextFlag - (ttfTextFlag / 2) * 2 == 0)
        {
            texel += int(fColour);
        }
        else
        {
            int hint_thresh = (iFlags / 256) - ((iFlags / 256) / 256) * 256;
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
        }
    }
    else
    {
        texel = int(fColour);
    }

    int paletteCount = iFlags - (iFlags / 4) * 4;
    if (paletteCount >= 3 && texel >= 46 && texel < 58)  // 0x2E = 46, 0x3A = 58
    {
        texel = int(texture2D(uPaletteTex, vec2(float(texel + 197), fPalettes.z) / 256.0).r * 255.0);  // 0xC5 = 197
    }
    else if (paletteCount >= 2 && texel >= 202 && texel < 214)  // 0xCA = 202, 0xD6 = 214
    {
        texel = int(texture2D(uPaletteTex, vec2(float(texel + 41), fPalettes.y) / 256.0).r * 255.0);  // 0x29 = 41
    }
    else if (paletteCount >= 1)
    {
        texel = int(texture2D(uPaletteTex, vec2(float(texel), fPalettes.x) / 256.0).r * 255.0);
    }

    if (texel == 0)
    {
        discard;
    }

    int crossHatchFlag = iFlags / int(FLAG_CROSS_HATCH);
    if (crossHatchFlag - (crossHatchFlag / 2) * 2 != 0)
    {
        int posSum = int(position.x) + int(position.y);
        int posSumMod2 = posSum - (posSum / 2) * 2;
        if (posSumMod2 != 0)
        {
            discard;
        }
    }

    int maskFlag = iFlags / int(FLAG_MASK);
    if (maskFlag - (maskFlag / 2) * 2 != 0)
    {
        float maskU = (fTexMask.x + position.x) / fTexMask.z;
        float maskV = (fTexMask.y + position.y) / fTexMask.w;
        int mask = int(texture3D(uTexture, vec3(maskU, maskV, (fTexMaskAtlas + 0.5) / float(uAtlasLayerCount))).r * 255.0);
        if (mask == 0)
        {
            discard;
        }
    }

    gl_FragColor = vec4(float(texel) / 255.0, 0.0, 0.0, 1.0);
}
