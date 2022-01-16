//
// Created by nic on 09/01/2022.
//

#include "scene.h"


std::shared_ptr<Scene> Scene::create(const std::shared_ptr<Device> &pDevice,
                                     const std::shared_ptr<Queue> &pTransferQueue,
                                     const std::shared_ptr<Queue> &pGraphicsQueue,
                                     size_t reservedInstances,
                                     size_t reservedIndirectCommands) {
    auto scene = std::make_shared<Scene>();
    scene->device = pDevice;

    scene->transferCommandPool = CommandPool::create(pDevice, pTransferQueue, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    std::vector<uint32_t> accessingQueues{ pTransferQueue->getQueueFamilyIndex(),
                                           pGraphicsQueue->getQueueFamilyIndex() };

    auto end = accessingQueues.end();
    for (auto it = accessingQueues.begin(); it != end; ++it) {
        end = std::remove(it + 1, end, *it);
    }
    accessingQueues.erase(end, accessingQueues.end());

    scene->instanceBuffer = DynamicBuffer::create(pDevice,
                                                  accessingQueues,
                                           VMA_MEMORY_USAGE_GPU_ONLY,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    scene->indirectCommandBuffer = DynamicBuffer::create(pDevice,
                                                         accessingQueues,
                                           VMA_MEMORY_USAGE_GPU_ONLY,
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    scene->vertexBuffer = DynamicBuffer::create(pDevice,
                                                accessingQueues,
                                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    scene->indexBuffer = DynamicBuffer::create(pDevice,
                                               accessingQueues,
                                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    scene->descriptorPool = DescriptorPoolManager::create(pDevice);

    scene->sceneBindings = {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS, VK_NULL_HANDLE }
    };
    scene->sceneInfoSetLayout = DescriptorSetLayout::create(pDevice, scene->sceneBindings);
    scene->sceneDescriptorSet = scene->descriptorPool->allocate(scene->sceneInfoSetLayout);

    scene->sceneInfoBuffer = UniformBuffer::create(pDevice, pTransferQueue, sizeof(SceneData));
    scene->sceneDescriptorSet->updateUniformBuffer(scene->sceneInfoBuffer, 0);

    scene->instanceBufferData = scene->instanceBuffer->map();
    scene->indirectCommandBufferData = scene->indirectCommandBuffer->map();

    auto* data = static_cast<SceneData *>(scene->sceneInfoBuffer->getDataHandle());
    data->view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));

    return scene;
}

void Scene::updateUBO(VkExtent2D windowExtend, glm::vec2 mouseDelta) {
    auto* data = static_cast<SceneData *>(sceneInfoBuffer->getDataHandle());
    const glm::vec4 eye = {0, 0, 0, 1};

    glm::vec4 position = {pos, 1};
    glm::vec4 pivot = {pos, 1};

    float deltaAngleX = (360 / (float) windowExtend.width); // a movement from left to right = 2*PI = 360 deg
    float deltaAngleY = (180 / (float) windowExtend.height);  // a movement from top to bottom = PI = 180 deg
    float xAngle = mouseDelta.x * deltaAngleX;
    float yAngle = mouseDelta.y * deltaAngleY;

    std::cout << "Angle :" << xAngle << ", " << yAngle << std::endl;

    pos = glm::vec3(
        glm::rotate(glm::mat4(1.0),
                    xAngle,
                    glm::vec3(glm::transpose(data->view)[0])
        ) * glm::vec4((pos - target), 1)) + target;

    auto n =  pos;
    std::cout << "Pos :" << n.x << ", " << n.y << ", " << n.z << std::endl;

    pos = glm::vec3(
        glm::rotate(glm::mat4(1.0),
                    yAngle,
                    up
        ) * glm::vec4((pos - target), 1)) + target;

    data->view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
    data->proj = glm::perspective(glm::radians(45.0f), (float) windowExtend.width / (float) windowExtend.height, 0.1f, 10.0f);
}

SceneData Scene::getUBO() {
    return *static_cast<SceneData*>(sceneInfoBuffer->getDataHandle());
}

void Scene::transferRenderData() {
    auto currentTime = std::chrono::high_resolution_clock::now();

    auto* data = static_cast<SceneData *>(sceneInfoBuffer->getDataHandle());
    data->frameTime = static_cast<glm::uint32>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(lastFrameTime - currentTime).count());
    data->nFrame++;

    lastFrameTime = currentTime;

    indirectDrawCalls.clear();
    std::vector<InstanceData> instanceData;
    std::vector<VkDrawIndexedIndirectCommand> indirectDrawData;

    std::unordered_map<std::shared_ptr<Meshlet>,
                       std::vector<std::shared_ptr<MeshInstance>>> meshletDraws{};
    for (const auto& instance : instances) {
        for (const auto& meshlet : instance->getMesh()->getMeshlets()) {
            meshletDraws[meshlet].push_back(instance);
        }
    }
    std::unordered_map<std::shared_ptr<Material>,
                       std::vector<std::pair<std::shared_ptr<Meshlet>,
                                             std::vector<std::shared_ptr<MeshInstance>>>>> materialDraws{};
    for (const auto& meshletDraw : meshletDraws) {
        materialDraws[meshletDraw.first->material].push_back(meshletDraw);
    }
    std::unordered_map<std::shared_ptr<MasterMaterial>,
                       std::vector<std::pair<std::shared_ptr<Material>,
                                   std::vector<std::pair<std::shared_ptr<Meshlet>,
                                               std::vector<std::shared_ptr<MeshInstance>>>>>>> masterMaterialDraws{};
    for (const auto& materialDraw : materialDraws) {
        masterMaterialDraws[materialDraw.first->getMasterMaterial()].push_back(materialDraw);
    }
    for (const auto& drawCall : masterMaterialDraws) {
        for (const auto& materialCall : drawCall.second) {
            IndirectDrawCall indirectDrawCall{};
            indirectDrawCall.material = materialCall.first;
            indirectDrawCall.drawCallOffset = static_cast<uint32_t>(indirectDrawData.size());

            for (const auto& meshletCall : materialCall.second) {
                for (const auto& instanceCall : meshletCall.second) {
                    instanceData.push_back(instanceCall->getInstanceData());
                }

                VkDrawIndexedIndirectCommand drawCommand = meshletCall.second.front()->getMesh()->getDrawCommand();
                drawCommand.firstInstance = 0;
                drawCommand.instanceCount = static_cast<uint32_t>(drawCall.second.size());
                indirectDrawData.push_back(drawCommand);
            }

            indirectDrawCall.drawCallLength = static_cast<uint32_t>(indirectDrawData.size())
                - indirectDrawCall.drawCallOffset;
            indirectDrawCalls.push_back(indirectDrawCall);
        }
    }

    instanceBuffer->getBuffer(instanceData.size() * sizeof(InstanceData));
    indirectCommandBuffer->getBuffer(indirectDrawData.size() * sizeof
        (VkDrawIndexedIndirectCommand));

    memcpy(*instanceBufferData, instanceData.data(), instanceData.size() * sizeof(InstanceData));
    memcpy(*indirectCommandBufferData, indirectDrawData.data(), indirectDrawData.size() * sizeof
    (VkDrawIndexedIndirectCommand));
}

void Scene::submitInstance(const std::shared_ptr<MeshInstance>& meshInstance) {
    instances.push_back(meshInstance);
}

void Scene::collectRenderBuffers() {
    std::vector<Vertex> vertexData;
    std::vector<uint32_t> indexData;

    std::unordered_set<std::shared_ptr<Mesh>> meshes;
    for (const auto& instance : instances) {
        meshes.insert(instance->getMesh());
    }
    for (const auto& mesh : meshes) {
        mesh->setOffsets(static_cast<uint32_t>(indexData.size()), static_cast<uint32_t>(vertexData.size()));
        auto& meshlets = mesh->getMeshlets();
        for (const auto& meshlet : meshlets) {
            vertexData.insert(vertexData.end(), meshlet->vertexData.begin(), meshlet->vertexData.end());
            indexData.insert(indexData.end(), meshlet->indexData.begin(), meshlet->indexData.end());
        }
    }

    vertexBuffer->getBuffer(sizeof(Vertex) * vertexData.size())->transferDataStaged(vertexData.data(),
                                                                                 transferCommandPool, sizeof(Vertex) * vertexData.size());
    indexBuffer->getBuffer(sizeof(uint32_t) * indexData.size())->transferDataStaged(indexData.data(),
                                                                                  transferCommandPool, sizeof(uint32_t) * indexData.size());
}

void Scene::bakeMaterials() {
    std::unordered_set<std::shared_ptr<MasterMaterial>> masterMaterials{};
    for (const auto& instance : instances) {
        for (const auto& meshlet : instance->getMesh()->getMeshlets()) {
            masterMaterials.insert(meshlet->material->getMasterMaterial());
        }
    }

    for (const auto& masterMaterial : masterMaterials) {
        masterMaterial->updateDescriptorSetLayouts(sceneInfoSetLayout);
    }
}

void Scene::bakeGraphicsBuffer(const std::shared_ptr<CommandBuffer>& graphicsCommandBuffer) {
    transferRenderData();

    std::vector<uint32_t> offsets{};

    std::vector<std::shared_ptr<Buffer>> vertexBuffers = {vertexBuffer->getBuffer(), instanceBuffer->getBuffer()};
    graphicsCommandBuffer->bindVertexBuffers(vertexBuffers, 0);
    graphicsCommandBuffer->bindIndexBuffer(indexBuffer->getBuffer(), VK_INDEX_TYPE_UINT32);

    std::shared_ptr<MasterMaterial> previousMasterMaterial = nullptr;
    for (const auto& drawCall : indirectDrawCalls) {
        std::shared_ptr<MasterMaterial> currentMasterMaterial = drawCall
            .material->getMasterMaterial();
        if (currentMasterMaterial != previousMasterMaterial) {
            if (previousMasterMaterial != nullptr) {
                graphicsCommandBuffer->endRenderPass();
            }
            previousMasterMaterial = currentMasterMaterial;
            graphicsCommandBuffer->bindPipeline(currentMasterMaterial->getPipeline());

            std::vector<std::shared_ptr<DescriptorSet>> sceneDescriptorSets{ sceneDescriptorSet };
            graphicsCommandBuffer->bindDescriptorSets(sceneDescriptorSets, currentMasterMaterial->getPipelineLayout(),
                                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                      offsets);
            /*std::vector<std::shared_ptr<DescriptorSet>> descriptorSets{ currentMasterMaterial->getDescriptorSet(),
                                                                                      drawCall.material->getDescriptorSet() };
            graphicsCommandBuffer->bindDescriptorSets(descriptorSets, currentMasterMaterial->getPipeline());*/
        } else {
            /*std::vector<std::shared_ptr<DescriptorSet>> descriptorSets{ drawCall.material->getDescriptorSet() };
            graphicsCommandBuffer->bindDescriptorSets(descriptorSets, currentMasterMaterial->getPipeline());*/

            //todo
        }

        graphicsCommandBuffer->drawIndexedIndirect(indirectCommandBuffer->getBuffer(), drawCall.drawCallLength, drawCall
        .drawCallOffset);
    }
}

Scene::~Scene() {
    instanceBuffer->unmap();
    indirectCommandBuffer->unmap();
}
