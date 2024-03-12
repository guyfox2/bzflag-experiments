#include "MagnumSceneRenderer.h"

#include "DrawableGroupManager.h"
#include "Magnum/GL/AbstractFramebuffer.h"
#include "Magnum/GL/Framebuffer.h"
#include "Magnum/GL/GL.h"
#include "Magnum/GL/Sampler.h"
#include "Magnum/Math/Vector3.h"
#include "Magnum/SceneGraph/Camera.h"

#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>

#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/Trade/MeshData.h>

#include <Magnum/SceneGraph/Camera.h>

#include <Magnum/ImGuiIntegration/Widgets.h>

#include <string>

#include "DrawModeManager.h"
#include "MagnumTextureManager.h"
#include "SceneObjectManager.h"

#include "BZDBCache.h"

using namespace Magnum;

MagnumSceneRenderer::MagnumSceneRenderer() {}

void MagnumSceneRenderer::init() {
    // Add depth map texture
    {
        GL::Texture2D *tex = new GL::Texture2D{};
        (*tex)
            .setWrapping(GL::SamplerWrapping::ClampToBorder)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear)
            .setBorderColor(Math::Color4{1.0f, 1.0f, 1.0f, 1.0f})
            .setStorage(1, GL::TextureFormat::DepthComponent16, _depthMapSize);
            
        addPipelineTex("DepthMapTex", {tex, (unsigned)_depthMapSize[0], (unsigned)_depthMapSize[1], false});
    }
    // Add depth map preview texture
    // This is just the 16-bit depth map rendered to a regular rgba texture
    // for previewing
    /*{
        GL::Texture2D *tex = new GL::Texture2D{};
        (*tex)
            .setWrapping(GL::SamplerWrapping::ClampToBorder)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear)
            .setStorage(1, GL::TextureFormat::RGBA8, _depthMapSize);
        addPipelineTex("DepthMapPreviewTex", {tex, (unsigned)_depthMapSize[0], (unsigned)_depthMapSize[1], false});
    }*/

    // Create a generic quad mesh for previewing
    const Vector3 vertices[]{
        {{ 1.0f, -1.0f, 0.0f}}, /* Bottom right */
        {{ 1.0f,  1.0f, 0.0f}}, /* Top right */
        {{-1.0f, -1.0f, 0.0f}}, /* Bottom left */
        {{-1.0f,  1.0f, 0.0f}}  /* Top left */
    };
    const Vector2 texcoords[]{
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 0.0f},
        {0.0f, 1.0f}
    };
    const UnsignedInt indices[]{        /* 3--1 1 */
        0, 1, 2,            /* | / /| */
        2, 1, 3             /* |/ / | */
    };

    struct VertexData {
        Vector3 position;
        Vector2 texcoord;
    };

    Containers::ArrayView<const Vector3> posview{vertices};
    Containers::ArrayView<const Vector2> texview{texcoords};

    // Pack mesh data
    Containers::Array<char> data{posview.size()*sizeof(VertexData)};
    data = MeshTools::interleave(posview, texview);
    Containers::StridedArrayView1D<const VertexData> dataview = Containers::arrayCast<const VertexData>(data);

    Trade::MeshData mdata{MeshPrimitive::Triangles, Trade::DataFlags{}, indices, Trade::MeshIndexData{indices}, std::move(data), {
        Trade::MeshAttributeData{Trade::MeshAttribute::Position, dataview.slice(&VertexData::position)},
        Trade::MeshAttributeData{Trade::MeshAttribute::TextureCoordinates, dataview.slice(&VertexData::texcoord)},
    }, static_cast<UnsignedInt>(dataview.size())};

    _quadMesh = MeshTools::compile(mdata);

    Object3D* lightobj = SOMGR.getObj("Sun");
    lightobj->resetTransformation();
    lightobj->transform(Matrix4::lookAt({5000.0f, 1000.0f, 8000.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));

    _lightCamera = new SceneGraph::Camera3D{*lightobj};
    _bzmatMode.setLightObj(lightobj);
    _bzmatMode.setLightCamera(_lightCamera);
}

void MagnumSceneRenderer::setSunPosition(Math::Vector3<float> position) {
    Object3D* lightobj = SOMGR.getObj("Sun");
    lightobj->resetTransformation();
    lightobj->transform(Matrix4::lookAt(position, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));
}

Math::Vector3<float> MagnumSceneRenderer::getSunPosition() const {
    Object3D* lightobj = SOMGR.getObj("Sun");
    return lightobj->transformation().translation();
}

float MagnumSceneRenderer::getSunNearPlane() const {
    Object3D* lightobj = SOMGR.getObj("Sun");
    float worldHeight = 1000.0f;    // Just assume this for now
    float worldSize = BZDBCache::worldSize;
    Math::Vector3<float> coords[4] = {
        {-worldSize/2.0f, -worldSize/2.0f, worldHeight},
        {-worldSize/2.0f, worldSize/2.0f, worldHeight},
        {worldSize/2.0f, -worldSize/2.0f, worldHeight},
        {worldSize/2.0f, worldSize/2.0f, worldHeight}
    };

    float minDist = 1e9;

    for (int i = 0; i < 4; ++i) {
        float dist = (coords[i]-lightobj->transformation().translation()).length();
        if (dist < minDist) {
            minDist = dist;
        }
    }

    return minDist;
}

