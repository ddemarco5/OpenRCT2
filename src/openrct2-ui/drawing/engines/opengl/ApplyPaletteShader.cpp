/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_OPENGL

    #include "ApplyPaletteShader.h"
    #include "OpenGLAPI.h"

using namespace OpenRCT2::Ui;

namespace
{
    struct VDStruct
    {
        GLfloat position[2];
        GLfloat texturecoordinate[2];
    };
} // namespace

constexpr VDStruct kVertexData[4] = {
    { -1.0f, -1.0f, 0.0f, 0.0f },
    { 1.0f, -1.0f, 1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
};

ApplyPaletteShader::ApplyPaletteShader()
    : OpenGLShaderProgram([]() {
        // Force GLSL 120 for testing integer texture replacement with regular textures
        constexpr bool forceGLSL120 = true;
        
        if (forceGLSL120)
        {
            LOG_WARNING("FORCE: Using GLSL 120 shader: applypalette_120");
            return "applypalette_120";
        }
        
        GLint majorVersion = 0;
        GLint minorVersion = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
        glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
        
        const char* shaderName = "applypalette";
        
        // If glGetIntegerv fails (returns 0), assume OpenGL 2.1
        if (majorVersion == 0 && minorVersion == 0)
        {
            shaderName = "applypalette_120";
            LOG_WARNING("OpenGL version query failed, assuming OpenGL 2.1, using shader: %s", shaderName);
        }
        // Use GLSL 120 for OpenGL 2.1
        else if (majorVersion == 2 && minorVersion == 1)
        {
            shaderName = "applypalette_120";
            LOG_WARNING("OpenGL 2.1 detected, using GLSL 120 shader: %s", shaderName);
        }
        else
        {
            LOG_WARNING("OpenGL %d.%d detected, using GLSL 330 shader: %s", majorVersion, minorVersion, shaderName);
        }
        
        return shaderName;
    }())
{
    GetLocations();

    glCall(glGenBuffers, 1, &_vbo);
    glCall(glGenVertexArrays, 1, &_vao);

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vbo);
    glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(kVertexData), kVertexData, GL_STATIC_DRAW);

    glCall(glBindVertexArray, _vao);
    glCall(
        glVertexAttribPointer, vPosition, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, position)));
    glCall(
        glVertexAttribPointer, vTextureCoordinate, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, texturecoordinate)));

    glCall(glEnableVertexAttribArray, vPosition);
    glCall(glEnableVertexAttribArray, vTextureCoordinate);

    Use();
    glCall(glUniform1i, uTexture, 0);
    glCall(glUniform1i, uPaletteTex, 1);

    // Create palette texture
    glCall(glGenTextures, 1, &_paletteTex);
    glCall(glBindTexture, GL_TEXTURE_2D, _paletteTex);
    glCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

ApplyPaletteShader::~ApplyPaletteShader()
{
    glCall(glDeleteBuffers, 1, &_vbo);
    glCall(glDeleteVertexArrays, 1, &_vao);
    if (_paletteTex != 0)
        glCall(glDeleteTextures, 1, &_paletteTex);
}

void ApplyPaletteShader::GetLocations()
{
    uTexture = GetUniformLocation("uTexture");
    uPaletteTex = GetUniformLocation("uPaletteTex");

    vPosition = GetAttributeLocation("vPosition");
    vTextureCoordinate = GetAttributeLocation("vTextureCoordinate");
}

void ApplyPaletteShader::SetTexture(GLuint texture)
{
    OpenGLAPI::SetTexture(0, GL_TEXTURE_2D, texture);
}

void ApplyPaletteShader::SetPalette(const vec4* glPalette)
{
    // Upload palette to texture instead of uniform array (for OpenGL 2.1 compatibility)
    glCall(glBindTexture, GL_TEXTURE_2D, _paletteTex);
    glCall(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_FLOAT, glPalette);
}

void ApplyPaletteShader::Draw()
{
    OpenGLAPI::SetTexture(1, GL_TEXTURE_2D, _paletteTex);
    glCall(glBindVertexArray, _vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

#endif /* DISABLE_OPENGL */
