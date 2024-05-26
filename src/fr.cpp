#include "fr/fr.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <set>
#include <limits>
#include <algorithm>

#include <sstream>

#include <cmath>

#undef max

namespace fr {

  #define VK_REPORT(call)                                            \
  do {                                                               \
    const char *res = string_VkResult(result);                       \
    const char *fmt = "%s failed with error %s!";                    \
    char *buf = (char*)malloc(1024);                                 \
    sprintf(buf, fmt, ""#call, res);                                 \
    throw fr::frVulkanException(buf);                                \
  } while(0)

  #define VK_WRAPPER(call)                                           \
  do {                                                               \
    VkResult result = (call);                                        \
    if (result != VK_SUCCESS) {                                      \
      VK_REPORT(call);                                               \
    }                                                                \
  } while(0)

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frWindow]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frWindow::frWindow(const char *title, int width, int height) 
  {
    if (!glfwInit()) {
      throw fr::frWindowException("Failed to initialize GLFW!");
    }

    if (!glfwVulkanSupported()) {
      throw fr::frWindowException("Vulkan is not supported!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mWindow = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!mWindow) {
      throw fr::frWindowException("Failed to create window!");
    }
  }

  frWindow::frWindow(std::string title, int width, int height):
    frWindow(title.c_str(), width, height)
  {}

  frWindow::~frWindow() {
    cleanup();
  }

  void frWindow::cleanup() {
    glfwDestroyWindow(mWindow);
  }

  std::pair<int, int> frWindow::getSize() {
    int w = 0, h = 0;
    glfwGetWindowSize(mWindow, &w, &h);
    return {w,h};
  }

  void frWindow::addExtensions(frRenderer *renderer) {
    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
      renderer->addExtension(extensions[i]);
    }
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frWindow]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frSwapchain]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frSwapchain::frSwapchain()
  {}

  frSwapchain::~frSwapchain() {
    cleanup();
  }

  void frSwapchain::initialize(frRenderer *renderer, frWindow *window) {
    { // Get swapchain details
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->mPhysicalDevice, renderer->mSurface, &mSupportDetails.capabilities);
      uint32_t formatCount;
      vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->mPhysicalDevice, renderer->mSurface, &formatCount, nullptr);

      if (formatCount != 0) {
        mSupportDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->mPhysicalDevice, renderer->mSurface, &formatCount, mSupportDetails.formats.data());
      }

      uint32_t presentModeCount;
      vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->mPhysicalDevice, renderer->mSurface, &presentModeCount, nullptr);

      if (presentModeCount != 0) {
        mSupportDetails.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->mPhysicalDevice, renderer->mSurface, &presentModeCount, mSupportDetails.presentModes.data());
      }
    }

    { // Choose format
      mFormat = mSupportDetails.formats[0];
      bool found = false;
      for (const auto& format : mDesiredFormats) {
        for (const auto& available : mSupportDetails.formats) {
          if (available.format == format.format && available.colorSpace == format.colorSpace) { mFormat = available; found = true; break; }
        }
        if (found) break;
      }
      renderer->setSurfaceFormat(mFormat.format);
    }

    { // Choose present mode
      mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
      bool found = false;
      for (const auto& presentMode : mDesiredPresentModes) {
        for (const auto& mode : mSupportDetails.presentModes) {
          if (mode == presentMode) { mPresentMode = mode; found = true; break; }
        }  
        if (found) break;
      }
    }

    { // Choose extent
      if (mSupportDetails.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        mExtent = mSupportDetails.capabilities.currentExtent;
      } else {
        int width, height;
        glfwGetFramebufferSize(window->get(), &width, &height);

        mExtent = {
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height)
        };

        mExtent.width = std::clamp(mExtent.width, mSupportDetails.capabilities.minImageExtent.width, mSupportDetails.capabilities.maxImageExtent.width);
        mExtent.height = std::clamp(mExtent.height, mSupportDetails.capabilities.minImageExtent.height, mSupportDetails.capabilities.maxImageExtent.height);
      }
    }

    mImageCount = mSupportDetails.capabilities.minImageCount + 1;
    if (mSupportDetails.capabilities.maxImageCount > 0 && mImageCount > mSupportDetails.capabilities.maxImageCount) {
      mImageCount = mSupportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = renderer->mSurface;
    createInfo.minImageCount = mImageCount;
    createInfo.imageFormat = mFormat.format;
    createInfo.imageColorSpace = mFormat.colorSpace;
    createInfo.imageExtent = mExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;    

    if (renderer->mGraphicsQueueFamily != renderer->mPresentQueueFamily) {
      uint32_t queueFamilyIndices[] = {renderer->mGraphicsQueueFamily, renderer->mPresentQueueFamily};
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;
      createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = mSupportDetails.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = mPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_WRAPPER(vkCreateSwapchainKHR(renderer->mDevice, &createInfo, nullptr, &mSwapchain));

    vkGetSwapchainImagesKHR(renderer->mDevice, mSwapchain, &mImageCount, nullptr);
      mImages.resize(mImageCount);
    vkGetSwapchainImagesKHR(renderer->mDevice, mSwapchain, &mImageCount, mImages.data());

    mDevice = renderer->mDevice;
  }

  void frSwapchain::cleanup() {
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frSwapchain]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frSampler]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frSampler::frSampler()
  {}

  frSampler::~frSampler() {
    cleanup();
  }

  void frSampler::initialize(frRenderer *renderer, frSamplerInfo info) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter        = info.magFilter;
    samplerInfo.minFilter        = info.minFilter;
    samplerInfo.anisotropyEnable = info.anisotropyEnable;
    samplerInfo.borderColor      = info.borderColor;
    samplerInfo.compareEnable    = info.compareEnable;
    samplerInfo.compareOp        = info.compareOp;
    samplerInfo.mipmapMode       = info.mipmapMode;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (info.anisotropyEnable) {
      VkPhysicalDeviceProperties properties{};
      vkGetPhysicalDeviceProperties(renderer->mPhysicalDevice, &properties);
      samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    } else {
      samplerInfo.maxAnisotropy = 1.0f;
    }

    VK_WRAPPER(vkCreateSampler(renderer->mDevice, &samplerInfo, nullptr, &mSampler));
    mDevice = renderer->mDevice;
  }

  void frSampler::cleanup() {
    vkDestroySampler(mDevice, mSampler, nullptr);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frSampler]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frImage]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frImage::frImage()
  {}

  frImage::~frImage() {
    cleanup();
  }

  void frImage::initialize(frRenderer *renderer, frImageInfo info) {
    info.mipLevels = info.generateMipmaps ? static_cast<uint32_t>(std::floor(std::log2(std::max(info.width, info.height)))) + 1 : 1;

    { // Create image
      VkImageCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      createInfo.imageType = VK_IMAGE_TYPE_2D;
      createInfo.extent.width = static_cast<uint32_t>(info.width);
      createInfo.extent.height = static_cast<uint32_t>(info.height);
      createInfo.extent.depth = 1;
      createInfo.mipLevels = info.mipLevels;
      createInfo.arrayLayers = 1;
      createInfo.format = info.format;
      createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      createInfo.usage = info.usage;
      createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.samples = info.samples;
      createInfo.flags = 0;

      VK_WRAPPER(vkCreateImage(renderer->mDevice, &createInfo, nullptr, &mImage));
    }

    if (info.memory) { // Create and bind image memory
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(renderer->mDevice, mImage, &memRequirements);

      VkMemoryAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocInfo.allocationSize = memRequirements.size;
      allocInfo.memoryTypeIndex = renderer->FindMemoryType(memRequirements.memoryTypeBits, info.memoryProperties);

      VK_WRAPPER(vkAllocateMemory(renderer->mDevice, &allocInfo, nullptr, &mImageMemory));

      vkBindImageMemory(renderer->mDevice, mImage, mImageMemory, 0);
    }

    mInfo = info;
    mDevice = renderer->mDevice;

    createView(info);
  }

  void frImage::initialize(frRenderer *renderer, VkImage image, frImageInfo info) {
    mDestroyImage = false;
    mImage = image;

    info.mipLevels = info.generateMipmaps ? static_cast<uint32_t>(std::floor(std::log2(std::max(info.width, info.height)))) + 1 : 1;

    if (info.memory) { // Create and bind image memory
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(renderer->mDevice, mImage, &memRequirements);

      VkMemoryAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocInfo.allocationSize = memRequirements.size;
      allocInfo.memoryTypeIndex = renderer->FindMemoryType(memRequirements.memoryTypeBits, info.memoryProperties);

      VK_WRAPPER(vkAllocateMemory(renderer->mDevice, &allocInfo, nullptr, &mImageMemory));

      vkBindImageMemory(renderer->mDevice, mImage, mImageMemory, 0);
    }

    mInfo = info;
    mDevice = renderer->mDevice;

    createView(info);
  }

  void frImage::cleanup() {
    if (mImageView) vkDestroyImageView(mDevice, mImageView, nullptr);
    if (mImage && mDestroyImage) vkDestroyImage(mDevice, mImage, nullptr);
    if (mImageMemory) vkFreeMemory(mDevice, mImageMemory, nullptr);
  }

  void frImage::transitionLayout(frRenderer *renderer, frCommands *commands, frImageTransitionInfo info) {
    VkCommandBuffer cmdBuf = commands->beginSingleTime();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = info.oldLayout;
    barrier.newLayout = info.newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = mImage;
    barrier.subresourceRange.aspectMask = mInfo.imageAspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mInfo.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = info.srcAccess;
    barrier.dstAccessMask = info.dstAccess;
    
    vkCmdPipelineBarrier(
      cmdBuf,
      info.srcStage, info.dstStage,
      0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    commands->endSingleTime(renderer, cmdBuf);
  }

  void frImage::generateMipmaps(frRenderer *renderer, frCommands *commands) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(renderer->mPhysicalDevice, mInfo.format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
      throw fr::frVulkanException("Texture image format does not support linear blitting!");
    }

    VkCommandBuffer cmdBuf = commands->beginSingleTime();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = mImage;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = mInfo.width;
    int32_t mipHeight = mInfo.height;

    for (uint32_t i = 1; i < mInfo.mipLevels; i++) {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

      vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

      VkImageBlit blit{};
      blit.srcOffsets[0] = {0, 0, 0};
      blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = 1;
      blit.dstOffsets[0] = {0, 0, 0};
      blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel = i;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = 1;

      vkCmdBlitImage(cmdBuf,
        mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR);

      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mInfo.mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuf,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
      0, nullptr,
      0, nullptr,
      1, &barrier);

    commands->endSingleTime(renderer, cmdBuf);
  }

  void frImage::copyFromBuffer(frRenderer *renderer, frCommands *commands, frBuffer *buffer, VkDeviceSize size) {
    VkCommandBuffer cmdBuf = commands->beginSingleTime();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = mInfo.imageAspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
      static_cast<uint32_t>(mInfo.width), 
      static_cast<uint32_t>(mInfo.height), 
      1
    };

    vkCmdCopyBufferToImage(
      cmdBuf,
      buffer->get(),
      mImage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
    );

    commands->endSingleTime(renderer, cmdBuf);
  }

  void frImage::setName(frRenderer *renderer, const char *imageName) {
    { // Set name for mImage
      VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
      objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      objectNameInfo.pNext = nullptr;
      objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
      objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mImage);
      objectNameInfo.pObjectName = imageName;

      VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
    }

    if (mImageMemory) { // Set name for mImageMemory
      VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
      objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      objectNameInfo.pNext = nullptr;
      objectNameInfo.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
      objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mImageMemory);
      objectNameInfo.pObjectName = imageName;

      VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
    }

    { // Set name for mImageView
      VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
      objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      objectNameInfo.pNext = nullptr;
      objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
      objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mImageView);
      objectNameInfo.pObjectName = imageName;

      VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
    }
  }

  void frImage::createView(frImageInfo info) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = mImage;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = info.format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = info.imageAspect;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = info.mipLevels;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VK_WRAPPER(vkCreateImageView(mDevice, &createInfo, nullptr, &mImageView));
  }

  bool frImage::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
  }  
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frImage]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frRenderPass]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frRenderPass::frRenderPass() 
  {}

  frRenderPass::~frRenderPass() {
    cleanup();
  }

  void frRenderPass::initialize(frRenderer *renderer) {
    { // Create RenderPass
      VkRenderPassCreateInfo createInfo{};
      createInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      createInfo.pNext           = VK_NULL_HANDLE;
      createInfo.flags           = 0;
      createInfo.attachmentCount = static_cast<uint32_t>(mAttachments.size());
      createInfo.pAttachments    = mAttachments.data();
      createInfo.subpassCount    = static_cast<uint32_t>(mSubpasses.size());
      createInfo.pSubpasses      = mSubpasses.data();
      createInfo.dependencyCount = static_cast<uint32_t>(mDependencies.size());
      createInfo.pDependencies   = mDependencies.data();

      VK_WRAPPER(vkCreateRenderPass(renderer->mDevice, &createInfo, nullptr, &mRenderPass));
    }

    mDevice = renderer->mDevice;
  }

  void frRenderPass::cleanup() {
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
  }

  void frRenderPass::begin(VkCommandBuffer cmdBuf, VkExtent2D extent, frFramebuffer *fb, std::vector<VkClearValue> clearValues) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mRenderPass;
    renderPassInfo.framebuffer = fb->mFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void frRenderPass::end(VkCommandBuffer cmdBuf) {
    vkCmdEndRenderPass(cmdBuf);
  }

  void frRenderPass::setName(frRenderer *renderer, const char *name) {
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
    objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.pNext = nullptr;
    objectNameInfo.objectType = VK_OBJECT_TYPE_RENDER_PASS;
    objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mRenderPass);
    objectNameInfo.pObjectName = name;

    VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frRenderPass]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frFramebuffer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frFramebuffer::frFramebuffer()
  {}

  frFramebuffer::~frFramebuffer() {
    cleanup();
  }

  void frFramebuffer::initialize(frRenderer *renderer, int width, int height, frRenderPass *renderPass, std::vector<frImage *> images) {
    { // Create Framebuffer
      VkFramebufferCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      createInfo.pNext = VK_NULL_HANDLE;
      createInfo.flags = 0;
      createInfo.renderPass = renderPass->mRenderPass;
      createInfo.attachmentCount = images.size();
      std::vector<VkImageView> attachments(images.size());
      for (size_t i = 0; i < images.size(); ++i) attachments[i] = images[i]->mImageView;
      createInfo.pAttachments = attachments.data();
      createInfo.width = width;
      createInfo.height = height;
      createInfo.layers = 1;

      VK_WRAPPER(vkCreateFramebuffer(renderer->mDevice, &createInfo, nullptr, &mFramebuffer));
    }

    mDevice = renderer->mDevice;
  }

  void frFramebuffer::cleanup() {
    vkDestroyFramebuffer(mDevice, mFramebuffer, nullptr); 
  }

  void frFramebuffer::setName(frRenderer *renderer, const char *name) {
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
    objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.pNext = nullptr;
    objectNameInfo.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
    objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mFramebuffer);
    objectNameInfo.pObjectName = name;

    VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frFramebuffer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frShader]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frShader::frShader()
  {}

  frShader::~frShader() {
    cleanup();
  }

  bool frShader::initialize(frRenderer *renderer, const char *filepath, VkShaderStageFlagBits stage, const char *entry) {
    std::vector<char> code{};
    { // Read file
      FILE *fd = fopen(filepath, "rb");
      if (!fd) return false;
      fseek(fd, 0, SEEK_END);
      long size = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      
      code.resize(size);
      fread(code.data(), size, 1, fd);
      fclose(fd);
    }

    initialize(renderer, code, stage, entry);

    return true;
  }

  void frShader::initialize(frRenderer *renderer, std::vector<char> code, VkShaderStageFlagBits stage, const char *entry) {
    VkShaderModuleCreateInfo createInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      VK_NULL_HANDLE,
      0,
      code.size(),
      reinterpret_cast<const uint32_t*>(code.data())
    };

    VK_WRAPPER(vkCreateShaderModule(renderer->mDevice, &createInfo, nullptr, &mModule));

    mStageInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VK_NULL_HANDLE, 0,
      stage, mModule, entry,
      VK_NULL_HANDLE,
    };

    mDevice = renderer->mDevice;
  }

  void frShader::cleanup() {
    vkDestroyShaderModule(mDevice, mModule, nullptr);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frShader]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frDescriptorLayout]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frDescriptorLayout::frDescriptorLayout() 
  {}

  frDescriptorLayout::~frDescriptorLayout() {
    cleanup();
  }

  void frDescriptorLayout::initialize(frRenderer *renderer) {
    VkDescriptorSetLayoutCreateInfo createInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, VK_NULL_HANDLE, 0,
      static_cast<uint32_t>(mBindings.size()), mBindings.data()
    };

    VK_WRAPPER(vkCreateDescriptorSetLayout(renderer->mDevice, &createInfo, nullptr, &mLayout));

    mDevice = renderer->mDevice;
  }

  void frDescriptorLayout::cleanup() {
    vkDestroyDescriptorSetLayout(mDevice, mLayout, nullptr);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frDescriptorLayout]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frDescriptor]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frDescriptor::frDescriptor(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set):
    mDevice(device), mPool(pool), mSet(set) {}

  frDescriptor::~frDescriptor() {

  }
  
  void frDescriptor::update(frDescriptorWriteInfo writeInfo) {
    VkWriteDescriptorSet write = {
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, VK_NULL_HANDLE,
      mSet, writeInfo.binding, writeInfo.dstArrayElement, writeInfo.descriptorCount,
      writeInfo.descriptorType, writeInfo.imageInfo, writeInfo.bufferInfo, writeInfo.texelBufferView
    };

    vkUpdateDescriptorSets(mDevice, 1, &write, 0, VK_NULL_HANDLE);
  }

  void frDescriptor::cleanup() {
    vkFreeDescriptorSets(mDevice, mPool, 1, &mSet);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frDescriptor]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frDescriptors]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frDescriptors::frDescriptors()
  {}

  frDescriptors::~frDescriptors() {
    cleanup();
  }

  void frDescriptors::initialize(frRenderer *renderer, std::vector<VkDescriptorPoolSize> poolSizes) {
    uint32_t maxSets = 0;
    for (auto size : poolSizes) maxSets += size.descriptorCount;

    VkDescriptorPoolCreateInfo createInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, VK_NULL_HANDLE, 0,
      maxSets, static_cast<uint32_t>(poolSizes.size()), poolSizes.data()
    };

    VK_WRAPPER(vkCreateDescriptorPool(renderer->mDevice, &createInfo, nullptr, &mPool));

    mDevice = renderer->mDevice;
  }

  void frDescriptors::cleanup() {
    vkDestroyDescriptorPool(mDevice, mPool, nullptr);
  }

  std::vector<frDescriptor*> frDescriptors::allocate(uint32_t count, frDescriptorLayout *layout) {
    std::vector<VkDescriptorSetLayout> layouts(count, layout->mLayout);

    VkDescriptorSetAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, VK_NULL_HANDLE,
      mPool, count, layouts.data()
    };

    std::vector<VkDescriptorSet> sets(count);

    VK_WRAPPER(vkAllocateDescriptorSets(mDevice, &allocInfo, sets.data()));

    std::vector<frDescriptor*> descs{};
    for (auto set : sets) descs.push_back(new frDescriptor(mDevice, mPool, set));
    return descs;
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frDescriptors]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frPipeline]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frPipeline::frPipeline()
  {}

  frPipeline::~frPipeline() {
    cleanup();
  }

  void frPipeline::initialize(frRenderer *renderer, frRenderPass *renderPass) {
    std::vector<VkPipelineShaderStageCreateInfo> stages{};
    for (auto shader : mShaders) stages.push_back(shader);

    { // Create PipelineLayout
      std::vector<VkDescriptorSetLayout> layouts = {};
      for (auto layout : mDescLayouts) layouts.push_back(layout->mLayout);

      VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, VK_NULL_HANDLE, 0,
        static_cast<uint32_t>(layouts.size()), layouts.data(),
        static_cast<uint32_t>(0), nullptr
      };

      VK_WRAPPER(vkCreatePipelineLayout(renderer->mDevice, &createInfo, nullptr, &mLayout));
    }

    { // Create da Pipeline
      VkGraphicsPipelineCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, VK_NULL_HANDLE, 0,
        static_cast<uint32_t>(stages.size()), stages.data(),
        mVertexInputState, mInputAssemblyState, mTessellationState, mViewportState, mRasterizationState, 
        mMultisampleInfo, mDepthStencilState, mColorBlendState, mDynamicState, 
        mLayout, renderPass->mRenderPass, 0,
        VK_NULL_HANDLE, 0,
      };

      VK_WRAPPER(vkCreateGraphicsPipelines(renderer->mDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &mPipeline));
    }

    if (mVertexInputState) delete mVertexInputState;
    if (mInputAssemblyState) delete mInputAssemblyState;
    if (mTessellationState) delete mTessellationState;
    if (mViewportState) delete mViewportState;
    if (mRasterizationState) delete mRasterizationState;
    if (mMultisampleInfo) delete mMultisampleInfo;
    if (mDepthStencilState) delete mDepthStencilState;
    if (mColorBlendState) delete mColorBlendState;
    if (mDynamicState) delete mDynamicState;

    mDevice = renderer->mDevice;
  }

  void frPipeline::cleanup() {
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mLayout, nullptr);
  }

  void frPipeline::bind(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint) {
    vkCmdBindPipeline(cmdBuf, bindPoint, mPipeline);
  }

  void frPipeline::bindDescriptor(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint, uint32_t firstSet, frDescriptor *descriptor) {
    VkDescriptorSet set = descriptor->mSet;
    vkCmdBindDescriptorSets(cmdBuf, bindPoint, mLayout, firstSet, 1, &set, 0, VK_NULL_HANDLE);
  }

  void frPipeline::setName(frRenderer *renderer, const char *name) {
    { // Set name for mLayout
      VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
      objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      objectNameInfo.pNext = nullptr;
      objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
      objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mLayout);
      objectNameInfo.pObjectName = name;

      VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
    }

    { // Set name for mPipeline
      VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
      objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      objectNameInfo.pNext = nullptr;
      objectNameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
      objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(mPipeline);
      objectNameInfo.pObjectName = name;

      VK_WRAPPER(renderer->getSetDebugUtilsObjectNameFunc()(mDevice, &objectNameInfo));
    }
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frPipeline]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frCommands]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frCommands::frCommands()
  {}

  frCommands::~frCommands() {
    cleanup();
  }

  void frCommands::initialize(frRenderer *renderer) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = renderer->mGraphicsQueueFamily;
    VK_WRAPPER(vkCreateCommandPool(renderer->mDevice, &poolInfo, nullptr, &mPool));
    
    mDevice = renderer->mDevice;
  }

  void frCommands::cleanup() {
    vkDestroyCommandPool(mDevice, mPool, nullptr);
  }

  VkCommandBuffer *frCommands::allocateBuffers(VkCommandBufferLevel level, uint32_t count) {
    VkCommandBuffer *buffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * count);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = count;
    VK_WRAPPER(vkAllocateCommandBuffers(mDevice, &allocInfo, buffers));

    return buffers;
  }

  VkCommandBuffer frCommands::beginSingleTime() {
    VkCommandBuffer cmdBuf = allocateBuffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY)[0];
    frCommands::begin(cmdBuf, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return cmdBuf;
  }
  
  void frCommands::endSingleTime(frRenderer *renderer, VkCommandBuffer cmdBuf) {
    frCommands::end(cmdBuf);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    vkQueueSubmit(renderer->mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderer->mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mPool, 1, &cmdBuf);
  }

  void frCommands::begin(VkCommandBuffer cmdBuf, VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;
    VK_WRAPPER(vkBeginCommandBuffer(cmdBuf, &beginInfo));
  }

  void frCommands::end(VkCommandBuffer cmdBuf) {
    VK_WRAPPER(vkEndCommandBuffer(cmdBuf));
  }

  void frCommands::submit(frRenderer *renderer, VkCommandBuffer cmdBuf, frSynchronization *sync) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkFence fence = VK_NULL_HANDLE;
    if (sync) {
      fence = sync->mInFlightFence;

      VkSemaphore waitSemaphores[] = {sync->mImageAvailableSemaphore};
      VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = waitSemaphores;
      submitInfo.pWaitDstStageMask = waitStages;

      VkSemaphore signalSemaphores[] = {sync->mRenderFinishedSemaphore};
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = signalSemaphores;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    VK_WRAPPER(vkQueueSubmit(renderer->mGraphicsQueue, 1, &submitInfo, fence));
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frCommands]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frSynchronization]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frSynchronization::frSynchronization()
  {}

  frSynchronization::~frSynchronization() {
    cleanup();
  }

  void frSynchronization::initialize(frRenderer *renderer) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_WRAPPER(vkCreateSemaphore(renderer->mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore));
    VK_WRAPPER(vkCreateSemaphore(renderer->mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphore));
    VK_WRAPPER(vkCreateFence(renderer->mDevice, &fenceInfo, nullptr, &mInFlightFence));

    mDevice = renderer->mDevice;
  }

  void frSynchronization::cleanup() {
    vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mRenderFinishedSemaphore, nullptr);
    vkDestroyFence(mDevice,     mInFlightFence, nullptr);
  }

  void frSynchronization::wait() {
    vkWaitForFences(mDevice, 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
  }
   
  void frSynchronization::reset() {
    vkResetFences(mDevice, 1, &mInFlightFence);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frSynchronization]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frBuffer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frBuffer::frBuffer()
  {}

  frBuffer::~frBuffer() {
    cleanup();
  }

  void frBuffer::copyData(VkDeviceSize offset, VkDeviceSize size, void *data) {
    void *bufData;
    vkMapMemory(mDevice, mMemory, offset, size, 0, &bufData);
      memcpy(bufData, data, size);
    vkUnmapMemory(mDevice, mMemory);
  }

  void frBuffer::copyFromBuffer(frRenderer *renderer, frCommands *commands, frBuffer *buffer, VkDeviceSize size) {
    VkCommandBuffer cmdBuf = commands->beginSingleTime();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuf, buffer->mBuffer, mBuffer, 1, &copyRegion);

    commands->endSingleTime(renderer, cmdBuf);
  }

  void frBuffer::initialize(frRenderer *renderer, frBufferInfo info, bool bindMemory) {
    renderer->CreateBuffer(info.size, info.usage, info.properties, mBuffer, bindMemory ? &mMemory : VK_NULL_HANDLE); // Create mBuffer and if (if bindMemory == true) { bind mMemory }

    mDevice = renderer->mDevice;
  }

  void frBuffer::cleanup() {
    vkDestroyBuffer(mDevice, mBuffer, VK_NULL_HANDLE);
    vkFreeMemory(mDevice, mMemory, VK_NULL_HANDLE);
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frBuffer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[+frRenderer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  frRenderer::frRenderer() 
  {}

  frRenderer::~frRenderer() {
    cleanup();
  }

  void frRenderer::initialize(frWindow *window, VkPhysicalDeviceFeatures *deviceFeatures) {
    { // Instance
      VkApplicationInfo appInfo{};
      appInfo.pApplicationName = mApplicationName;
      appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
      appInfo.pEngineName = "FissionRender";
      appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
      appInfo.apiVersion = VK_API_VERSION_1_1;

      VkInstanceCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      createInfo.pApplicationInfo = &appInfo;
      createInfo.enabledLayerCount = static_cast<uint32_t>(mLayers.size());
      createInfo.ppEnabledLayerNames = mLayers.data();
      createInfo.enabledExtensionCount = static_cast<uint32_t>(mExtensions.size());
      createInfo.ppEnabledExtensionNames = mExtensions.data();

      uint32_t extensionCount = 0;
      vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> extensions(extensionCount);
      vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

      std::vector<const char *> requiredExtensions = mExtensions;
      for (const auto& extension : extensions) {
        for (auto ext: requiredExtensions) {
          if (strcmp(extension.extensionName, ext) == 0) {
            requiredExtensions.erase(std::find(requiredExtensions.begin(), requiredExtensions.end(), ext));
          }
        }
      }

      if (requiredExtensions.size() > 0) {
        fprintf(stderr, "Unsupported extensions:\n");
        for (auto ext: requiredExtensions) fprintf(stderr, "  - %s\n", ext);
      }

      VK_WRAPPER(vkCreateInstance(&createInfo, nullptr, &mInstance));
    }

    {
      VK_WRAPPER(glfwCreateWindowSurface(mInstance, window->get(), nullptr, &mSurface));
    }

    { // Physical Device
      int32_t best = 0;

      uint32_t count = 0;
      vkEnumeratePhysicalDevices(mInstance, &count, NULL);
      std::vector<VkPhysicalDevice> devices(count);
      VK_WRAPPER(vkEnumeratePhysicalDevices(mInstance, &count, devices.data()));

      for (auto device : devices) {
        int32_t score = 0;
        
        { // Check extensions supported
          uint32_t extensionCount;
          vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
          std::vector<VkExtensionProperties> availableExtensions(extensionCount);
          vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

          std::set<std::string> requiredExtensions(mDeviceExtensions.begin(), mDeviceExtensions.end());

          for (const auto& extension : availableExtensions) requiredExtensions.erase(extension.extensionName);

          if (!requiredExtensions.empty()) {
            score = -1;
          }
        }
 
        if (score == 0) { // Score the device
          VkPhysicalDeviceProperties properties{};
          vkGetPhysicalDeviceProperties(device, &properties);
          VkPhysicalDeviceFeatures features{};
          vkGetPhysicalDeviceFeatures(device, &features);

          switch (properties.deviceType) {
          case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: {
            score += 50;
          } break;
          case VK_PHYSICAL_DEVICE_TYPE_CPU: {
            score += 75;
          } break;
          case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: {
            score += 100;
          } break;
          case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: {
            score += 500;
          } break;
          default: break;
          }
          score += GetMaxUsableSampleCount(properties) * 100;

          score += properties.limits.maxImageDimension2D;
          // if (features.geometryShader) score += 150;
        }

        if (score > best) {
          best = score;
          mPhysicalDevice = device;
        }
      }

      if (!mPhysicalDevice) {
        throw fr::frVulkanException("Failed to pick physical device!");
      }

      { // Queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto &queueFamily : queueFamilies) {
          if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            mGraphicsQueueFamily = i;
            mGraphicsQueueSet = true;
          }

          VkBool32 presentSupport = false;
          vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &presentSupport);
          if (presentSupport) {
            mPresentQueueFamily = i;
            mPresentQueueSet = true;
          }

          if (mGraphicsQueueSet && mPresentQueueSet) break;

          i++;
        }

        if (!mGraphicsQueueSet || !mPresentQueueSet) {
          throw fr::frVulkanException("Failed to find graphics and/or present queue family!");
        }
      }
    }

    { // Device
      std::vector<VkDeviceQueueCreateInfo> queueInfos = {};
      std::set<uint32_t> uniqueQueueFamilies = {mGraphicsQueueFamily, mPresentQueueFamily};

      float priority = 1.0f;
      for (uint32_t family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueInfos.push_back(queueInfo);
      }

      VkDeviceCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
      createInfo.pQueueCreateInfos = queueInfos.data();
      createInfo.enabledLayerCount = static_cast<uint32_t>(mDeviceLayers.size());
      createInfo.ppEnabledLayerNames = mDeviceLayers.data();
      createInfo.enabledExtensionCount = static_cast<uint32_t>(mDeviceExtensions.size());
      createInfo.ppEnabledExtensionNames = mDeviceExtensions.data();
      createInfo.pEnabledFeatures = deviceFeatures;

      VK_WRAPPER(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));

      vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
      vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);
    }
  }

  void frRenderer::cleanup() {
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroyInstance(mInstance, nullptr);
  }

  uint32_t frRenderer::acquireNextImage(frSwapchain *swapchain, frSynchronization *sync) {
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(mDevice, swapchain->mSwapchain, UINT64_MAX, sync->mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      throw fr::frSwapchainResizeException();
      return imageIndex;
    }
    
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
      return imageIndex;
    } else {
      VK_REPORT(vkAcquireNextImageKHR(mDevice, swapchain->mSwapchain, UINT64_MAX, sync->mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));
    }
    
    return UINT32_MAX;
  }

  void frRenderer::present(frSwapchain *swapchain, frSynchronization *sync, uint32_t *imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &sync->mRenderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain->mSwapchain;
    presentInfo.pImageIndices = imageIndex;

    VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
    if (result == VK_SUCCESS) {
      return;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      throw fr::frSwapchainResizeException();
    } else {
      VK_REPORT(vkQueuePresentKHR(mPresentQueue, &presentInfo));
    }
  }

  void frRenderer::waitIdle() {
    vkDeviceWaitIdle(mDevice);
  }

  // - Utilities:
  uint32_t frRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }

    throw fr::frVulkanException("Failed to find suitable memory type!");

    return UINT32_MAX;
  }

  void frRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlagBits usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory *bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_WRAPPER(vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer));

    if (!bufferMemory) return;
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    VK_WRAPPER(vkAllocateMemory(mDevice, &allocInfo, nullptr, bufferMemory));

    vkBindBufferMemory(mDevice, buffer, *bufferMemory, 0);
  }
 
  VkSampleCountFlagBits frRenderer::GetMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
  }
 
  VkSampleCountFlagBits frRenderer::GetMaxUsableSampleCount(VkPhysicalDeviceProperties properties) {
    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
  }

  VkFormat frRenderer::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        return format;
      else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        return format;
    }
    return VK_FORMAT_UNDEFINED;
  }
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-[-frRenderer]-=-=-=-=-=-=-=-=-=-=-=-=-=-=

}