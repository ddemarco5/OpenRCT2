#version 120

varying float fColour;

void main()
{
    gl_FragColor = vec4(fColour / 255.0, 0.0, 0.0, 1.0);
}
