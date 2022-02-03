//
// Created by nic on 07/12/2021.
//

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <set>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>

#include <glm/glm.hpp>
#include <unordered_set>

#include "../renderer/vulkan/window.h"
#include "../renderer/vulkan/swapchain.h"
#include "../renderer/modelloader.h"
#include "../renderer/scene.h"
#include "../renderer/ui/uirenderer.h"
#include "../renderer/framebufferselector.h"
#include "../renderer/ui/objectproperties.h"
#include "../renderer/ui/objectlist.h"
#include "../renderer/ui/mainmenu.h"
#include "../renderer/texturelibrary.h"
#include "../renderer/rendermanager.h"

const uint32_t WIDTH = 1080 * 2;
const uint32_t HEIGHT = 720 * 2;

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class VulkanRasterizer {
  public:
    void run() {
        std::cout << "starting application\n";

        init();
        mainLoop();
        cleanup();
    }

  private:
    uint32_t version = VK_MAKE_VERSION(0, 3, 0);
    std::shared_ptr<Instance> instance;
    std::shared_ptr<Window> window;

    std::shared_ptr<ModelLoader> modelLoader;
    std::shared_ptr<Mesh> model;
    std::vector<std::shared_ptr<MeshInstance>> objects;

    std::shared_ptr<ObjectProperties> objectProperties;
    std::shared_ptr<ObjectList> objectList;
    std::shared_ptr<MainMenu> menuBar;

    std::shared_ptr<RenderManager> renderManager;

    void initWindow() {
        Window::initWindows();
        window = Window::create(instance, "API Test " + std::to_string(version), WIDTH, HEIGHT);
    }

    void createRenderManager() {
        TextureQualitySettings settings{};
        settings.anisotropy = 0.0f;
        settings.mipLevels = 1;

        CameraModel cameraModel{};
        cameraModel.fieldOfView = 75.0;
        cameraModel.cameraMode = CAMERA_MODE_PERSPECTIVE_INFINITE;
        cameraModel.clipNear = 0.1;

        renderManager = RenderManager::create(instance, window, settings, cameraModel);
    }

    void init() {
        createInstance();
        initWindow();
        createRenderManager();
        loadModels();
        createScene();
        createUI();
    }

    void createUI() {
        TextureQualitySettings settings{};
        settings.mipLevels = 1;
        settings.anisotropy = 0;

        objectProperties = ObjectProperties::create(renderManager->getScene());
        renderManager->getUIRenderer()->submitDrawable(objectProperties);
        objectList = ObjectList::create(renderManager->getScene());
        renderManager->getUIRenderer()->submitDrawable(objectList);
        renderManager->getSceneWriter()->setFilename(renderManager->getSceneWriter()->getFilename()
                                                     + "/resources/saves/test.ccs");
        menuBar = MainMenu::create(renderManager->getTextureLibrary(), renderManager->getSceneWriter());
        renderManager->getUIRenderer()->submitDrawable(menuBar);
    }

    void loadModels() {
        modelLoader = ModelLoader::create();

        auto vertexShader = VertexShader::create(renderManager->getDevice(),
                                                 "main", "resources/shaders/compiled/"
                                                         "PBR_basic.vert.spv");
        auto fragmentShader = FragmentShader::create(renderManager->getDevice(),
                                                     "main", "resources/shaders/compiled/"
                                                             "PBR_basic.frag.spv");
        std::vector<std::shared_ptr<GraphicsShader>> shaders{vertexShader, fragmentShader};

        MaterialProperty materialProperty{};
        materialProperty.input =
            static_cast<MaterialPropertyInput>(MATERIAL_PROPERTY_INPUT_CONSTANT | MATERIAL_PROPERTY_INPUT_TEXTURE);
        materialProperty.size = MATERIAL_PROPERTY_SIZE_8;
        materialProperty.count = MATERIAL_PROPERTY_COUNT_3;
        materialProperty.format = MATERIAL_PROPERTY_FORMAT_UINT;

        MaterialPropertyLayout propertyLayout{{ materialProperty }};
        auto layoutBuilt = buildLayout(propertyLayout);

        auto masterMaterial = MasterMaterial::create(renderManager->getDevice(), shaders, 2, renderManager->getExtend(),
                                                     layoutBuilt,
                                                     renderManager->getRenderPass(),
                                                     renderManager->getDescriptorManager(),
                                                     "basicMaterial");
        masterMaterial->updateImageSampler(renderManager->getTextureLibrary());

        auto texture = renderManager->getTextureLibrary()->createTexture("/home/nic/Downloads/viking-room/viking-room"
                                                                         ".png", VK_FORMAT_R8G8B8A8_UNORM);

        char* parameters = new char[layoutBuilt.totalSize];
        auto data = glm::vec3(1, 1, 1);
        memcpy(parameters, layoutBuilt.properties[0]->cast(&data), layoutBuilt.properties[0]->getSize());

        auto material = Material::create(masterMaterial, renderManager->getDescriptorManager(), { texture },
                                         parameters);
        renderManager->getSceneWriter()->material = material;
        model = renderManager->getMeshLibrary()->createMesh("/home/nic/Downloads/viking-room/viking-room.obj", material)
            .front();

        for (size_t i = 0; i < 10; i++) {
            auto object = MeshInstance::create(model);
            object->setScale(glm::vec3(10.0f));
            object->setPosition(glm::normalize(glm::vec3(
                static_cast <float> (rand()) / static_cast <float> (RAND_MAX),
                static_cast <float> (rand()) / static_cast <float> (RAND_MAX),
                static_cast <float> (rand()) / static_cast <float> (RAND_MAX))
                - glm::vec3(0.5f)) *
            glm::vec3(20));
            objects.push_back(object);
        }
    }

    void createScene() {
        for (const auto& object : objects)
            renderManager->getScene()->submitInstance(object);
        renderManager->getScene()->collectRenderBuffers();
        renderManager->getScene()->bakeMaterials(true);
    }

    void mainLoop() {
        std::cout << "entering main loop\n";

        while (!glfwWindowShouldClose(window->getWindow())) {
            glfwPollEvents();

            renderManager->processInputs();
            objectProperties->setObjectByID(renderManager->getScene()->getSelected());
            renderManager->drawFrame();
        }

        std::cout << "exiting\n";
    }

    static void cleanup() {
        std::cout << "cleaning up\n";

        glfwTerminate();
    }

    void createInstance() {
        glfwInit();

        instance = Instance::create("api_test", version, true);
    }
};

int main() {
    VulkanRasterizer app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "application closed\n";

    return EXIT_SUCCESS;
}