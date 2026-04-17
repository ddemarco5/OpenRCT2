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

    #include <openrct2/core/Console.hpp>

using namespace OpenRCT2::Ui;

namespace
{
    struct VDStruct
    {
        GLfloat mat[4][2];
        GLfloat vec[2];
    };

    constexpr VDStruct kVertexData[4] = {
        { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
    };

    constexpr size_t kInitialInstancesBufferSize = 32768;

    // Shader used by every variant; only the preprocessor defines differ.
    constexpr const char* kShaderName = "drawrect_120";

    // Keep this list in sync with DrawRectShader::AttribLoc.
    const std::initializer_list<ShaderAttribBinding> kAttribBindings = {
        { DrawRectShader::kAttrVertMat + 0, "vVertMat" },
        { DrawRectShader::kAttrVertVec, "vVertVec" },
        { DrawRectShader::kAttrClip, "vClip" },
        { DrawRectShader::kAttrTexColourAtlas, "vTexColourAtlas" },
        { DrawRectShader::kAttrTexColourCoords, "vTexColourCoords" },
        { DrawRectShader::kAttrTexMaskAtlas, "vTexMaskAtlas" },
        { DrawRectShader::kAttrTexMaskCoords, "vTexMaskCoords" },
        { DrawRectShader::kAttrPalettes, "vPalettes" },
        { DrawRectShader::kAttrFlags, "vFlags" },
        { DrawRectShader::kAttrColour, "vColour" },
        { DrawRectShader::kAttrBounds, "vBounds" },
        { DrawRectShader::kAttrDepth, "vDepth" },
        { DrawRectShader::kAttrZoom, "vZoom" },
    };
} // namespace

int DrawRectShader::VariantKeyFromFlags(int32_t flags)
{
    int key = 0;
    if (flags & DrawRectCommand::FLAG_MASK)
        key |= kVarMask;
    if (flags & DrawRectCommand::FLAG_TTF_TEXT)
        key |= kVarTtf;
    if (flags & DrawRectCommand::FLAG_CROSS_HATCH)
        key |= kVarCrossHatch;
    if ((flags & 0x3) >= 1) // paletteCount in the low 2 bits
        key |= kVarPalette;
    if (flags & DrawRectCommand::FLAG_NO_TEXTURE)
        key |= kVarNoTexture;
    return key;
}

DrawRectShader::DrawRectShader()
    : _maxInstancesBufferSize(kInitialInstancesBufferSize)
{
    LOG_WARNING("FORCE: Using GLSL 120 shader: drawrect_120 (lazy specialized variants)");

    // Eagerly build the "empty" variant (no features, no peel) so that Use()
    // and VAO setup below have something bound. All other variants are built
    // lazily on first SelectVariant() for their key.
    EnsureVariant(0);

    glCall(glGenBuffers, 1, &_vbo);
    glCall(glGenBuffers, 1, &_vboInstances);
    glCall(glGenVertexArrays, 1, &_vao);

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vbo);
    glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(kVertexData), kVertexData, GL_STATIC_DRAW);

    glCall(glBindVertexArray, _vao);

    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrVertMat + 0), 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[0])));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrVertMat + 1), 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[1])));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrVertMat + 2), 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[2])));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrVertMat + 3), 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, mat[3])));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrVertVec), 2, GL_FLOAT, GL_FALSE, glSizeOf<VDStruct>(),
        reinterpret_cast<void*>(offsetof(VDStruct, vec)));

    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vboInstances);
    glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(DrawRectCommand) * kInitialInstancesBufferSize, nullptr, GL_STREAM_DRAW);

    // drawrect_120.vert declares integer instance attributes as float;
    // glVertexAttribPointer converts GL_INT/GL_UNSIGNED_INT data to float.
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrClip), 4, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, clip)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrTexColourAtlas), 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourAtlas)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrTexColourCoords), 4, GL_FLOAT, GL_FALSE,
        glSizeOf<DrawRectCommand>(), reinterpret_cast<void*>(offsetof(DrawRectCommand, texColourBounds)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrTexMaskAtlas), 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskAtlas)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrTexMaskCoords), 4, GL_FLOAT, GL_FALSE,
        glSizeOf<DrawRectCommand>(), reinterpret_cast<void*>(offsetof(DrawRectCommand, texMaskBounds)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrPalettes), 3, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, palettes)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrFlags), 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, flags)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrColour), 1, GL_UNSIGNED_INT, GL_FALSE,
        glSizeOf<DrawRectCommand>(), reinterpret_cast<void*>(offsetof(DrawRectCommand, colour)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrBounds), 4, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, bounds)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrDepth), 1, GL_INT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, depth)));
    glCall(
        glVertexAttribPointer, static_cast<GLuint>(kAttrZoom), 1, GL_FLOAT, GL_FALSE, glSizeOf<DrawRectCommand>(),
        reinterpret_cast<void*>(offsetof(DrawRectCommand, zoom)));

    for (GLint i = 0; i <= kAttrZoom; ++i)
    {
        glCall(glEnableVertexAttribArray, static_cast<GLuint>(i));
    }

    // Per-instance divisors (all per-instance attributes except vVertMat/vVertVec which are per-vertex)
    for (GLint i = kAttrClip; i <= kAttrZoom; ++i)
    {
        glCall(glVertexAttribDivisor, static_cast<GLuint>(i), 1);
    }

    Use();
}

