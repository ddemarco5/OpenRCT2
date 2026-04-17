#version 120

uniform vec4 uPalette[256];
uniform sampler2D uTexture;

varying vec2 fTextureCoordinate;

void main()
{
    gl_FragColor = uPalette[int(texture2D(uTexture, fTextureCoordinate).r * 255.0)];
}
