#include <Magnum/GL/Context.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Resource.h>

#include <Magnum/GL/Version.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Types.h>

#include <imgui.h>

#include "DrawableGroupBrowser.h"
#include "GLInfo.h"

#include "MagnumBZMaterial.h"
#include "MagnumDefs.h"
#include "MagnumSceneManager.h"
#include "MagnumTextureManager.h"
#include "WorldSceneObjectGenerator.h"

#include "BZTextureBrowser.h"
#include "BZMaterialBrowser.h"
#include "BZMaterialViewer.h"
#include "ObstacleBrowser.h"
#include "BZWTextEditor.h"
#include "AboutDialog.h"
#include "CacheBrowser.h"
#include "TexMatBrowser.h"
#include "DynColorBrowser.h"
#include "MeshTransformBrowser.h"
#include "PhyDrvBrowser.h"
#include "OnlineMapBrowser.h"

#include "WorldMeshGenerator.h"
#include "cURLManager.h"

#include "SceneObjectManager.h"
#include "DrawableGroupManager.h"

#include "MagnumSceneManager.h"
#include "MagnumSceneRenderer.h"

#include <ctime>
#include <cassert>
#include <cstring>
#include <fstream>

#ifdef TARGET_EMSCRIPTEN
#include "emscripten_browser_file.h"
#else
#include "imfilebrowser.h"
#endif

#include "common.h"

#include "BZDBCache.h"
#include "BZDBLocal.h"
#include "ErrorHandler.h"
#include "GameTime.h"
#include "PhysicsDriver.h"
#include "ObstacleMgr.h"
#include "TextureMatrix.h"
#include "DynamicColor.h"
#include "Teleporter.h"

#include "Downloads.h"

#include "BZWReader.h"

// defaults for bzdb
#include "defaultBZDB.h"

// Some leftover global state that bzflag code needs
int         debugLevel = 4;

class BasicLoggingCallback : public LoggingCallback {
    public:
        void log(int lvl, const char* msg) override;
};

void BasicLoggingCallback::log(int lvl, const char* msg)
{
    if (lvl <= debugLevel)
        std::cout << lvl << " " << msg << std::endl;
}

BasicLoggingCallback blc;

using namespace Magnum;
using namespace Magnum::Math::Literals;

class MapViewer: public Platform::Sdl2Application {
    public:
        explicit MapViewer(const Arguments& arguments);
        void init();


    private:
        void resetBZDB();

        void tickEvent() override;

        void drawEvent() override;

        void viewportEvent(ViewportEvent& e) override;
        void mousePressEvent(MouseEvent& e) override;
        void mouseReleaseEvent(MouseEvent& e) override;
        void mouseMoveEvent(MouseMoveEvent& e) override;
        void mouseScrollEvent(MouseScrollEvent& e) override;
        void textInputEvent(TextInputEvent& event) override;
        void exitEvent(ExitEvent& event) override;

        void keyPressEvent(KeyEvent& event) override;
        void keyReleaseEvent(KeyEvent& event) override;

        void tryConnect(const std::string& callsign, const std::string& password, const std::string& server, const std::string& port);

        // IMGUI
        void showMainMenuBar();
        void showMenuView();
        void showMenuDebug();

        void drawWindows();

        // Window should draw
        bool showProfiler = false;
        bool showTMBrowser = false;
        bool showMATBrowser = false;
        bool showMATViewer = false;
        bool showObsBrowser = false;
        bool showGLInfo = false;
        bool showEditor = false;
        bool showAbout = false;
        bool showCacheBrowser = false;
        bool showTexMatBrowser = false;
        bool showPhyDrvBrowser = false;
        bool showDynColorBrowser = false;
        bool showMeshTransformBrowser = false;
        bool showMapBrowser = true;
        bool showDrawableGroupBrowser = false;
        bool showRendererSettings = false;
        bool showAdjustSun = false;
        bool showPipelineTexBrowser = false;

        bool showGrid = false;

#ifndef TARGET_EMSCRIPTEN
        void maybeShowFileBrowser();
#endif
        
        Vector3 positionOnSphere(const Vector2i& position) const;

        static void startupErrorCallback(const char* msg);

#ifdef TARGET_EMSCRIPTEN
        static void handleUploadedMap(const std::string& filename, const std::string& mime_type, std::string_view buffer, void*);
#endif
        static void handleSaveFromEditor(const std::string& filename, const std::string& data);
        static void handleUpdateFromEditor(const std::string& filename, const std::string& data);

