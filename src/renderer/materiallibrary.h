//
// Created by nic on 02/01/2022.
//

#pragma once

#ifndef CUBICAD_MATERIALLIBRARY_H
#define CUBICAD_MATERIALLIBRARY_H

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vector>
#include <glm/glm.hpp>

#include "mastermaterial.h"
#include "material.h"
#include "descriptorpoolmanager.h"
#include "dynamicbuffer.h"
#include "vulkan/sampler.h"


class MasterMaterial;

class Material;

class Texture;

class TextureLibrary;

class MaterialLibrary {
  private:
    std::shared_ptr<Device> device;
    std::shared_ptr<CommandPool> transferPool;

    size_t alignment;
    uint32_t bufferOODSize{};  // is the material buffer out of date?
    uint32_t bufferSize;  // how many materials are registered
    std::shared_ptr<DynamicBuffer> materialBuffer;  // the material buffer
    std::shared_ptr<Buffer> prevBuffer{};

    std::shared_ptr<DescriptorPoolManager> descriptorPool;
    std::shared_ptr<TextureLibrary> textureLibrary;

    std::map<std::shared_ptr<MasterMaterial>, std::vector<std::shared_ptr<Material>>> materials;

  public:
    static std::shared_ptr<MaterialLibrary> create(const std::shared_ptr<Device> &pDevice,
                                                   const std::shared_ptr<CommandPool> &pTransferPool,
                                                   const std::shared_ptr<DescriptorPoolManager> &pDescriptorPool,
                                                   const std::shared_ptr<TextureLibrary>& textureLibrary,
                                                   const std::vector<std::shared_ptr<Queue>> &accessingQueues);

    std::shared_ptr<Material> registerShader(const std::shared_ptr<MasterMaterial> &masterMaterial,
                                             void *parameters,
                                             std::vector<std::shared_ptr<Texture>> textures = {},
                                             const std::string& name = "");

    void pushParameters();
};

#endif //CUBICAD_MATERIALLIBRARY_H
