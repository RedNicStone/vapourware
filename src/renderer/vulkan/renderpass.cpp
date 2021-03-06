//
// Created by nic on 19/05/2021.
//

#include "renderpass.h"

#include <utility>


uint32_t RenderPass::submitImageAttachment(VkFormat format,
                                           VkSampleCountFlagBits sampleCount,
                                           VkAttachmentLoadOp loadOp,
                                           VkAttachmentStoreOp storeOp,
                                           VkAttachmentLoadOp stencilLoadOp,
                                           VkAttachmentStoreOp stencilStoreOp,
                                           VkImageLayout initialLayout,
                                           VkImageLayout finalLayout) {
    VkAttachmentDescription description{};

    description.flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    description.format = format;
    description.samples = sampleCount;
    description.loadOp = loadOp;
    description.storeOp = storeOp;
    description.stencilLoadOp = stencilLoadOp;
    description.stencilStoreOp = stencilStoreOp;
    description.initialLayout = initialLayout;
    description.finalLayout = finalLayout;

    attachments.push_back(description);
    return static_cast<uint32_t>(attachments.size() - 1);
}

uint32_t RenderPass::submitSwapChainAttachment(const std::shared_ptr<SwapChain> &swapChain, bool clearImage) {
    return submitImageAttachment(swapChain->getFormat(),
                                 VK_SAMPLE_COUNT_1_BIT,
                                 clearImage ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                 VK_ATTACHMENT_STORE_OP_STORE,
                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                 VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

uint32_t RenderPass::submitSubpass(VkPipelineBindPoint bindPoint,
                                   std::vector<VkAttachmentReference> &colorAttachments,
                                   std::vector<VkAttachmentReference> &inputAttachments,
                                   VkAttachmentReference *depthAttachment,
                                   VkPipelineStageFlags stage) {
    VkSubpassDescription description{};
    description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // todo compute not implemented
    description.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    description.pColorAttachments = colorAttachments.data();
    description.inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size());
    description.pInputAttachments = inputAttachments.data();
    description.pDepthStencilAttachment = depthAttachment;
    description.pipelineBindPoint = bindPoint;

    subpasses.push_back(description);
    subpass_stages.push_back(stage);
    return static_cast<uint32_t>(subpasses.size() - 1);
}

void RenderPass::submitDependency(uint32_t src, uint32_t dst, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                  VkPipelineStageFlags srcPipeline, VkPipelineStageFlags dstPipeline) {
    VkSubpassDependency dependency{};
    dependency.srcSubpass = src;
    dependency.dstSubpass = dst;
    dependency.srcStageMask = srcPipeline;
    dependency.srcAccessMask = srcAccess;
    dependency.dstStageMask = dstPipeline;
    dependency.dstAccessMask = dstAccess;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies.push_back(dependency);
}

void RenderPass::build() {
    VkRenderPassCreateInfo passCreateInfo{};
    passCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    passCreateInfo.pAttachments = attachments.data();
    passCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    passCreateInfo.pSubpasses = subpasses.data();
    passCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    passCreateInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device->getHandle(), &passCreateInfo, nullptr, &handle) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

std::shared_ptr<RenderPass> RenderPass::create(std::shared_ptr<Device> pDevice) {
    auto renderPass = std::make_shared<RenderPass>();
    renderPass->device = std::move(pDevice);

    return renderPass;
}

RenderPass::~RenderPass() {
    vkDestroyRenderPass(device->getHandle(), handle, nullptr);
}
