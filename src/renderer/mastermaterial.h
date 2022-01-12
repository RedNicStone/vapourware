//
// Created by nic on 31/12/2021.
//

#pragma once

#ifndef CUBICAD_MASTERMATERIAL_H
#define CUBICAD_MASTERMATERIAL_H

#include "vulkan/graphicspipeline.h"
#include "material.h"
#include "mesh.h"
#include "objectinstance.h"

#include <utility>


class Material;

struct Vertex;

struct PBRMaterialParameters;

class MasterMaterial {
  private:
    std::shared_ptr<Device> device;

    PBRMaterialParameters defaultParameters;

    VkExtent2D extent{};
    std::vector<std::shared_ptr<GraphicsShader>> shaders;
    std::shared_ptr<RenderPass> renderPass;
    std::shared_ptr<GraphicsPipeline> pipeline;
    std::shared_ptr<PipelineLayout> pipelineLayout;

    std::shared_ptr<DescriptorSetLayout> materialSetLayout;
    std::shared_ptr<DescriptorSetLayout> masterMaterialSetLayout;
    std::shared_ptr<DescriptorSet> masterMaterialSet;

  public:
    static std::shared_ptr<Material> create(const std::shared_ptr<Device>& pDevice, const
    std::vector<std::shared_ptr<GraphicsShader>>& shaders, VkExtent2D extent);

    void updateDescriptorSetLayouts(const std::shared_ptr<DescriptorSetLayout>& sceneLayout);

    std::shared_ptr<GraphicsPipeline> getPipeline() { return pipeline; };
    std::shared_ptr<PipelineLayout> getPipelineLayout() { return pipelineLayout; };
    std::shared_ptr<DescriptorSet> getDescriptorSet() { return masterMaterialSet; };
};

void MasterMaterial::updateDescriptorSetLayouts(const std::shared_ptr<DescriptorSetLayout>& sceneLayout) {
    std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorLayouts{ sceneLayout, masterMaterialSetLayout,
                                                                         materialSetLayout };

    pipelineLayout = PipelineLayout::create(device, descriptorLayouts);

    std::vector<VkVertexInputBindingDescription> bindingDescription {
        { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE }
    };
    std::vector<VkVertexInputAttributeDescription> attributeDescription {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        { 1, 1, VK_FORMAT_R32_UINT, offsetof(InstanceData, objectID) },
        { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::float32) * 0 },
        { 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::float32) * 1 },
        { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::float32) * 2 },
        { 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, model) + sizeof(glm::float32) * 3 }
    };
    pipeline = GraphicsPipeline::create(device, pipelineLayout, shaders, renderPass, extent,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
}

#endif //CUBICAD_MASTERMATERIAL_H