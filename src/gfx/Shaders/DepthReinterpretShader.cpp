#include "DepthReinterpretShader.h"

#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>

#include <Corrade/Utility/Resource.h>

using namespace Magnum;

static void importShaderResources() {
    CORRADE_RESOURCE_INITIALIZE(SHADER_RESOURCES)
}

DepthReinterpretShader::DepthReinterpretShader() {
    Magnum::GL::Version shaderVersion;
    #ifdef TARGET_EMSCRIPTEN
    shaderVersion = GL::Version::GLES300;
    #else
    shaderVersion = GL::Version::GL330;
    #endif
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(shaderVersion);

    if (!Utility::Resource::hasGroup("Shader-data"))
        importShaderResources();

    GL::Shader vert{shaderVersion, GL::Shader::Type::Vertex};
    GL::Shader frag{shaderVersion, GL::Shader::Type::Fragment};

    Utility::Resource rs{"Shader-data"};
    vert.addSource(rs.getString("DepthReinterpretShader.vert"));
    frag.addSource(rs.getString("DepthReinterpretShader.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile() && frag.compile());

    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(uniformLocation("depthTexture"), TextureUnit);
}