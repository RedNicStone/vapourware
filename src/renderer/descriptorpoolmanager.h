//
// Created by nic on 09/01/2022.
//

#pragma once

#ifndef CUBICAD_DESCRIPTORPOOLMANAGER_H
#define CUBICAD_DESCRIPTORPOOLMANAGER_H

#include "vulkan/descriptorpool.h"
#include "vulkan/descriptorset.h"


/// Descriptor pool manager that eases in descriptor set creation
class DescriptorPoolManager {
  public:
    /// Struct storing pool sizes for each descriptor type
    struct PoolSizes {
        std::vector<std::pair<VkDescriptorType, float>>
            sizes =
            {{VK_DESCRIPTOR_TYPE_SAMPLER, .5f}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .5f},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .5f}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .5f},
             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .5f}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .5f},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8.f}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4.f},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .5f}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .5f},
             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
    };

  private:
    std::shared_ptr<Device> device;

    std::shared_ptr<DescriptorPool> currentPool{};
    PoolSizes descriptorSizes;
    std::vector<std::shared_ptr<DescriptorPool>> usedPools;
    std::vector<std::shared_ptr<DescriptorPool>> freePools;

    std::shared_ptr<DescriptorPool> grabPool();

  public:
    /// Create a new descriptor pool manager
    /// \param pDevice Device to create this object on
    /// \return Valid handle to the descriptor pool manager
    static std::shared_ptr<DescriptorPoolManager> create(std::shared_ptr<Device> pDevice);

    /// Clear the pool and invalidate all created descriptor set handles
    void resetPools();
    /// Allocate a new descriptor set
    /// \param layout Layout of the new set
    /// \return Handle the the new descriptor set
    std::shared_ptr<DescriptorSet> allocate(std::shared_ptr<DescriptorSetLayout> layout);

    std::shared_ptr<Device> getDevice() { return device; };
};

#endif //CUBICAD_DESCRIPTORPOOLMANAGER_H
