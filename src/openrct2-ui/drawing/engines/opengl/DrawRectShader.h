/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "DrawCommands.h"
#include "GLSLTypes.h"
#include "OpenGLShaderProgram.h"

#include <SDL_pixels.h>
#include <array>
#include <set>

namespace OpenRCT2::Ui
{
    // DrawRectShader manages a family of specialized GLSL 120 programs compiled
    // from the same source with different preprocessor defines. On ancient
    // OpenGL 2.1 hardware (Intel GMA class) the general shader blows through
    // ARB_fp's indirection / ALU budgets, so we partition each frame's rect
    // commands into feature buckets and issue one draw per bucket using the
    // matching, minimally-featured program.
    //
    // Variants are identified by a 6-bit key (see VariantBit). Programs are
    // compiled LAZILY on first use so impossible / never-used combinations
    // don't pay a startup cost.
    class DrawRectShader final
    {
    public:
        // Must match the indices used with glBindAttribLocation so one VAO
        // works for every program variant.
        enum AttribLoc : GLint
        {
            kAttrVertMat = 0, // occupies 4 consecutive slots (0..3)
            kAttrVertVec = 4,
            kAttrClip = 5,
            kAttrTexColourAtlas = 6,
            kAttrTexColourCoords = 7,
            kAttrTexMaskAtlas = 8,
            kAttrTexMaskCoords = 9,
            kAttrPalettes = 10,
            kAttrFlags = 11,
            kAttrColour = 12,
            kAttrBounds = 13,
            kAttrDepth = 14,
            kAttrZoom = 15,
        };

        // Feature bits composed into the variant key.
        enum VariantBit : int
        {
            kVarPeel = 1 << 0,       // HAS_PEEL       (managed by EnablePeeling/DisablePeeling)
            kVarMask = 1 << 1,       // HAS_MASK       (per-instance FLAG_MASK)
            kVarTtf = 1 << 2,        // HAS_TTF        (per-instance FLAG_TTF_TEXT)
            kVarCrossHatch = 1 << 3, // HAS_CROSSHATCH (per-instance FLAG_CROSS_HATCH)
            kVarPalette = 1 << 4,    // HAS_PALETTE    (per-instance paletteCount >= 1)
            kVarNoTexture = 1 << 5,  // NO_TEXTURE     (per-instance FLAG_NO_TEXTURE)
        };
        static constexpr int kVariantCount = 64;
        static constexpr int kInstanceBitMask
            = kVarMask | kVarTtf | kVarCrossHatch | kVarPalette | kVarNoTexture; // everything except peel

        // Map a DrawRectCommand::flags value to its instance-side bucket key.
        static int VariantKeyFromFlags(int32_t flags);

    private:
        struct ProgramVariant
        {
            std::unique_ptr<OpenGLShaderProgram> program;
            GLint uScreenSize{ -1 };
            GLint uTexColour{ -1 };
            GLint uTexMask{ -1 };
            GLint uPaletteTex{ -1 };
            GLint uPeelingTex{ -1 };
        };

        // Lazily populated; index is the 6-bit variant key.
        std::array<std::unique_ptr<ProgramVariant>, kVariantCount> _variants;
        std::set<int> _failedKeys; // keys that failed to compile (avoid retrying)
        int _activeKey{ 0 };
        bool _peelActive{ false };

        // Persistent state re-applied to each newly-compiled variant.
        int32_t _screenWidth{ 0 };
        int32_t _screenHeight{ 0 };

        GLuint _vbo{ 0 };
        GLuint _vboInstances{ 0 };
        GLuint _vao{ 0 };

        GLsizei _instanceCount = 0;
        size_t _maxInstancesBufferSize;

    public:
        DrawRectShader();
        ~DrawRectShader();

        void Use();
        void SetScreenSize(int32_t width, int32_t height);
        void EnablePeeling(GLuint peelingTex);
        void DisablePeeling();

        // Select the variant matching this per-instance feature key (no peel bit);
        // the peel bit is OR'd in automatically based on EnablePeeling state.
        void SelectVariant(int instanceKey);

        void SetInstances(const DrawRectCommand* data, size_t count);
        void SetInstances(const RectCommandBatch& instances);
        void DrawInstances();

    private:
        ProgramVariant& EnsureVariant(int key);
        ProgramVariant* ActiveOrNull()
        {
            return _variants[_activeKey].get();
        }
    };
} // namespace OpenRCT2::Ui
