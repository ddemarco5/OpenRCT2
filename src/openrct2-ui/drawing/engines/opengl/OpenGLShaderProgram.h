/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "OpenGLAPI.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
namespace OpenRCT2::Ui
{
    struct ShaderAttribBinding
    {
        GLuint index;
        const char* name;
    };

    class OpenGLShader final
    {
    private:
        static constexpr uint64_t kMaxSourceSize = 8 * 1024 * 1024; // 8 MiB

        GLenum _type;
        GLuint _id = 0;

    public:
        OpenGLShader(const char* name, GLenum type, std::string_view defines = {});
        ~OpenGLShader();

        GLuint GetShaderId();

    private:
        std::string GetPath(const std::string& name);
        static std::string ReadSourceCode(const std::string& path);
        static std::string InjectDefines(std::string source, std::string_view defines);
    };

    class OpenGLShaderProgram
    {
    private:
        GLuint _id = 0;
        std::unique_ptr<OpenGLShader> _vertexShader;
        std::unique_ptr<OpenGLShader> _fragmentShader;

    public:
        explicit OpenGLShaderProgram(const char* name, std::string_view defines = {});
        OpenGLShaderProgram(
            const char* name, std::string_view defines,
            std::initializer_list<ShaderAttribBinding> attribBindings);
        explicit OpenGLShaderProgram(const OpenGLShaderProgram&) = delete;
        explicit OpenGLShaderProgram(OpenGLShaderProgram&&) = default;
        virtual ~OpenGLShaderProgram();

        GLint GetAttributeLocation(const char* name);
        GLint GetUniformLocation(const char* name);
        void Use();
        GLuint GetProgramId() const
        {
            return _id;
        }

    private:
        bool Link();
    };
} // namespace OpenRCT2::Ui
