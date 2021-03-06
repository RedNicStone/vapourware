//
// Created by nic on 28/01/2022.
//

#include "rendermanager.h"


std::shared_ptr<RenderManager> RenderManager::create(const std::shared_ptr<Instance> &instance,
                                                     const std::shared_ptr<Window> &window,
                                                     const TextureQualitySettings &textureQuality,
                                                     const RenderQualityOptions &renderQuality,
                                                     const CameraModel &cameraModel) {
    auto renderManager = std::make_shared<RenderManager>();

    window->setUserPointer(renderManager.get());

    renderManager->instance = instance;
    renderManager->window = window;
    renderManager->textureQualitySettings = textureQuality;
    renderManager->renderQuality = renderQuality;
    renderManager->cameraModel = cameraModel;

    renderManager->createRenderObjects();

    return renderManager;
}

void RenderManager::pickPhysicalDevice() {
    for (const auto &possiblePhysicalDevice: instance->getPhysicalDevices()) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(possiblePhysicalDevice->getHandle(), &deviceProperties);

        std::string name = deviceProperties.deviceName;
        std::cout << " * " << name;
        if (name.find("llvmpipe") == std::string::npos) { // check if device is virtual
            physicalDevice = possiblePhysicalDevice;
            std::cout << "  [USING]";
        }
        std::cout << std::endl;

    }
}

void RenderManager::createLogicalDevice() {
    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    auto familyHandler = physicalDevice->getQueueFamilyHandler();

    optional<uint32_t> graphicsFamily = familyHandler->getOptimalQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (graphicsFamily.has_value())
        familyHandler->appendQueueFamilyAllocations(graphicsFamily.value(), 1);

    optional<uint32_t> transferFamily = familyHandler->getOptimalQueueFamily(VK_QUEUE_TRANSFER_BIT);
    if (transferFamily.has_value())
        familyHandler->appendQueueFamilyAllocations(transferFamily.value(), 1);

    optional<uint32_t> computeFamily = familyHandler->getOptimalQueueFamily(VK_QUEUE_COMPUTE_BIT);
    if (computeFamily.has_value())
        familyHandler->appendQueueFamilyAllocations(computeFamily.value(), 1);

    optional<uint32_t> presentFamily = familyHandler->getOptimalPresentQueue(window);
    if (presentFamily.has_value())
        familyHandler->appendQueueFamilyAllocations(presentFamily.value(), 1);

    device = Device::create(physicalDevice, deviceExtensions, deviceFeatures);

    graphicsQueue = physicalDevice->getQueueFamilyHandler()->getQueueFamily(graphicsFamily.value())->getQueue(0);
    transferQueue = physicalDevice->getQueueFamilyHandler()->getQueueFamily(transferFamily.value())->getQueue(0);
    computeQueue = physicalDevice->getQueueFamilyHandler()->getQueueFamily(computeFamily.value())->getQueue(0);
    presentQueue = physicalDevice->getQueueFamilyHandler()->getQueueFamily(presentFamily.value())->getQueue(0);

    std::cout << "Graphics queue: " << graphicsQueue->getHandle() << std::endl;
    std::cout << "Transfer queue: " << graphicsQueue->getHandle() << std::endl;
    std::cout << "Compute queue: " << graphicsQueue->getHandle() << std::endl;
    std::cout << "Present queue: " << presentQueue->getHandle() << std::endl;
}

void RenderManager::createCommandPools() {
    graphicsPool = CommandPool::create(device, graphicsQueue, 0);
    transferPool = CommandPool::create(device, transferQueue, 0);
    computePool = CommandPool::create(device, computeQueue, 0);
    presentPool = CommandPool::create(device, presentQueue, 0);
}

