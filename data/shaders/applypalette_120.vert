#version 120

attribute vec4 vPosition;
attribute vec2 vTextureCoordinate;

varying vec2 fTextureCoordinate;

void main()
{
    fTextureCoordinate = vTextureCoordinate;
    gl_Position = vPosition;
}