        static void handleLoadFromMapBrowser(const CachedResource& mapRsc);

        void loadMap(std::string path, const std::string& data, bool reloadEditor = true);

        void loopIteration();

        Object3D *_sceneRoot;
        Object3D *_manipulator, *_cameraObject;
        SceneGraph::Camera3D* _camera;
        SceneGraph::DrawableGroup3D _drawables;
        Vector3 _previousPosition;

        WorldSceneObjectGenerator worldSceneObjGen;
        WorldMeshGenerator worldSceneBuilder;
        ImGuiIntegration::Context _imgui{NoCreate};

        MagnumSceneRenderer sceneRenderer;

        BZTextureBrowser tmBrowser;
        BZMaterialBrowser matBrowser;
        BZMaterialViewer matViewer;
        ObstacleBrowser obsBrowser;
        BZWTextEditor editor;
        AboutDialog about;
        CacheBrowser cacheBrowser;
        TexMatBrowser texMatBrowser;
        DynColorBrowser dynColorBrowser;
        MeshTransformBrowser meshTFBrowser;
        PhyDrvBrowser phyDrvBrowser;
        OnlineMapBrowser mapBrowser;
        DrawableGroupBrowser dgrpBrowser;
#ifndef TARGET_EMSCRIPTEN
        ImGui::FileBrowser fileBrowser;
#endif

        // For C-style callbacks from other code / emscripten
        static MapViewer *instance;
};

MapViewer *MapViewer::instance = NULL;

MapViewer::MapViewer(const Arguments& arguments):
    Platform::Sdl2Application{arguments, Configuration{}
        .setTitle("BZFlag Map Viewer")
        .setWindowFlags(Configuration::WindowFlag::Resizable),
        GLConfiguration{}
#if defined(MAGNUM_TARGET_GLES)
        // No multisampling for GLES, assume less capable machine
        .setVersion(GL::Version::GLES300)
#endif
        .setSampleCount(4)
        }
{
    using namespace Math::Literals;

    assert(instance == NULL);

    instance = this;

    loggingCallback = &blc;
    debugLevel = 0;

    Utility::Arguments args;
    args
        .setGlobalHelp("BZFlag Map Viewer")
        .parse(arguments.argc, arguments.argv);
    
    MagnumSceneManager::initScene();
    sceneRenderer.init();

    _sceneRoot = SOMGR.getObj("Scene");

    _cameraObject = new Object3D{};
    
    (*_cameraObject)
        .setParent(_sceneRoot)
        .translate(Vector3::zAxis(10.0f));
        
    
    (*(_camera = new SceneGraph::Camera3D{*_cameraObject}))
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(35.0_degf, 1.0f, 1.0f, 1000.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size());

    sceneRenderer.resizeViewport(GL::defaultFramebuffer.viewport().size().x(), GL::defaultFramebuffer.viewport().size().y());

    _manipulator = SOMGR.getObj("World");


    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::enable(GL::Renderer::Feature::Blending);

    // Important for proper rendering of skydome
    GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::LessOrEqual);

    _imgui = ImGuiIntegration::Context(Vector2{windowSize()}/dpiScaling(), windowSize(), framebufferSize());
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifndef TARGET_EMSCRIPTEN
    fileBrowser.SetTitle("Select Map File");
    fileBrowser.SetTypeFilters({".bzw"});
#endif

    editor.setReloadCallback(handleUpdateFromEditor);
    editor.setSaveCallback(handleSaveFromEditor);

    OnlineMapBrowser::setOnDownloadCompleteCallback(handleLoadFromMapBrowser);

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add,
        GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
        GL::Renderer::BlendFunction::OneMinusSourceAlpha);

    init();
}

void MapViewer::handleSaveFromEditor(const std::string& filename, const std::string& data) {
#ifdef TARGET_EMSCRIPTEN
    emscripten_browser_file::download(filename, "text/plain", data);
#else
    std::ofstream of(filename);
    of << data;
    of.close();
#endif
}

void MapViewer::handleUpdateFromEditor(const std::string& filename, const std::string& data) {
    instance->loadMap(filename, data, false);
}

void MapViewer::handleLoadFromMapBrowser(const CachedResource& mapRsc) {
    std::string data = std::string(mapRsc.getData().begin(), mapRsc.getData().end());
    instance->loadMap(mapRsc.getFilename(), data);
}

void MapViewer::exitEvent(ExitEvent& event) {
    event.setAccepted();
    MagnumTextureManager::instance().clear();
}

