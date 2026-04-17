#version 120

uniform sampler2D uTexture;
uniform sampler2D uPaletteTex;

varying vec2 fTextureCoordinate;

void main()
{
    int index = int(texture2D(uTexture, fTextureCoordinate).r * 255.0);
    gl_FragColor = texture2D(uPaletteTex, vec2(float(index) / 255.0, 0.0));
}
