#version 330 core

// clang-format off
uniform sampler2D       uOpaqueTex;
uniform sampler2D       uOpaqueDepth;
uniform sampler2D      uTransparentTex;
uniform sampler2D       uTransparentDepth;
uniform sampler2D      uPaletteTex;
uniform sampler2D      uBlendPaletteTex;
// clang-format on

in vec2 fTextureCoordinate;

out uint oColour;

void main()
{
    uint opaque = uint(texture(uOpaqueTex, fTextureCoordinate).r * 255.0);
    float opaqueDepth = texture(uOpaqueDepth, fTextureCoordinate).r;
    uint transparent = uint(texture(uTransparentTex, fTextureCoordinate).r * 255.0);
    float transparentDepth = texture(uTransparentDepth, fTextureCoordinate).r;

    if (opaqueDepth <= transparentDepth)
    {
        transparent = 0u;
    }

    uint blendColour = (transparent & 0xff00u) >> 8;
    if (blendColour > 0u)
    {
        if ((transparent & 0x00ffu) != 0u)
        {
            oColour = blendColour;
        }
        else
        {
            oColour = uint(texture(uBlendPaletteTex, vec2(opaque, blendColour) / 256.f).r * 255.0);
        }
    }
    else
    {
        oColour = uint(texture(uPaletteTex, vec2(opaque, transparent) / 256.f).r * 255.0);
    }
}
