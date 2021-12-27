//
// Created by nic on 12/07/2021.
//

#pragma once

#ifndef CUBICAD_BUFFER_H
#define CUBICAD_BUFFER_H

#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include "device.h"
#include "vulkanclass.h"
#include "queues.h"

#include <vector>


class Buffer : public VulkanClass<VkBuffer> {
  private:
    std::shared_ptr<Device> device;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo{};

    VkDeviceSize deviceSize;

  public:
    static std::shared_ptr<Buffer> create(std::shared_ptr<Device> pDevice,
                                          VkDeviceSize size,
                                          VmaMemoryUsage memoryUsage,
                                          VkMemoryPropertyFlags preferredFlags,
                                          VkBufferUsageFlagBits bufferUsage,
                                          std::vector<uint32_t> accessingQueues);

    static std::shared_ptr<Buffer> createHostStagingBuffer(std::shared_ptr<Device> pDevice,
                                                           VkDeviceSize size,
                                                           std::vector<uint32_t> &accessingQueues);

    void transferDataMapped(void* src);

    std::shared_ptr<Device> getDevice() { return device; }

    VmaAllocation getAllocation() { return allocation; }

    [[nodiscard]] VkDeviceSize getSize() const { return deviceSize; }

    void *map();
    void unmap();

    ~Buffer();
};

#endif //CUBICAD_BUFFER_H
