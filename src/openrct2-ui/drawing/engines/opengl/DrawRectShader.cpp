/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_OPENGL

    #include "DrawRectShader.h"

using namespace OpenRCT2::Ui;

namespace
{
    struct VDStruct
    {
        GLfloat mat[4][2];
        GLfloat vec[2];
    };
} // namespace

constexpr VDStruct kVertexData[4] = {
    { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
};

constexpr size_t kInitialInstancesBufferSize = 32768;

DrawRectShader::DrawRectShader()
    : OpenGLShaderProgram([]() -> const char* {
        constexpr bool forceGLSL120 = true;

        if (forceGLSL120)
        {
            LOG_WARNING("FORCE: Using GLSL 120 shader: drawrect_120");
            return "drawrect_120";
        }

        GLint majorVersion = 0;
        GLint minorVersion = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
        glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

        const char* shaderName = "drawrect";

        if (majorVersion == 0 && minorVersion == 0)
        {
            shaderName = "drawrect_120";
            LOG_WARNING("OpenGL version query failed, assuming OpenGL 2.1, using shader: %s", shaderName);
        }
        else if (majorVersion == 2 && minorVersion == 1)
        {
            shaderName = "drawrect_120";
            LOG_WARNING("OpenGL 2.1 detected, using GLSL 120 shader: %s", shaderName);
        }
        else
        {
            LOG_WARNING("OpenGL %d.%d detected, using GLSL 330 shader: %s", majorVersion, minorVersion, shaderName);
        }

        return shaderName;
    }())
    , _maxInstancesBufferSize(kInitialInstancesBufferSize)
{
    GetLocations();

    constexpr bool useGL120 = true; // Must match the forceGLSL120 flag in the lambda above

    glCall(glGenBuffers, 1, &_vbo);
    glCall(glGenBuffers, 1, &_vboInstances);
    glCall(glGenVertexArrays, 1, &_vao);

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vbo);
    glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(kVertexData), kVertexData, GL_STATIC_DRAW);

    glCall(glBindVertexArray, _vao);

    glCall(
        glVertexAttribPointer, vVertMat + 0, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[0])));
    glCall(
        glVertexAttribPointer, vVertMat + 1, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[1])));
    glCall(
        glVertexAttribPointer, vVertMat + 2, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[2])));
    glCall(
        glVertexAttribPointer, vVertMat + 3, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[3])));
    glCall(
        glVertexAttribPointer, vVertVec, 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, vec)));

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vboInstances);
    glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(DrawRectCommand) * kInitialInstancesBufferSize, nullptr, GL_STREAM_DRAW);

    if (useGL120)
    {
        // drawrect_120.vert declares integer instance attributes as float;
        // glVertexAttribPointer converts GL_INT/GL_UNSIGNED_INT data to float.
        glCall(
            glVertexAttribPointer, vClip, 4, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, clip)));
        glCall(
            glVertexAttribPointer, vTexColourAtlas, 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourAtlas)));
        glCall(
            glVertexAttribPointer, vTexColourCoords, 4, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourBounds)));
        glCall(
            glVertexAttribPointer, vTexMaskAtlas, 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskAtlas)));
        glCall(
            glVertexAttribPointer, vTexMaskCoords, 4, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskBounds)));
        glCall(
            glVertexAttribPointer, vPalettes, 3, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, palettes)));
        glCall(
            glVertexAttribPointer, vFlags, 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, flags)));
        glCall(
            glVertexAttribPointer, vColour, 1, GL_UNSIGNED_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, colour)));
        glCall(
            glVertexAttribPointer, vBounds, 4, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, bounds)));
        glCall(
            glVertexAttribPointer, vDepth, 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, depth)));
        glCall(
            glVertexAttribPointer, vZoom, 1, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, zoom)));
    }
    else
    {
        // drawrect.vert (GLSL 330) uses integer attribute types; requires glVertexAttribIPointer.
        glCall(
            glVertexAttribIPointer, vClip, 4, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, clip)));
        glCall(
            glVertexAttribIPointer, vTexColourAtlas, 1, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourAtlas)));
        glCall(
            glVertexAttribPointer, vTexColourCoords, 4, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourBounds)));
        glCall(
            glVertexAttribIPointer, vTexMaskAtlas, 1, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskAtlas)));
        glCall(
            glVertexAttribPointer, vTexMaskCoords, 4, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskBounds)));
        glCall(
            glVertexAttribIPointer, vPalettes, 3, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, palettes)));
        glCall(
            glVertexAttribIPointer, vFlags, 1, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, flags)));
        glCall(
            glVertexAttribIPointer, vColour, 1, GL_UNSIGNED_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, colour)));
        glCall(
            glVertexAttribIPointer, vBounds, 4, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, bounds)));
        glCall(
            glVertexAttribIPointer, vDepth, 1, GL_INT, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, depth)));
        glCall(
            glVertexAttribPointer, vZoom, 1, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
            reinterpret_cast<void*>(offsetof(DrawRectCommand, zoom)));
    }

    glCall(glEnableVertexAttribArray, vVertMat + 0);
    glCall(glEnableVertexAttribArray, vVertMat + 1);
    glCall(glEnableVertexAttribArray, vVertMat + 2);
    glCall(glEnableVertexAttribArray, vVertMat + 3);
    glCall(glEnableVertexAttribArray, vVertVec);

    glCall(glEnableVertexAttribArray, vClip);
    glCall(glEnableVertexAttribArray, vTexColourAtlas);
    glCall(glEnableVertexAttribArray, vTexColourCoords);
    glCall(glEnableVertexAttribArray, vTexMaskAtlas);
    glCall(glEnableVertexAttribArray, vTexMaskCoords);
    glCall(glEnableVertexAttribArray, vPalettes);
    glCall(glEnableVertexAttribArray, vFlags);
    glCall(glEnableVertexAttribArray, vColour);
    glCall(glEnableVertexAttribArray, vBounds);
    glCall(glEnableVertexAttribArray, vDepth);
    glCall(glEnableVertexAttribArray, vZoom);

    glCall(glVertexAttribDivisor, vClip, 1);
    glCall(glVertexAttribDivisor, vTexColourAtlas, 1);
    glCall(glVertexAttribDivisor, vTexColourCoords, 1);
    glCall(glVertexAttribDivisor, vTexMaskAtlas, 1);
    glCall(glVertexAttribDivisor, vTexMaskCoords, 1);
    glCall(glVertexAttribDivisor, vPalettes, 1);
    glCall(glVertexAttribDivisor, vFlags, 1);
    glCall(glVertexAttribDivisor, vColour, 1);
    glCall(glVertexAttribDivisor, vBounds, 1);
    glCall(glVertexAttribDivisor, vDepth, 1);
    glCall(glVertexAttribDivisor, vZoom, 1);

    Use();
    glCall(glUniform1i, uTexture, 0);
    glCall(glUniform1i, uPaletteTex, 1);

    glCall(glUniform1i, uPeelingTex, 2);
    glCall(glUniform1i, uPeeling, 0);
}