float MagnumSceneRenderer::getSunFarPlane() const {
    Object3D* lightobj = SOMGR.getObj("Sun");
    float worldHeight = 1000.0f;    // Just assume this for now
    float worldSize = BZDBCache::worldSize;
    Math::Vector3<float> coords[4] = {
        {-worldSize/2.0f, -worldSize/2.0f, 0.0f},
        {-worldSize/2.0f, worldSize/2.0f, 0.0f},
        {worldSize/2.0f, -worldSize/2.0f, 0.0f},
        {worldSize/2.0f, worldSize/2.0f, 0.0f}
    };

    float maxDist = 0.0f;

    for (int i = 0; i < 4; ++i) {
        float dist = (coords[i]-lightobj->transformation().translation()).length();
        if (dist > maxDist) {
            maxDist = dist;
        }
    }

    return maxDist;
}

TextureData MagnumSceneRenderer::getPipelineTex(const std::string& name) {
    auto it = _pipelineTexMap.find(name);
    if (it != _pipelineTexMap.end()) {
        return it->second;
    }
    return {NULL, 0, 0, false};
}

void MagnumSceneRenderer::addPipelineTex(const std::string& name, TextureData data) {
    _pipelineTexMap.insert(std::make_pair(name, data));
}

// Render scene from POV of camera using current drawmode and framebuffer
void MagnumSceneRenderer::renderScene(SceneGraph::Camera3D* camera) {
    _bzmatMode.bindShadowMap(*getPipelineTex("DepthMapTex").texture);
    DRAWMODEMGR.setDrawMode(&_bzmatMode);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::Blending);
    if (auto* dg = DGRPMGR.getGroup("WorldDrawables"))
        camera->draw(*dg);
    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    if (auto* dg = DGRPMGR.getGroup("TankDrawables"))
        camera->draw(*dg);
    if (auto* dg = DGRPMGR.getGroup("WorldTransDrawables"))
        camera->draw(*dg);
   
}

void MagnumSceneRenderer::renderLightDepthMap() {
    TextureData depthTexData = getPipelineTex("DepthMapTex");
    // Much of this should only be done when the sun moves.
    float worldDiag = 1.414f*BZDBCache::worldSize;
    // Set up camera
    (*_lightCamera)
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::orthographicProjection({-worldDiag, worldDiag}, getSunNearPlane(), getSunFarPlane()))
        .setViewport({(int)depthTexData.width, (int)depthTexData.height});
    
    // Set up renderbuffer / framebuffer
    GL::Framebuffer depthFB{{{}, {(int)depthTexData.width, (int)depthTexData.height}}};
    depthFB.attachTexture(GL::Framebuffer::BufferAttachment::Depth, *depthTexData.texture, 0);

    DRAWMODEMGR.setDrawMode(&_depthMode);

    depthFB.clear(GL::FramebufferClear::Depth).bind();

    GL::Renderer::setFaceCullingMode(GL::Renderer::PolygonFacing::Front);
    // Now render to texture
    if (auto* dg = DGRPMGR.getGroup("WorldDrawables"))
        _lightCamera->draw(*dg);
    if (auto* dg = DGRPMGR.getGroup("TankDrawables"))
        _lightCamera->draw(*dg);
    if (auto* dg = DGRPMGR.getGroup("WorldTransDrawables"))
        _lightCamera->draw(*dg);
    GL::Renderer::setFaceCullingMode(GL::Renderer::PolygonFacing::Back);

    GL::defaultFramebuffer.bind();
}

// Render the 16-bit depth buffer to a regular rgba texture for presentation
// Not really necessary, but a good demo on how to do something like this.
/*void MagnumSceneRenderer::renderLightDepthMapPreview() {
    TextureData depthTexData = getPipelineTex("DepthMapTex");
    TextureData depthPreviewTexData = getPipelineTex("DepthMapPreviewTex");

    GL::Renderbuffer depth;
    depth.setStorage(GL::RenderbufferFormat::DepthComponent16, {(int)depthPreviewTexData.width, (int)depthPreviewTexData.height});
    GL::Framebuffer framebuffer{ {{}, {(int)depthPreviewTexData.width, (int)depthPreviewTexData.height}} };
    framebuffer.attachTexture(GL::Framebuffer::ColorAttachment{ 0 }, *depthPreviewTexData.texture, 0);

    framebuffer.attachRenderbuffer(
    GL::Framebuffer::BufferAttachment::Depth, depth);

    framebuffer.clear(GL::FramebufferClear::Color|GL::FramebufferClear::Depth).bind();

    _depthVisShader
        .bindTexture(*depthTexData.texture)
        .draw(_quadMesh);

    GL::defaultFramebuffer.bind();
}*/

void MagnumSceneRenderer::drawPipelineTexBrowser(const char *title, bool *p_open) {
    std::string names_cc;
    std::vector<std::string> names;
    for (const auto& e: _pipelineTexMap) {
        names_cc += e.first + std::string("\0", 1);
        names.push_back(e.first);
    }
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin(title, p_open, ImGuiWindowFlags_NoScrollbar);
    static int itemCurrent = 0;
    if (names_cc.size() >= 0) {
        ImGui::Combo("Pipeline Texture Name", &itemCurrent, names_cc.c_str(), names_cc.size());
    } else {
        ImGui::Text("No Pipeline Textures");
    }
    if (names_cc.size() >= 0 && itemCurrent < names.size()) {
        TextureData tex = _pipelineTexMap[names[itemCurrent]];
        ImVec2 ws = ImGui::GetContentRegionAvail();
        float width = fmin((float)ws.x, (float)tex.width);
        float height = fmin((float)tex.height, width/(float)tex.width*(float)tex.height);
        ImGuiIntegration::image(*tex.texture, {width, height});
    }
    ImGui::End();
}