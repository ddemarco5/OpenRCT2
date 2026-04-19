/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "DrawingEngineFactory.hpp"
#include "opengl/OpenGLAPI.h"

#include <SDL.h>
#include <openrct2/Diagnostic.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Guard.hpp>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/X8DrawingEngine.h>
#include <openrct2/profiling/FrameProfiler.hpp>
#include <openrct2/ui/UiContext.h>
#include <stdexcept>

using namespace OpenRCT2;
using namespace OpenRCT2::Drawing;
using namespace OpenRCT2::Ui;

namespace
{
    // Minimal vertex shader: passes a fullscreen quad with texcoords.
    // GLSL 1.20 for OpenGL 2.1 / i915 hardware.
    constexpr const char* kVertexShaderSrc = R"(#version 120
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

    // Minimal fragment shader: indexed palette lookup.
    // One indirect texture lookup (palette depends on index texture result).
    // Well within i915's 4-indirect-lookup budget.
    constexpr const char* kFragmentShaderSrc = R"(#version 120
uniform sampler2D uIndexTex;
uniform sampler2D uPaletteTex;
varying vec2 vTexCoord;
void main()
{
    float index = texture2D(uIndexTex, vTexCoord).a;
    gl_FragColor = texture2D(uPaletteTex, vec2(index, 0.5));
}
)";

    // Fullscreen quad (triangle strip): 4 vertices, each with vec2 pos + vec2 uv.
    // Flipped Y so the CPU buffer (top-down) appears right-side up.
    // clang-format off
    constexpr float kQuadVertices[] = {
        //   pos          uv
        -1.0f, -1.0f,   0.0f, 1.0f,  // bottom-left
         1.0f, -1.0f,   1.0f, 1.0f,  // bottom-right
        -1.0f,  1.0f,   0.0f, 0.0f,  // top-left
         1.0f,  1.0f,   1.0f, 0.0f,  // top-right
    };
    // clang-format on

    GLuint CompileShader(GLenum type, const char* src)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            char log[1024] = {};
            glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
            LOG_ERROR("Hybrid shader compile error: %s", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint LinkProgram(GLuint vs, GLuint fs)
    {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glBindAttribLocation(prog, 0, "aPosition");
        glBindAttribLocation(prog, 1, "aTexCoord");
        glLinkProgram(prog);

        GLint status = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &status);
        if (status != GL_TRUE)
        {
            char log[1024] = {};
            glGetProgramInfoLog(prog, sizeof(log) - 1, nullptr, log);
            LOG_ERROR("Hybrid shader link error: %s", log);
            glDeleteProgram(prog);
            return 0;
        }
        return prog;
    }
} // namespace

/**
 * HybridDisplayDrawingEngine - renders game via software (X8DrawingEngine) but
 * uses a minimal OpenGL pipeline to perform the final palette->RGB conversion
 * and present. This offloads the per-pixel palette lookup from the CPU to the
 * GPU, which on low-end integrated graphics (e.g. Intel 945GME) is still
 * substantially faster than doing it on an Atom N270 CPU with in-order
 * execution and cache-hostile write patterns.
 */
class HybridDisplayDrawingEngine final : public X8DrawingEngine
{
private:
    IUiContext& _uiContext;
    SDL_Window* _window = nullptr;
    SDL_GLContext _glContext = nullptr;

    GLuint _program = 0;
    GLuint _indexTexture = 0;   // R8/GL_ALPHA, width x height palette indices
    GLuint _paletteTexture = 0; // RGBA8, 256x1
    GLuint _quadVbo = 0;

    GLint _uIndexTex = -1;
    GLint _uPaletteTex = -1;

    uint8_t _paletteRGBA[256 * 4] = {};
    bool _paletteDirty = true;

    uint32_t _glTexWidth = 0;
    uint32_t _glTexHeight = 0;

    bool _useVsync = true;

public:
    explicit HybridDisplayDrawingEngine(IUiContext& uiContext)
        : X8DrawingEngine(uiContext)
        , _uiContext(uiContext)
    {
        _window = static_cast<SDL_Window*>(_uiContext.GetWindow());
    }

    ~HybridDisplayDrawingEngine() override
    {
        if (_glContext != nullptr)
        {
            SDL_GL_MakeCurrent(_window, _glContext);
            if (_program != 0)
                glDeleteProgram(_program);
            if (_indexTexture != 0)
                glDeleteTextures(1, &_indexTexture);
            if (_paletteTexture != 0)
                glDeleteTextures(1, &_paletteTexture);
            if (_quadVbo != 0)
                glDeleteBuffers(1, &_quadVbo);
            SDL_GL_DeleteContext(_glContext);
        }
    }