#ifdef TARGET_EMSCRIPTEN
void MapViewer::handleUploadedMap(const std::string& filename, const std::string& mime_type, std::string_view buffer, void*) {
    instance->loadMap(filename, std::string(buffer));
}
#endif

void MapViewer::showMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::MenuItem("Load")) {
#ifdef TARGET_EMSCRIPTEN
            emscripten_browser_file::upload(".bzw", handleUploadedMap, NULL);
#else
            fileBrowser.Open();
#endif
        }
        if (ImGui::BeginMenu("View")) {
            showMenuView();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("Renderer Settings", NULL, &showRendererSettings)) {}
            if (ImGui::MenuItem("Adjust Sun", NULL, &showAdjustSun)) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            showMenuDebug();
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("About")) {
            showAbout = true;
        }
        ImGui::EndMainMenuBar();
    }
}

void MapViewer::showMenuView() {
    if (ImGui::MenuItem("Map Browser", NULL, &showMapBrowser)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Map Editor", NULL, &showEditor)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Obstacle Browser", NULL, &showObsBrowser)) {}
    if (ImGui::MenuItem("Texture Browser", NULL, &showTMBrowser)) {}
    if (ImGui::MenuItem("Material Browser", NULL, &showMATBrowser)) {}
    if (ImGui::MenuItem("Texture Matrix Browser", NULL, &showTexMatBrowser)) {}
    if (ImGui::MenuItem("Dynamic Color Browser", NULL, &showDynColorBrowser)) {}
    if (ImGui::MenuItem("Physics Driver Browser", NULL, &showPhyDrvBrowser)) {}
    if (ImGui::MenuItem("Mesh Transform Browser", NULL, &showMeshTransformBrowser)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Material Viewer", NULL, &showMATViewer)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Cache Browser", NULL, &showCacheBrowser)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Drawable Group Browser", NULL, &showDrawableGroupBrowser)) {}
#ifndef MAGNUM_TARGET_GLES2
    ImGui::Separator();
    if (ImGui::MenuItem("Grid", NULL, &showGrid)) {}
#endif
}

void MapViewer::showMenuDebug() {
    if (ImGui::MenuItem("Profiler", NULL, &showProfiler)) {}
    if (ImGui::MenuItem("GL Info", NULL, &showGLInfo)) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Recompile World Mesh")) {
        worldSceneObjGen.destroyWorldObject();
        worldSceneObjGen.createWorldObject(&worldSceneBuilder);
    }
    if (ImGui::MenuItem("Force Load Material Textures")) {
        MAGNUMMATERIALMGR.forceLoadTextures();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Pipeline Texture Browser", NULL, &showPipelineTexBrowser)) {}
}

void MapViewer::drawWindows() {
    if (showProfiler) {
        ImGui::Begin("Profiler", &showProfiler);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0/Double(ImGui::GetIO().Framerate), Double(ImGui::GetIO().Framerate));
        ImGui::End();
    }

    if (showTMBrowser) {
        tmBrowser.draw("Texture Browser", &showTMBrowser);
    }

    if (showMATBrowser) {
        matBrowser.draw("Material Browser", &showMATBrowser);
    }

    if (showMATViewer) {
        matViewer.draw("Material Viewer", &showMATViewer);
    }

    if (showObsBrowser) {
        obsBrowser.draw("Obstacle Browser", &showObsBrowser);
    }

    static std::string info = getGLInfo();
    if (showGLInfo) {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("OpenGL Info", &showGLInfo);
        ImGui::TextWrapped(info.c_str());
        ImGui::End();
    }

    if (showEditor) {
        editor.draw("Map Editor", &showEditor);
    }

#ifndef TARGET_EMSCRIPTEN
    // Filebrowser tracks whether to draw internally
    fileBrowser.Display();
    if (fileBrowser.HasSelected()) {
        loadMap(fileBrowser.GetSelected().string(), "", true);
        fileBrowser.ClearSelected();
    }