DrawRectShader::~DrawRectShader()
{
    glCall(glDeleteBuffers, 1, &_vbo);
    glCall(glDeleteBuffers, 1, &_vboInstances);
    glCall(glDeleteVertexArrays, 1, &_vao);
}

DrawRectShader::ProgramVariant& DrawRectShader::EnsureVariant(int key)
{
    if (_variants[key] != nullptr)
        return *_variants[key];

    // Compose the set of #defines for this key.
    std::string defines;
    defines.reserve(128);
    if (key & kVarPeel)
        defines += "#define HAS_PEEL\n";
    if (key & kVarMask)
        defines += "#define HAS_MASK\n";
    if (key & kVarTtf)
        defines += "#define HAS_TTF\n";
    if (key & kVarCrossHatch)
        defines += "#define HAS_CROSSHATCH\n";
    if (key & kVarPalette)
        defines += "#define HAS_PALETTE\n";
    if (key & kVarNoTexture)
        defines += "#define NO_TEXTURE\n";

    auto variant = std::make_unique<ProgramVariant>();
    variant->program = std::make_unique<OpenGLShaderProgram>(kShaderName, defines, kAttribBindings);

    // Use raw glGetUniformLocation to avoid the error-log noise that
    // OpenGLShaderProgram::GetUniformLocation emits for missing uniforms;
    // some uniforms only exist in certain variants.
    const GLuint pid = variant->program->GetProgramId();
    variant->uScreenSize = glCall(glGetUniformLocation, pid, "uScreenSize");
    variant->uTexture = glCall(glGetUniformLocation, pid, "uTexture");
    variant->uPaletteTex = glCall(glGetUniformLocation, pid, "uPaletteTex");
    variant->uPeelingTex = glCall(glGetUniformLocation, pid, "uPeelingTex");
    variant->uAtlasLayerCount = glCall(glGetUniformLocation, pid, "uAtlasLayerCount");

    // One-shot uniform initialization: sampler unit bindings and cached screen size.
    variant->program->Use();
    if (variant->uTexture != -1)
        glCall(glUniform1i, variant->uTexture, 0);
    if (variant->uPaletteTex != -1)
        glCall(glUniform1i, variant->uPaletteTex, 1);
    if (variant->uPeelingTex != -1)
        glCall(glUniform1i, variant->uPeelingTex, 2);
    if (variant->uScreenSize != -1 && (_screenWidth | _screenHeight) != 0)
        glCall(glUniform2i, variant->uScreenSize, _screenWidth, _screenHeight);

    _variants[key] = std::move(variant);
    return *_variants[key];
}

void DrawRectShader::Use()
{
    if (auto* v = ActiveOrNull())
        v->program->Use();
}

void DrawRectShader::SetAtlasLayerCount(GLuint count)
{
    if (auto* v = ActiveOrNull(); v != nullptr && v->uAtlasLayerCount != -1)
        glCall(glUniform1i, v->uAtlasLayerCount, static_cast<GLint>(count));
}

void DrawRectShader::SetScreenSize(int32_t width, int32_t height)
{
    _screenWidth = width;
    _screenHeight = height;
    // Propagate to every already-built variant so we don't need to track
    // dirty flags per variant.
    for (auto& v : _variants)
    {
        if (v == nullptr || v->uScreenSize == -1)
            continue;
        v->program->Use();
        glCall(glUniform2i, v->uScreenSize, width, height);
    }
    // Re-bind the currently active program.
    if (auto* v = ActiveOrNull())
        v->program->Use();
}

void DrawRectShader::EnablePeeling(GLuint peelingTex)
{
    OpenGLAPI::SetTexture(2, GL_TEXTURE_2D, peelingTex);
    _peelActive = true;
    // Keep the currently-selected instance key, but recompute effective variant.
    SelectVariant(_activeKey & kInstanceBitMask);
}

void DrawRectShader::DisablePeeling()
{
    _peelActive = false;
    SelectVariant(_activeKey & kInstanceBitMask);
}

void DrawRectShader::SelectVariant(int instanceKey)
{
    const int key = (instanceKey & kInstanceBitMask) | (_peelActive ? kVarPeel : 0);
    _activeKey = key;
    EnsureVariant(key).program->Use();
}

void DrawRectShader::SetInstances(const DrawRectCommand* data, size_t count)
{
    glCall(glBindVertexArray, _vao);
    glCall(glBindBuffer, GL_ARRAY_BUFFER, _vboInstances);

    if (count > _maxInstancesBufferSize)
    {
        glCall(glBufferData, GL_ARRAY_BUFFER, sizeof(DrawRectCommand) * count, data, GL_STREAM_DRAW);
        _maxInstancesBufferSize = count;
    }
    else
    {
        glCall(glBufferSubData, GL_ARRAY_BUFFER, 0, sizeof(DrawRectCommand) * count, data);
    }

    _instanceCount = static_cast<GLsizei>(count);
}

void DrawRectShader::SetInstances(const RectCommandBatch& instances)
{
    SetInstances(instances.data(), instances.size());
}

void DrawRectShader::DrawInstances()
{
    glCall(glBindVertexArray, _vao);
    glCall(glDrawArraysInstanced, GL_TRIANGLE_STRIP, 0, 4, _instanceCount);
}

#endif /* DISABLE_OPENGL */