void RenderManager::createSwapChain() {
    std::vector<uint32_t>
        renderPassQueues = {graphicsQueue->getQueueFamilyIndex(), presentQueue->getQueueFamilyIndex()};
    Utils::remove(renderPassQueues);

    std::vector<uint32_t> graphicsQueues = {graphicsQueue->getQueueFamilyIndex()};

    swapChain = SwapChain::create(device, window, presentQueue, renderQuality.bufferCount, renderPassQueues);

    imageCount = swapChain->getImageCount();
    VkExtent2D extent = swapChain->getSwapExtent();
    swapChainExtent = extent;

    renderTargets.clear();
    renderTargetViews.clear();
    renderTargets.resize(RENDER_TARGET_MAX);
    renderTargetViews.resize(RENDER_TARGET_MAX);
    for (uint32_t i = 0; i < RENDER_TARGET_MAX; i++) {
        VkFormat imageFormat;
        if (i == RENDER_TARGET_DEPTH)
            imageFormat = VK_FORMAT_D32_SFLOAT;
        else if (i == RENDER_TARGET_POSITION)
            imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        else if (i == RENDER_TARGET_NORMAL)
            imageFormat = VK_FORMAT_R16G16_UNORM;
        else if (i == RENDER_TARGET_AMBIENT)
            imageFormat = VK_FORMAT_R16_UNORM;
        else
            imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        if (i == RENDER_TARGET_DEPTH)
            imageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else if (i == RENDER_TARGET_AMBIENT)
            imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        else
            imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        renderTargets[i] =
            Image::create(device, swapChain->getSwapExtent(), 1, imageFormat, imageUsage, graphicsQueues);

        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        renderTargets[i]->setLayout(layout);

        VkImageAspectFlags imageAspect;
        if (i == RENDER_TARGET_DEPTH)
            imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;

        renderTargetViews[i] = ImageView::create(renderTargets[i], VK_IMAGE_VIEW_TYPE_2D, imageAspect);
    }

    objectBufferImage =
        Image::create(device,
                      swapChain->getSwapExtent(),
                      1,
                      VK_FORMAT_R32_UINT,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                      graphicsQueues,
                      VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    objectBufferImageView = ImageView::create(objectBufferImage, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    objectBuffer = FramebufferSelector::create(device, graphicsQueue, extent);
}

void RenderManager::createRenderPass() {

    ////
    //  Geometry pass
    ////

    geometryRenderPass = RenderPass::create(device);

    uint32_t
        objectIDAttachment =
        geometryRenderPass->submitImageAttachment(objectBufferImage->getFormat(),
                                                  VK_SAMPLE_COUNT_1_BIT,
                                                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  VK_ATTACHMENT_STORE_OP_STORE,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    std::vector<uint32_t> geometryAttachments(geometryRenderTargets.size());
    std::vector<VkAttachmentReference> geometryOutputAttachmentReferences(geometryRenderTargets.size());
    for (auto i: geometryRenderTargets) {
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (i == RENDER_TARGET_DEPTH)
            layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        geometryAttachments[i] =
            (geometryRenderPass->submitImageAttachment(renderTargets[i]->getFormat(),
                                                       VK_SAMPLE_COUNT_1_BIT,
                                                       VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                       VK_ATTACHMENT_STORE_OP_STORE,
                                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                                       layout));

        geometryOutputAttachmentReferences[i].attachment = geometryAttachments[i];
        geometryOutputAttachmentReferences[i].layout = layout;
    }

    VkAttachmentReference objectIDAttachmentRef{};
    objectIDAttachmentRef.attachment = objectIDAttachment;
    objectIDAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentReference> geometryPipelineColorReference(geometryOutputAttachmentReferences);
    geometryPipelineColorReference.erase(geometryPipelineColorReference.begin() + RENDER_TARGET_DEPTH);
    geometryPipelineColorReference.push_back(objectIDAttachmentRef);
    std::vector<VkAttachmentReference> geometryPipelineInputReference = {};
    geometrySubpass =
        geometryRenderPass->submitSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          geometryPipelineColorReference,
                                          geometryPipelineInputReference,
                                          &geometryOutputAttachmentReferences[RENDER_TARGET_DEPTH],
                                          0);
    geometryRenderPass->submitDependency(geometrySubpass,
                                         VK_SUBPASS_EXTERNAL,
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_ACCESS_SHADER_READ_BIT,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    geometryRenderPass->build();

    ////
    //  UI/Shading pass
    ////

    UIRenderPass = RenderPass::create(device);

    uint32_t swapChainAttachment = UIRenderPass->submitSwapChainAttachment(swapChain, true);

    std::vector<uint32_t> UIAttachments(UIRenderTargets.size());
    std::vector<VkAttachmentReference> UIOutputAttachmentReferences(UIRenderTargets.size());
    std::vector<VkAttachmentReference> UIInputAttachmentReferences(UIRenderTargets.size());
    for (uint32_t i = 0; i < RENDER_TARGET_MAX; i++) {
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (i == RENDER_TARGET_DEPTH)
            layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        else if (i == RENDER_TARGET_AMBIENT)
            layout = VK_IMAGE_LAYOUT_GENERAL;
        else if (i &= RENDER_TARGET_NORMAL | RENDER_TARGET_POSITION)
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        UIAttachments[i] =
            (UIRenderPass->submitImageAttachment(renderTargets[i]->getFormat(),
                                                 VK_SAMPLE_COUNT_1_BIT,
                                                 VK_ATTACHMENT_LOAD_OP_LOAD,
                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                 layout,
                                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

        UIInputAttachmentReferences[i].attachment = UIAttachments[i];
        UIInputAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkAttachmentReference swapChainAttachmentRef{};
    swapChainAttachmentRef.attachment = swapChainAttachment;
    swapChainAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentReference> selectorPipelineOutputReference = {swapChainAttachmentRef};
    std::vector<VkAttachmentReference> selectorPipelineInputReference(UIInputAttachmentReferences);
    selectorSubpass =
        UIRenderPass->submitSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    selectorPipelineOutputReference,
                                    selectorPipelineInputReference,
                                    nullptr,
                                    0);

    std::vector<VkAttachmentReference> UIPipelineColorReference = {swapChainAttachmentRef};
    std::vector<VkAttachmentReference> UIPipelineInputReference = {};
    uiSubpass =
        UIRenderPass->submitSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    UIPipelineColorReference,
                                    UIPipelineInputReference,
                                    nullptr,
                                    0);

    UIRenderPass->submitDependency(VK_SUBPASS_EXTERNAL,
                                   selectorSubpass,
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    UIRenderPass->submitDependency(selectorSubpass,
                                   uiSubpass,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    UIRenderPass->submitDependency(uiSubpass,
                                   VK_SUBPASS_EXTERNAL,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_ACCESS_MEMORY_READ_BIT,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    UIRenderPass->build();
}

void RenderManager::createFrameBuffer() {
    std::vector<std::shared_ptr<ImageView>> geometryFrameViews(geometryRenderTargets.size() + 1);
    geometryFrameViews[0] = objectBufferImageView;
    uint32_t n = 1;
    for (auto i: geometryRenderTargets) {
        geometryFrameViews[n] = renderTargetViews[i];
        n++;
    }

    std::vector<std::vector<std::shared_ptr<ImageView>>> geometryImageViews(imageCount, geometryFrameViews);

    geometryFrameBuffer =
        FrameBuffer::create(device, geometryRenderPass, swapChain->getSwapExtent(), geometryImageViews);

    std::vector<std::shared_ptr<ImageView>> UIFrameViews(UIRenderTargets.size());
    n = 0;
    for (auto i: UIRenderTargets) {
        UIFrameViews[n] = renderTargetViews[i];
        n++;
    }

    std::vector<std::vector<std::shared_ptr<ImageView>>> UIImageViews(imageCount, UIFrameViews);

    UIFrameBuffer = FrameBuffer::create(device, UIRenderPass, swapChain, UIImageViews);
}

void RenderManager::createSceneObjects() {
    poolManager = DescriptorPoolManager::create(device);

    camera = Camera::create(cameraModel, swapChainExtent);
    scene = Scene::create(shared_from_this(), poolManager, graphicsQueue, transferPool, computePool, camera);
    auto currentDir = std::string(get_current_dir_name());
    sceneWriter = SceneWriter::create(shared_from_this(), currentDir);

    textureLibrary = TextureLibrary::create(device, graphicsQueue, graphicsPool, textureQualitySettings);
    materialLibrary =
        MaterialLibrary::create(device,
                                transferPool,
                                poolManager,
                                textureLibrary,
                                {graphicsQueue},
                                geometryRenderPass,
                                static_cast<uint32_t>(geometryRenderTargets.size()),
                                swapChainExtent);
    meshLibrary = MeshLibrary::create(textureLibrary, materialLibrary);
}

void RenderManager::createDefaultMaterialLayout() {
    auto vertexShader = VertexShader::create(device, "main", "resources/shaders/compiled/"
                                                             "PBR_basic.vert.spv");
    auto fragmentShader = FragmentShader::create(device, "main", "resources/shaders/compiled/"
                                                                 "PBR_basic.frag.spv");
    std::vector<std::shared_ptr<GraphicsShader>> shaders{vertexShader, fragmentShader};

    std::vector<MaterialProperty> materialProperties(1);
    materialProperties[0].input =
        static_cast<MaterialPropertyInput>(MATERIAL_PROPERTY_INPUT_CONSTANT | MATERIAL_PROPERTY_INPUT_TEXTURE);
    materialProperties[0].size = MATERIAL_PROPERTY_SIZE_8;
    materialProperties[0].count = MATERIAL_PROPERTY_COUNT_4;
    materialProperties[0].format = MATERIAL_PROPERTY_FORMAT_SRGB;
    materialProperties[0].attributeName = "diffuse";

    MaterialPropertyLayout propertyLayout{materialProperties};
    auto layoutBuilt = buildLayout(propertyLayout);

    defaultMaterialTemplate = MasterMaterialTemplate::create(shaders, layoutBuilt);
}

void RenderManager::createUIObjects() {
    uiRenderer = UIRenderer::create(graphicsQueue, graphicsPool, UIRenderPass, window, imageCount, uiSubpass);
}

void RenderManager::createRenderObjects() {
    glfwSetMouseButtonCallback(window->getWindow(), mouseButtonCallback);
    glfwSetKeyCallback(window->getWindow(), keyCallback);

    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPools();
    createSwapChain();
    createRenderPass();
    createFrameBuffer();
    createSceneObjects();
    createDefaultMaterialLayout();
    createUIObjects();
    createPostProcessingPipelines();
}

void RenderManager::recreateSwapChain(bool newImageCount) {
    createSwapChain();
    createRenderPass();
    createFrameBuffer();
    createDefaultMaterialLayout();
    createPostProcessingPipelines();
    camera->updateCameraModel(cameraModel, swapChainExtent);

    if (newImageCount) {
        createUIObjects();
    }
}

void RenderManager::processMouseInputs() {
    if (glfwGetWindowAttrib(window->getWindow(), GLFW_HOVERED) == GLFW_TRUE) {
        auto &io = UIRenderer::getIO();

        if (!mouseCaptured && !io.WantCaptureMouse) {
            VkExtent2D mousePos{static_cast<uint32_t>(io.MousePos.x), static_cast<uint32_t>(io.MousePos.y)};
            uint32_t objectID = objectBuffer->getIDAtPosition(mousePos);

            scene->setHovered(objectID);
        } else if (io.WantCaptureMouse) {
            scene->setHovered(0);
        } else if (mouseCaptured) {
            auto moveSpeed = 0.02f;
            if (glfwGetKey(window->getWindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                moveSpeed *= 3;
            }
            if (glfwGetKey(window->getWindow(), GLFW_KEY_W) == GLFW_PRESS) {
                camera->move(camera->getRotation() * glm::vec3(moveSpeed));
            }
            if (glfwGetKey(window->getWindow(), GLFW_KEY_S) == GLFW_PRESS) {
                camera->move(-camera->getRotation() * glm::vec3(moveSpeed));
            }
            if (glfwGetKey(window->getWindow(), GLFW_KEY_A) == GLFW_PRESS) {
                camera->move(-glm::cross(camera->getRotation(), {0, 1, 0}) * glm::vec3(moveSpeed));
            }
            if (glfwGetKey(window->getWindow(), GLFW_KEY_D) == GLFW_PRESS) {
                camera->move(glm::cross(camera->getRotation(), {0, 1, 0}) * glm::vec3(moveSpeed));
            }

            camera->rotate(io.MouseDelta.x * mouseSpeed, {0, -1, 0});
            camera->rotate(io.MouseDelta.y * mouseSpeed, glm::normalize(glm::cross({0, 1, 0}, camera->getRotation())));
        }

        if (!io.WantCaptureKeyboard) {
            if (glfwGetKey(window->getWindow(), GLFW_KEY_1) == GLFW_PRESS
                || glfwGetKey(window->getWindow(), GLFW_KEY_0) == GLFW_PRESS)
                activePresentTarget = 0;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_2) == GLFW_PRESS)
                activePresentTarget = 1;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_3) == GLFW_PRESS)
                activePresentTarget = 2;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_4) == GLFW_PRESS)
                activePresentTarget = 3;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_5) == GLFW_PRESS)
                activePresentTarget = 4;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_6) == GLFW_PRESS)
                activePresentTarget = 5;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_7) == GLFW_PRESS)
                activePresentTarget = 6;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_8) == GLFW_PRESS)
                activePresentTarget = 7;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_9) == GLFW_PRESS)
                activePresentTarget = 8;
            else if (glfwGetKey(window->getWindow(), GLFW_KEY_9) == GLFW_PRESS)
                activePresentTarget = 9;
            memcpy(viewportUniform->getDataHandle(), &activePresentTarget, sizeof(uint32_t));
        }
    }
}