#endif

    if (showAbout) {
        about.draw("About", &showAbout);
    }

    if (showCacheBrowser) {
        cacheBrowser.draw("Cache Browser", &showCacheBrowser);
    }

    if (showTexMatBrowser) {
        texMatBrowser.draw("Texture Matrix Browser", &showTexMatBrowser);
    }

    if (showDynColorBrowser) {
        dynColorBrowser.draw("Dynamic Color Browser", &showDynColorBrowser);
    }

    if (showMeshTransformBrowser) {
        meshTFBrowser.draw("Mesh Transform Browser", &showMeshTransformBrowser);
    }

    if (showPhyDrvBrowser) {
        phyDrvBrowser.draw("Physics Driver Browser", &showPhyDrvBrowser);
    }

    if (showMapBrowser) {
        mapBrowser.draw("Map Browser", &showMapBrowser);
    }

    if (showDrawableGroupBrowser) {
        dgrpBrowser.draw("Drawable Group Browser", &showDrawableGroupBrowser);
    }

    if (showPipelineTexBrowser) {
        sceneRenderer.drawPipelineTexBrowser("Pipeline Texture Browser", &showPipelineTexBrowser);
    }

    if (showAdjustSun) {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Adjust Sun", &showAdjustSun);
        auto pos = sceneRenderer.getSunPosition();
        static float x = pos.x(), y = pos.y(), z = pos.z();
        ImGui::DragFloat("Sun X", &x, 10.0f, -10000.0f, 10000.0f, "%.2f");
        ImGui::DragFloat("Sun Y", &y, 10.0f, -10000.0f, 10000.0f, "%.2f");
        ImGui::DragFloat("Sun Z", &z, 10.0f, 1000.0f, 10000.0f, "%.2f");
        sceneRenderer.setSunPosition({x, y, z});
        ImGui::End();
    }

    if (showRendererSettings) {
        sceneRenderer.drawSettings("Renderer Settings", &showRendererSettings);
    }
}

void MapViewer::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    _imgui.newFrame();

    showMainMenuBar();
    drawWindows();

    _imgui.updateApplicationCursor(*this);

    if(ImGui::GetIO().WantTextInput && !isTextInputActive())
        startTextInput();
    else if(!ImGui::GetIO().WantTextInput && isTextInputActive())
        stopTextInput();

    sceneRenderer.renderScene(_camera);

    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    _imgui.drawFrame();

    swapBuffers();
}


void MapViewer::tickEvent() {
    loopIteration();
    redraw();
}

void MapViewer::viewportEvent(ViewportEvent& e) {
    GL::defaultFramebuffer.setViewport({{}, e.framebufferSize()});
    _imgui.relayout(Vector2{e.windowSize()}/e.dpiScaling(),
        e.windowSize(), e.framebufferSize());
    _camera->setViewport(e.windowSize());
}

void MapViewer::keyPressEvent(KeyEvent& event) {
    if(_imgui.handleKeyPressEvent(event)) return;
}

void MapViewer::keyReleaseEvent(KeyEvent& event) {
    if(_imgui.handleKeyReleaseEvent(event)) return;
}

void MapViewer::mousePressEvent(MouseEvent& e) {
    if(_imgui.handleMousePressEvent(e)) return;
    if (e.button() == MouseEvent::Button::Left)
        _previousPosition = positionOnSphere(e.position());
}

void MapViewer::mouseReleaseEvent(MouseEvent& e) {
    if(_imgui.handleMouseReleaseEvent(e)) return;
    if (e.button() == MouseEvent::Button::Left)
        _previousPosition = Vector3{};
}

void MapViewer::mouseScrollEvent(MouseScrollEvent& e) {
    if(_imgui.handleMouseScrollEvent(e)) {
        /* Prevent scrolling the page */
        e.setAccepted();
        return;
    }
    e.setAccepted();
    if (!e.offset().y()) return;
    const Float distance = _cameraObject->transformation().translation().z();
    _cameraObject->translate(Vector3::zAxis(distance*(1.0f - (e.offset().y() > 0 ? 1/0.85f : 0.85f))));
    redraw();
}

void MapViewer::mouseMoveEvent(MouseMoveEvent &e) {
    if(_imgui.handleMouseMoveEvent(e)) return;
    if (!(e.buttons() & MouseMoveEvent::Button::Left)) return;

    const Vector3 currentPosition = positionOnSphere(e.position());
    const Vector3 axis = Math::cross(_previousPosition, currentPosition);

    if (_previousPosition.length() < 0.001f || axis.length() < 0.001f) return;

    _manipulator->rotate(Math::angle(_previousPosition, currentPosition), axis.normalized());
    _previousPosition = currentPosition;

    redraw();
}

void MapViewer::textInputEvent(TextInputEvent& e) {
    if(_imgui.handleTextInputEvent(e)) return;
}

Vector3 MapViewer::positionOnSphere(const Vector2i& position) const {
    const Vector2 positionNormalized = Vector2{position}/Vector2{_camera->viewport()} - Vector2{0.5f};
    const Float length = positionNormalized.length();
    const Vector3 result(length > 1.0f ? Vector3(positionNormalized, 0.0f) : Vector3(positionNormalized, 1.0f - length));
    return (result * Vector3::yScale(-1.0f)).normalized();
}

