#version 120

attribute vec2 vPosition;
attribute vec2 vTextureCoordinate;

varying vec2 fTextureCoordinate;

uniform vec4 uSourceRect;
uniform vec2 uTextureSize;

void main()
{
    gl_Position = vec4(vPosition, 0.0, 1.0);
    vec2 srcPos = uSourceRect.xy;
    vec2 srcSize = uSourceRect.zw;
    fTextureCoordinate = (srcPos + vTextureCoordinate * srcSize) / uTextureSize;
}