    void Initialise() override
    {
        // Request OpenGL 2.1 compatibility profile (matches target i915 hardware).
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

        _glContext = SDL_GL_CreateContext(_window);
        if (_glContext == nullptr)
        {
            throw std::runtime_error(std::string("Failed to create OpenGL 2.1 context: ") + SDL_GetError());
        }
        SDL_GL_MakeCurrent(_window, _glContext);
        SDL_GL_SetSwapInterval(_useVsync ? 1 : 0);

        if (!OpenGLAPI::Initialise())
        {
            throw std::runtime_error("Failed to initialise OpenGL API for hybrid engine.");
        }

        // Compile & link the minimal palette-lookup program
        GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
        if (vs == 0 || fs == 0)
        {
            throw std::runtime_error("Hybrid engine: shader compilation failed.");
        }
        _program = LinkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (_program == 0)
        {
            throw std::runtime_error("Hybrid engine: shader linking failed.");
        }

        _uIndexTex = glGetUniformLocation(_program, "uIndexTex");
        _uPaletteTex = glGetUniformLocation(_program, "uPaletteTex");

        // Quad VBO
        glGenBuffers(1, &_quadVbo);
        glBindBuffer(GL_ARRAY_BUFFER, _quadVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Palette texture (256x1 RGBA8)
        glGenTextures(1, &_paletteTexture);
        glBindTexture(GL_TEXTURE_2D, _paletteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Index texture - allocated on first Resize. Use GL_ALPHA so the .a swizzle
        // in the shader gives us the raw palette index byte.
        glGenTextures(1, &_indexTexture);
        glBindTexture(GL_TEXTURE_2D, _indexTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void SetVSync(bool vsync) override
    {
        _useVsync = vsync;
        if (_glContext != nullptr)
        {
            SDL_GL_SetSwapInterval(vsync ? 1 : 0);
        }
    }

    void Resize(uint32_t width, uint32_t height) override
    {
        if (width == 0 || height == 0)
            return;

        X8DrawingEngine::Resize(width, height);

        if (_glContext != nullptr)
        {
            SDL_GL_MakeCurrent(_window, _glContext);
            glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

            // Reallocate index texture to match the new size. Use GL_ALPHA for 8bpp.
            glBindTexture(GL_TEXTURE_2D, _indexTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_ALPHA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_ALPHA,
                GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            _glTexWidth = width;
            _glTexHeight = height;
        }
    }

    void SetPalette(const GamePalette& palette) override
    {
        // Build RGBA palette for the GPU. Order matches framebuffer swizzle of
        // GL_RGBA / GL_UNSIGNED_BYTE which is R,G,B,A bytes in memory.
        for (int i = 0; i < 256; i++)
        {
            _paletteRGBA[i * 4 + 0] = palette[i].red;
            _paletteRGBA[i * 4 + 1] = palette[i].green;
            _paletteRGBA[i * 4 + 2] = palette[i].blue;
            _paletteRGBA[i * 4 + 3] = 255;
        }
        _paletteDirty = true;
    }

    void BeginDraw() override
    {
        X8DrawingEngine::BeginDraw();
    }

    void EndDraw() override
    {
        X8DrawingEngine::EndDraw();

        if (_glContext == nullptr || _bits == nullptr)
            return;

        SDL_GL_MakeCurrent(_window, _glContext);

        // Upload palette if it changed (usually once per palette shift)
        if (_paletteDirty)
        {
            auto timer = OpenRCT2::Profiling::FramePhaseTimer(OpenRCT2::Profiling::FramePhase::CopyBitsToTexture);
            glBindTexture(GL_TEXTURE_2D, _paletteTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, _paletteRGBA);
            _paletteDirty = false;
        }

        // Upload the 8-bit software framebuffer. This is the ONLY per-pixel data
        // transfer - 1 byte/pixel instead of 4, and no CPU palette lookup.
        {
            auto timer = OpenRCT2::Profiling::FramePhaseTimer(OpenRCT2::Profiling::FramePhase::CopyBitsToTexture);
            glBindTexture(GL_TEXTURE_2D, _indexTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(_pitch));
            glTexSubImage2D(
                GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(_glTexWidth), static_cast<GLsizei>(_glTexHeight), GL_ALPHA,
                GL_UNSIGNED_BYTE, _bits);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }

        // Draw fullscreen quad with palette lookup
        {
            auto timer = OpenRCT2::Profiling::FramePhaseTimer(OpenRCT2::Profiling::FramePhase::SDLRenderPresent);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glUseProgram(_program);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _indexTexture);
            glUniform1i(_uIndexTex, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _paletteTexture);
            glUniform1i(_uPaletteTex, 1);

            glBindBuffer(GL_ARRAY_BUFFER, _quadVbo);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
            glVertexAttribPointer(
                1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);

            SDL_GL_SwapWindow(_window);
        }
    }
};

std::unique_ptr<IDrawingEngine> Ui::CreateHybridDisplayDrawingEngine(IUiContext& uiContext)
{
    return std::make_unique<HybridDisplayDrawingEngine>(uiContext);
}