DrawRectShader::~DrawRectShader()
{
    glCall(glDeleteBuffers, 1, &_vbo);
    glCall(glDeleteBuffers, 1, &_vboInstances);
    glCall(glDeleteVertexArrays, 1, &_vao);
}

void DrawRectShader::GetLocations()
{
    uScreenSize = GetUniformLocation("uScreenSize");
    uTexture = GetUniformLocation("uTexture");
    uPaletteTex = GetUniformLocation("uPaletteTex");

    uPeelingTex = GetUniformLocation("uPeelingTex");
    uPeeling = GetUniformLocation("uPeeling");

    uAtlasLayerCount = GetUniformLocation("uAtlasLayerCount");

    vClip = GetAttributeLocation("vClip");
    vTexColourAtlas = GetAttributeLocation("vTexColourAtlas");
    vTexColourCoords = GetAttributeLocation("vTexColourCoords");
    vTexMaskAtlas = GetAttributeLocation("vTexMaskAtlas");
    vTexMaskCoords = GetAttributeLocation("vTexMaskCoords");
    vPalettes = GetAttributeLocation("vPalettes");
    vFlags = GetAttributeLocation("vFlags");
    vColour = GetAttributeLocation("vColour");
    vBounds = GetAttributeLocation("vBounds");
    vDepth = GetAttributeLocation("vDepth");
    vZoom = GetAttributeLocation("vZoom");

    vVertMat = GetAttributeLocation("vVertMat");
    vVertVec = GetAttributeLocation("vVertVec");
}

void DrawRectShader::SetAtlasLayerCount(GLuint count)
{
    glCall(glUniform1i, uAtlasLayerCount, static_cast<GLint>(count));
}

void DrawRectShader::SetScreenSize(int32_t width, int32_t height)
{
    glCall(glUniform2i, uScreenSize, width, height);
}

void DrawRectShader::EnablePeeling(GLuint peelingTex)
{
    OpenGLAPI::SetTexture(2, GL_TEXTURE_2D, peelingTex);
    glCall(glUniform1i, uPeeling, 1);
}

void DrawRectShader::DisablePeeling()
{
    glCall(glUniform1i, uPeeling, 0);
}

void DrawRectShader::SetInstances(const RectCommandBatch& instances)
{
    glCall(glBindVertexArray, _vao);

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vboInstances);

    if (instances.size() > _maxInstancesBufferSize)
    {
        glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(DrawRectCommand) * instances.size(), instances.data(), GL_STREAM_DRAW);
        _maxInstancesBufferSize = instances.size();
    }
    else
    {
        glCall(glBufferSubData, GL_ARRAY_BUFFER, 0, sizeof(DrawRectCommand) * instances.size(), instances.data());
    }

    _instanceCount = static_cast<GLsizei>(instances.size());
}

void DrawRectShader::DrawInstances()
{
    glCall(glBindVertexArray, _vao);
    glCall(glDrawArraysInstanced, GL_TRIANGLE_STRIP, 0, 4, _instanceCount);
}

#endif /* DISABLE_OPENGL */
