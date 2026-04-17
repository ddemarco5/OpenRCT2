#version 120

varying float fColour;

void main()
{
    // Pack float back to int for color
    // Note: OpenGL 2.1 doesn't support integer framebuffers
    // This is a workaround - the actual color packing needs to match the C++ side
    gl_FragColor = vec4(fColour / 255.0, 0.0, 0.0, 1.0);
}
