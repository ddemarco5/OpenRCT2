#version 120

varying vec2 fTextureCoordinate;

uniform sampler2D uTexture;

void main()
{
    gl_FragColor = texture2D(uTexture, fTextureCoordinate);
}