void MapViewer::resetBZDB() {
    // set default DB entries
    for (unsigned int gi = 0; gi < numGlobalDBItems; ++gi)
    {
        assert(globalDBItems[gi].name != NULL);
        if (globalDBItems[gi].value != NULL)
        {
            BZDB.set(globalDBItems[gi].name, globalDBItems[gi].value);
            BZDB.setDefault(globalDBItems[gi].name, globalDBItems[gi].value);
        }
        BZDB.setPersistent(globalDBItems[gi].name, globalDBItems[gi].persistent);
        BZDB.setPermission(globalDBItems[gi].name, globalDBItems[gi].permission);
    }

    BZDBCache::init();
    BZDBLOCAL.init();

    loadBZDBDefaults();
}

void MapViewer::init() {
    bzfsrand((unsigned int)time(0));
    
    resetBZDB();

    setErrorCallback(startupErrorCallback);

    CACHEMGR.loadIndex();
    CACHEMGR.limitCacheSize();
}

void MapViewer::loadMap(std::string path, const std::string& data, bool reloadEditor)
{
    worldSceneObjGen.destroyWorldObject();
    worldSceneBuilder.reset();

    DYNCOLORMGR.clear();
    TEXMATRIXMGR.clear();
    MAGNUMMATERIALMGR.clear(false);
    PHYDRVMGR.clear();
    TRANSFORMMGR.clear();
    OBSTACLEMGR.clear();
    MagnumTextureManager::instance().clear();

    resetBZDB();

    if (data == "") {
        BZWReader* reader = new BZWReader(path);
        auto worldInfo = reader->defineWorldFromFile();
        delete reader;
    } else {
        BZWReader* reader = new BZWReader(path, data);
        auto worldInfo = reader->defineWorldFromFile();
        delete reader;
    }

    if (reloadEditor) {
        editor.loadFile(path, data, data == "");
    }

    MAGNUMMATERIALMGR.loadDefaultMaterials();

    Downloads::startDownloads(true, true, false);

}

void MapViewer::startupErrorCallback(const char* msg)
{
    Warning{} << msg;
}

void MapViewer::loopIteration()
{
    // set this step game time
    GameTime::setStepTime();

    // update the dynamic colors
    DYNCOLORMGR.update();

    // update the texture matrices
    TEXMATRIXMGR.update();

    // Needed for desktop. On emscripten this is a no-op
    cURLManager::perform();

    if (Downloads::requestFinalized()) {
        Downloads::finalizeDownloads();

        worldSceneObjGen.destroyWorldObject();
        worldSceneBuilder.reset();

        MAGNUMMATERIALMGR.forceLoadTextures();
        MAGNUMMATERIALMGR.rescanTextures();
        MagnumTextureManager::instance().disableAutomaticLoading();

        const ObstacleList& boxes = OBSTACLEMGR.getBoxes();
        for (int i = 0; i < boxes.size(); ++i) {
            worldSceneBuilder.addBox(*((BoxBuilding*) boxes[i]));
        }
        const ObstacleList& pyrs = OBSTACLEMGR.getPyrs();
        for (int i = 0; i < pyrs.size(); ++i) {
            worldSceneBuilder.addPyr(*((PyramidBuilding*) pyrs[i]));
        }
        const ObstacleList& bases = OBSTACLEMGR.getBases();
        for (int i = 0; i < bases.size(); ++i) {
            worldSceneBuilder.addBase(*((BaseBuilding*) bases[i]));
        }
        const ObstacleList& walls = OBSTACLEMGR.getWalls();
        for (int i = 0; i < walls.size(); ++i) {
            worldSceneBuilder.addWall(*((WallObstacle*) walls[i]));
        }
        const ObstacleList& teles = OBSTACLEMGR.getTeles();
        for (int i = 0; i < teles.size(); ++i) {
            worldSceneBuilder.addTeleporter(*((Teleporter*) teles[i]));
        }
        worldSceneBuilder.addGround(BZDBCache::worldSize);
        const ObstacleList& meshes = OBSTACLEMGR.getMeshes();
        for (int i = 0; i < meshes.size(); i++)
            worldSceneBuilder.addMesh (*((MeshObstacle*) meshes[i]));
        
        worldSceneObjGen.createWorldObject(&worldSceneBuilder);
    }
}

MAGNUM_APPLICATION_MAIN(MapViewer)