void RenderManager::resizeSwapChain(uint32_t newImageCount) {
    bool bNewImageCount = newImageCount != 0 && newImageCount != imageCount;
    if (bNewImageCount)
        imageCount = newImageCount;
    recreateSwapChain(newImageCount);
}

void RenderManager::drawFrame_() {
    // acquire frame
    swapChain->acquireNextFrame();
    uint32_t index = swapChain->getCurrentImageIndex();

    // allocate command buffer
    drawCommandBuffer = CommandBuffer::create(graphicsPool);

    std::vector<std::shared_ptr<Semaphore>> signalSemaphores = swapChain->getRenderSignalSemaphores();
    std::vector<std::shared_ptr<Semaphore>> waitSemaphores = swapChain->getRenderWaitSemaphores();
    waitSemaphores.push_back(scene->getWaitSemaphore());

    std::vector<std::shared_ptr<Semaphore>> ssaoSemaphores = {ssaoSemaphore};
    std::vector<std::shared_ptr<Semaphore>> uiSemaphores = {uiSemaphore};

    // begin recording instructions
    drawCommandBuffer->beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // clear and begin render pass (Geometry)
    std::vector<VkClearValue> clearColor(geometryRenderTargets.size() + 1);
    clearColor[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_DEPTH + 1].depthStencil = {1.0, 0};
    clearColor[RENDER_TARGET_DIFFUSE + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_POSITION + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_NORMAL + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    drawCommandBuffer->beginRenderPass(geometryRenderPass,
                                       geometryFrameBuffer,
                                       clearColor,
                                       index,
                                       geometryFrameBuffer->getExtent(),
                                       {0, 0});

    // write all commands to draw the scene
    scene->bakeGraphicsBuffer(drawCommandBuffer);

    // end the render pass
    drawCommandBuffer->endRenderPass();

    // transition the object buffer into a layout we can copy from
    objectBufferImage->setLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    objectBufferImage->transitionImageLayout(drawCommandBuffer,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VK_ACCESS_TRANSFER_READ_BIT);

    // transition the CPU sided object buffer into a layout we can copy to
    objectBuffer->transferLayoutWrite(drawCommandBuffer);

    VkImageSubresourceLayers imageLayers{};
    imageLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageLayers.baseArrayLayer = 0;
    imageLayers.layerCount = 1;
    imageLayers.mipLevel = 0;

    std::vector<VkImageCopy> copyRegions(1);
    copyRegions[0].extent = {swapChainExtent.width, swapChainExtent.height, 1};
    copyRegions[0].srcOffset = {0, 0, 0};
    copyRegions[0].dstOffset = {0, 0, 0};
    copyRegions[0].srcSubresource = imageLayers;
    copyRegions[0].dstSubresource = imageLayers;

    // copy GPU sided object buffer to the CPU sided object buffer
    // todo? only copy region of interest ?
    drawCommandBuffer->copyImage(objectBufferImage, objectBuffer->getImage(), copyRegions);

    // transition the object buffer into a layout we can write to from the fragment shader
    objectBufferImage->transitionImageLayout(drawCommandBuffer,
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                             VK_ACCESS_SHADER_WRITE_BIT);

    // transition the CPU sided object buffer into a layout we can read from
    objectBuffer->transferLayoutRead(drawCommandBuffer);

    // compile all commands [resource intensive part]
    drawCommandBuffer->endCommandBuffer();

    // submit the buffer for execution
    drawCommandBuffer->submitToQueue(ssaoSemaphores,
                                     {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
                                     waitSemaphores,
                                     graphicsQueue,
                                     nullptr);

    renderTargets[RENDER_TARGET_AMBIENT]->setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
    renderTargets[RENDER_TARGET_NORMAL]->setLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    renderTargets[RENDER_TARGET_POSITION]->setLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    drawCommandBuffer = CommandBuffer::create(computePool);

    drawCommandBuffer->beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    renderTargets[RENDER_TARGET_AMBIENT]->transitionImageLayout(drawCommandBuffer,
                                                                VK_IMAGE_LAYOUT_GENERAL,
                                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                VK_ACCESS_SHADER_WRITE_BIT);
    renderTargets[RENDER_TARGET_NORMAL]->transitionImageLayout(drawCommandBuffer,
                                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                               VK_ACCESS_SHADER_READ_BIT);
    renderTargets[RENDER_TARGET_POSITION]->transitionImageLayout(drawCommandBuffer,
                                                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                 VK_ACCESS_SHADER_READ_BIT);

    if (renderQuality.enableSSAO)
        ssaoPipeline->bakeGraphicsBuffer(drawCommandBuffer);

    drawCommandBuffer->endCommandBuffer();

    // submit the buffer for execution
    drawCommandBuffer->submitToQueue(uiSemaphores,
                                     {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
                                     ssaoSemaphores,
                                     computeQueue,
                                     swapChain->getPresentFence());

    // allocate command buffer
    drawCommandBuffer = CommandBuffer::create(graphicsPool);

    // begin recording instructions
    drawCommandBuffer->beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // clear and begin render pass (Geometry)
    clearColor = std::vector<VkClearValue>(UIRenderTargets.size() + 1);
    clearColor[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_DEPTH + 1].depthStencil = {1.0, 0};
    clearColor[RENDER_TARGET_DIFFUSE + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_POSITION + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_NORMAL + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearColor[RENDER_TARGET_AMBIENT + 1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    drawCommandBuffer->beginRenderPass(UIRenderPass,
                                       UIFrameBuffer,
                                       clearColor,
                                       index,
                                       UIFrameBuffer->getExtent(),
                                       {0, 0});

    viewportSelector->bakeGraphicsBuffer(drawCommandBuffer);

    // begin next subpass (UI)
    drawCommandBuffer->nextSubpass();

    // write all commands to draw the UI
    uiRenderer->draw(drawCommandBuffer);

    // end the render pass
    drawCommandBuffer->endRenderPass();

    // compile all commands [resource intensive part]
    drawCommandBuffer->endCommandBuffer();

    // submit the buffer for execution
    drawCommandBuffer->submitToQueue(signalSemaphores,
                                     {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                                     uiSemaphores,
                                     graphicsQueue,
                                     nullptr);

    // present the image and advance swap chain
    swapChain->presentImage();
}

void RenderManager::processInputs() {
    processMouseInputs();
    scene->updateUBO();
}

void RenderManager::keyCallback(GLFWwindow *glfwWindow, int key, int scancode, int action, int mods) {
    auto
        renderManager =
        static_cast<RenderManager *>(static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow))->getUserPointer());
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !UIRenderer::getIO().WantCaptureKeyboard) {
        if (renderManager->mouseCaptured)
            glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        renderManager->mouseCaptured = !renderManager->mouseCaptured;
        renderManager->uiRenderer->setHidden(renderManager->mouseCaptured);
    }
}

void RenderManager::mouseButtonCallback(GLFWwindow *glfwWindow, int button, int action, int mods) {
    auto
        renderManager =
        static_cast<RenderManager *>(static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow))->getUserPointer());
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !UIRenderer::getIO().WantCaptureMouse)
        renderManager->scene->setSelected(renderManager->scene->getHovered());
}

