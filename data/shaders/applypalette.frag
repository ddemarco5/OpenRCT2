#version 330 core

uniform vec4 uPalette[256];
uniform sampler2D uTexture;

in vec2 fTextureCoordinate;

out vec4 oColour;

void main()
{
    oColour = uPalette[int(texture(uTexture, fTextureCoordinate).r * 255.0)];
}
