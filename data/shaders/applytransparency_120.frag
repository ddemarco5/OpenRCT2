#version 120

uniform sampler2D uOpaqueTex;
uniform sampler2D uOpaqueDepth;
uniform sampler2D uTransparentTex;
uniform sampler2D uTransparentDepth;
uniform sampler2D uPaletteTex;
uniform sampler2D uBlendPaletteTex;

varying vec2 fTextureCoordinate;

void main()
{
    int opaque = int(texture2D(uOpaqueTex, fTextureCoordinate).r * 255.0);
    float opaqueDepth = texture2D(uOpaqueDepth, fTextureCoordinate).r;
    int transparent = int(texture2D(uTransparentTex, fTextureCoordinate).r * 255.0);
    float transparentDepth = texture2D(uTransparentDepth, fTextureCoordinate).r;

    if (opaqueDepth <= transparentDepth)
    {
        transparent = 0;
    }

    int blendColour = (transparent / 256);
    if (blendColour > 0)
    {
        int lowByte = transparent - (blendColour * 256);
        if (lowByte != 0)
        {
            gl_FragColor = vec4(float(blendColour) / 255.0, 0.0, 0.0, 1.0);
        }
        else
        {
            int result = int(texture2D(uBlendPaletteTex, vec2(float(opaque), float(blendColour)) / 256.0).r * 255.0);
            gl_FragColor = vec4(float(result) / 255.0, 0.0, 0.0, 1.0);
        }
    }
    else
    {
        int result = int(texture2D(uPaletteTex, vec2(float(opaque), float(transparent)) / 256.0).r * 255.0);
        gl_FragColor = vec4(float(result) / 255.0, 0.0, 0.0, 1.0);
    }
}