RenderManager::~RenderManager() {
    graphicsQueue->waitForIdle();
    transferQueue->waitForIdle();
    computeQueue->waitForIdle();
    presentQueue->waitForIdle();
    device->waitIdle();
}

void RenderManager::updateRenderData() {
    materialLibrary->pushParameters();
    scene->bakeMaterials(true);
}

void RenderManager::loadMesh(const std::string &filename, bool normalizePos) {
    auto meshes = meshLibrary->createMesh(filename, defaultMaterialTemplate, normalizePos);
    for (const auto &mesh: meshes)
        scene->submitInstance(MeshInstance::create(mesh));
}

void RenderManager::createPostProcessingPipelines() {
    auto shader = FragmentShader::create(device, "main", "resources/shaders/compiled/viewport_selector.frag.spv");

    std::vector<VkDescriptorSetLayoutBinding>
        bindings = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},};
    bindings.reserve(RENDER_TARGET_MAX + 1);
    for (uint32_t i = 0; i < RENDER_TARGET_MAX; i++) {
        bindings.push_back({i + 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    }

    std::shared_ptr<DescriptorSetLayout> viewportDescriptorLayout = DescriptorSetLayout::create(device, bindings);
    viewportDescriptor = poolManager->allocate(viewportDescriptorLayout);

    viewportSelector =
        ShadingPipeline::create(UIRenderPass, {viewportDescriptor}, shader, swapChainExtent, 1, selectorSubpass);

    viewportDescriptor->updateImages(renderTargetViews,
                                     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     1);

    viewportUniform = UniformBuffer::create(device, transferQueue, sizeof(uint32_t) + sizeof(glm::bvec1));
    auto ssaoEnabled = glm::bvec1(renderQuality.enableSSAO);
    memcpy((char *) viewportUniform->getDataHandle() + sizeof(uint32_t), &ssaoEnabled, sizeof(glm::bvec1));
    viewportDescriptor->updateUniformBuffer(viewportUniform, 0);

    ssaoPipeline =
        SSAOPipeline::create(device,
                             poolManager,
                             transferPool,
                             computePool,
                             renderTargetViews[RENDER_TARGET_AMBIENT],
                             renderTargetViews[RENDER_TARGET_NORMAL],
                             renderTargetViews[RENDER_TARGET_POSITION],
                             scene->getDescriptorSet(),
                             static_cast<float>(cameraModel.fieldOfView),
                             swapChainExtent,
                             renderQuality.SSAOSampleCount,
                             renderQuality.SSAOSampleRadius);

    ssaoSemaphore = Semaphore::create(device);
    uiSemaphore = Semaphore::create(device);
}

void RenderManager::invalidateFrame() {
    std::cout << "Frame invalidated, discarding frame and redrawing" << std::endl;
    throw std::runtime_error("Frame invalidated!");
}

void RenderManager::waitForExecution() {
    graphicsQueue->waitForIdle();
    presentQueue->waitForIdle();
    transferQueue->waitForIdle();
    computeQueue->waitForIdle();
}

void RenderManager::drawFrame() {
    try {
        drawFrame_();
    } catch (const std::runtime_error &runtime_error) {
        if (std::equal(std::string(runtime_error.what()).begin(), std::string(runtime_error.what()).end(), "Frame "
                                                                                                           "invalidated!")) {
            swapChain->getPresentFence()->resetState();
            vkQueueSubmit(graphicsQueue->getHandle(), 0, nullptr, swapChain->getPresentFence()->getHandle());
        }
    }
}

void RenderManager::registerFunctionNextFrame(const std::function<void()> &function) {
    frameFunctions.push_back(function);
}

void RenderManager::callFunctions() {
    auto thisFrame = frameFunctions;
    frameFunctions.clear();

    for (const auto &function: thisFrame) {
        function();
    }
}

