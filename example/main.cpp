#include <fr/fr.hpp>

#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Vertex {
  glm::vec3 position;
  glm::vec2 uv;

  static VkVertexInputBindingDescription *getBindingDescription() {
    VkVertexInputBindingDescription *bindingDescription = new VkVertexInputBindingDescription();
    bindingDescription->binding = 0;
    bindingDescription->stride = sizeof(Vertex);
    bindingDescription->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, uv);

    return attributeDescriptions;
  }

  bool operator==(const Vertex& other) const {
    return position == other.position && uv == other.uv;
  }
};

struct UBO {
  glm::mat4 proj;
  glm::mat4 view;
  glm::mat4 model;
};

using namespace fr;

frRenderer   *renderer = nullptr;
frSwapchain  *swapchain = nullptr;
frRenderPass *renderPass = nullptr;
frPipeline   *pipeline = nullptr;

VkFormat depthFormat = VK_FORMAT_UNDEFINED;

uint32_t                    swapchainImageCount = 0;
std::vector<frImage*>       swapchainImages{};
std::vector<frImage*>       offscreenImages{};
std::vector<frImage*>       depthImages{};
std::vector<frFramebuffer*> swapchainFramebuffers{};

frCommands         *commands = nullptr;
frDescriptors      *descriptors = nullptr;

frDescriptorLayout        *uboLayout = nullptr;
std::vector<frBuffer*>     uboBuffers{};
std::vector<frDescriptor*> ubos{};

frDescriptorLayout *textureLayout = nullptr;
frImage            *textureImage = nullptr;
frSampler          *textureSampler = nullptr;
frDescriptor       *texture = nullptr;

frBuffer *squareVBuf = nullptr;
frBuffer *squareIBuf = nullptr;

std::vector<Vertex> cubeVertices = {
  {{-0.5f,-0.5f,-0.5f}, {0.0f, 1.0f}},  // -X side
  {{-0.5f,-0.5f, 0.5f}, {1.0f, 1.0f}},
  {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
  {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
  {{-0.5f, 0.5f,-0.5f}, {0.0f, 0.0f}},
  {{-0.5f,-0.5f,-0.5f}, {0.0f, 1.0f}},
  {{-0.5f,-0.5f,-0.5f}, {1.0f, 1.0f}}, // -Z side
  {{ 0.5f, 0.5f,-0.5f}, {0.0f, 0.0f}},
  {{ 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f}},
  {{-0.5f,-0.5f,-0.5f}, {1.0f, 1.0f}},
  {{-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}},
  {{ 0.5f, 0.5f,-0.5f}, {0.0f, 0.0f}},
  {{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}},  // -Y side
  {{ 0.5f,-0.5f,-0.5f}, {1.0f, 1.0f}},
  {{ 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}},
  {{ 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{-0.5f,-0.5f, 0.5f}, {0.0f, 0.0f}},
  {{-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}},  // +Y side
  {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
  {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
  {{-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}},
  {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
  {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}},
  {{ 0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}},  // +X side
  {{ 0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
  {{ 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{ 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{ 0.5f,-0.5f,-0.5f}, {1.0f, 1.0f}},
  {{ 0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}},
  {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},  // +Z side
  {{-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{ 0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
  {{-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f}},
  {{ 0.5f,-0.5f, 0.5f}, {1.0f, 1.0f}},
  {{ 0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}}
};

std::vector<uint32_t> cubeIndices = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
};

glm::mat4 projection;

VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

bool recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex);
void createSwapchainFirst();
void createSwapchainLast();
void cleanupSwapchain();
void recreateSwapchain();
void updateUbos();

frWindow *window = nullptr;

const char *const textureFilePath = "./assets/textures/prototype.png";

uint32_t frame = 0;

int main(void) {
  try {
    window = new frWindow("Example", 800, 800);
  } catch(fr::frWindowException &ex) {
    fprintf(stderr, "[Window]: %s\n", ex.what());
    return 1;
  }

  renderer = new frRenderer();
  renderer->setApplicationName("fr example");
  renderer->addLayer("VK_LAYER_KHRONOS_validation");
  renderer->addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  window->addExtensions(renderer);

  std::vector<frSynchronization *> synchronizations;
  commands = new frCommands();

  try {
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};
    physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

    renderer->initialize(window, &physicalDeviceFeatures);

    createSwapchainFirst();

    sampleCount = renderer->GetMaxUsableSampleCount();

    depthFormat = renderer->FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    renderPass = new frRenderPass();
    { // Initialize renderPass
      renderPass->addAttachment(VkAttachmentDescription{
        0, swapchain->format(), sampleCount,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      });

      renderPass->addAttachment(VkAttachmentDescription{
        0, depthFormat, sampleCount,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      });
      
      renderPass->addAttachment(VkAttachmentDescription{
        0, swapchain->format(), VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      });

      VkAttachmentReference colorAttachmentRef{};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkAttachmentReference depthAttachmentRef{};
      depthAttachmentRef.attachment = 1;
      depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      VkAttachmentReference colorAttachmentResolveRef{};
      colorAttachmentResolveRef.attachment = 2;
      colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      
      VkSubpassDescription subpass{};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;
      subpass.pDepthStencilAttachment = &depthAttachmentRef;
      subpass.pResolveAttachments = &colorAttachmentResolveRef;

      VkSubpassDependency dependency{};
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      dependency.srcAccessMask = 0;
      dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      renderPass->addSubpass(subpass);
      renderPass->addDependency(dependency);
      renderPass->initialize(renderer);
      renderPass->setName(renderer, "SwapchainRenderPass");
    }

    createSwapchainLast();

    descriptors = new frDescriptors();
    descriptors->initialize(renderer, {
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, swapchain->imageCount() },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    });
    
    uboLayout = new frDescriptorLayout();
    uboLayout->addBinding(VkDescriptorSetLayoutBinding{
      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_NULL_HANDLE
    });
    uboLayout->initialize(renderer);

    textureLayout = new frDescriptorLayout();
    textureLayout->addBinding(VkDescriptorSetLayoutBinding{
      0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT, 
      VK_NULL_HANDLE
    });
    textureLayout->initialize(renderer);

    pipeline = new frPipeline();
    { // Initialize da pipeline
      frShader *vertexShader = new frShader();
      frShader *fragmentShader = new frShader();
      { // Vertex shader
        vertexShader->initialize(renderer, "assets/shaders/vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
        pipeline->addShader(vertexShader);
      }
      { // Fragment shader
        fragmentShader->initialize(renderer, "assets/shaders/fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipeline->addShader(fragmentShader);
      }

      pipeline->setVertexInputState<Vertex>();

      pipeline->setMultisampleInfo(VkPipelineMultisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        sampleCount, VK_FALSE, 0.0f, VK_NULL_HANDLE,
        VK_FALSE, VK_FALSE
      });

      pipeline->setInputAssemblyState(VkPipelineInputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE
      });

      VkPipelineColorBlendAttachmentState colorBlendAttachment{};
      colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      colorBlendAttachment.blendEnable = VK_FALSE;

      pipeline->setColorBlendState(VkPipelineColorBlendStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        VK_FALSE,
        VK_LOGIC_OP_COPY,
        1,
        &colorBlendAttachment,
        {0.0f, 0.0f, 0.0f, 0.0f}
      });

      pipeline->setRasterizationState(VkPipelineRasterizationStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f
      });

      std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

      pipeline->setDynamicState(VkPipelineDynamicStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data()
      });

      pipeline->setViewportState(VkPipelineViewportStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        1, nullptr, 1, nullptr
      });

      pipeline->setDepthStencilState(VkPipelineDepthStencilStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS,
        VK_FALSE, VK_FALSE,
        {}, {},
        1.0f, 1.0f
      });

      pipeline->addDescriptor(uboLayout);
      pipeline->addDescriptor(textureLayout);

      pipeline->initialize(renderer, renderPass);
      
      delete vertexShader;
      delete fragmentShader;
    }

    commands->initialize(renderer);

    {
      VkDeviceSize bufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();

      frBuffer *stagingBuffer = new frBuffer();
      stagingBuffer->initialize(renderer, frBuffer::frBufferInfo{
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
      });
      stagingBuffer->copyData(0, bufferSize, cubeVertices.data());
     
      squareVBuf = new frBuffer();
      squareVBuf->initialize(renderer, frBuffer::frBufferInfo{
        bufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
      });
      squareVBuf->copyFromBuffer(renderer, commands, stagingBuffer, bufferSize);

      delete stagingBuffer;
    }

    {
      VkDeviceSize bufferSize = sizeof(cubeIndices[0]) * cubeIndices.size();

      frBuffer *stagingBuffer = new frBuffer();
      stagingBuffer->initialize(renderer, frBuffer::frBufferInfo{
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
      });
      stagingBuffer->copyData(0, bufferSize, cubeIndices.data());
     
      squareIBuf = new frBuffer();
      squareIBuf->initialize(renderer, frBuffer::frBufferInfo{
        bufferSize,
        (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
      });
      squareIBuf->copyFromBuffer(renderer, commands, stagingBuffer, bufferSize);

      delete stagingBuffer;
    }

    { // Create UBO buffers
      for (size_t i = 0; i < swapchain->imageCount(); ++i) {
        frBuffer *buf = new frBuffer();
        buf->initialize(renderer, frBuffer::frBufferInfo{
          sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
        });
        uboBuffers.push_back(buf);
      }
    }

    { // Create ubo descriptors
      ubos = descriptors->allocate(swapchain->imageCount(), uboLayout);
      size_t i = 0;
      for (auto ubo : ubos) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uboBuffers[i++]->get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UBO);

        ubo->update(frDescriptor::frDescriptorWriteInfo{
          0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          VK_NULL_HANDLE, &bufferInfo, VK_NULL_HANDLE
        });
      }
    }

    { // Create texture
      int width, height, channels;
      stbi_uc* pixels = stbi_load(textureFilePath, &width, &height, &channels, STBI_rgb_alpha);

      if (!pixels) {
        fprintf(stderr, "Failed to load image %s\n", textureFilePath);
        exit(1);
      }

      VkDeviceSize bufferSize = width * height * 4;

      frBuffer *stagingBuffer = new frBuffer();
      stagingBuffer->initialize(renderer, frBuffer::frBufferInfo{
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, {}
      });
      stagingBuffer->copyData(0, bufferSize, pixels);

      textureImage = new frImage();
      textureImage->initialize(renderer, frImage::frImageInfo{
        width, height, VK_FORMAT_R8G8B8A8_SRGB,
        (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        true, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, true
      });
      textureImage->setName(renderer, "textureImage");
      textureImage->transitionLayout(renderer, commands, frImage::frImageTransitionInfo{
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, VK_ACCESS_TRANSFER_WRITE_BIT
      });
      textureImage->copyFromBuffer(renderer, commands, stagingBuffer, bufferSize);
      delete stagingBuffer;
      textureImage->generateMipmaps(renderer, commands);
    }

    { // Create texture sampler
      textureSampler = new frSampler();
      textureSampler->initialize(renderer, frSampler::frSamplerInfo{
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_TRUE,
        VK_FALSE, VK_COMPARE_OP_ALWAYS,
        0.0f, static_cast<float>(textureImage->getMipLevels()), 0.0f,
        VK_BORDER_COLOR_INT_OPAQUE_BLACK
      });
    }

    { // Update texture (descriptor)
      VkDescriptorImageInfo imageInfo = {
        textureSampler->get(),
        textureImage->getView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      };

      texture = descriptors->allocate(1, textureLayout)[0];
      texture->update(frDescriptor::frDescriptorWriteInfo{
        0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        &imageInfo, VK_NULL_HANDLE, VK_NULL_HANDLE
      });
    }
    
    VkCommandBuffer *commandBuffers = commands->allocateBuffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, swapchain->imageCount());
    for (size_t i = 0; i < swapchain->imageCount(); ++i) {
      frSynchronization *sync = new frSynchronization();
      sync->initialize(renderer);
      synchronizations.push_back(sync);
    }

    // size_t frameCount = 0;
    // double previousTime = glfwGetTime();
    while (!window->shouldClose()) {
      // double currentTime = glfwGetTime();
      // frameCount++;
      // if (currentTime - previousTime >= 1.0f) {
      //   printf("FPS: %zu\n", frameCount);
      //   frameCount = 0;
      //   previousTime = currentTime;
      // }

      glfwPollEvents();

      synchronizations[frame]->wait();

      uint32_t imageIndex;
      try {
        imageIndex = renderer->acquireNextImage(swapchain, synchronizations[frame]);
      } catch (fr::frSwapchainResizeException &ex) {
        recreateSwapchain();
        continue;
      }

      synchronizations[frame]->reset();

      vkResetCommandBuffer(commandBuffers[frame], 0);
      if (!recordCommandBuffer(commandBuffers[frame], imageIndex)) {
        fprintf(stderr, "Failed to record command buffer!\n");
        return 1;
      }

      frCommands::submit(renderer, commandBuffers[frame], synchronizations[frame]);
      try {
        renderer->present(swapchain, synchronizations[frame], &imageIndex);
      } catch (fr::frSwapchainResizeException &ex) {
        recreateSwapchain();
        continue;
      }

      frame = (frame+1) % swapchain->imageCount();
    }

    renderer->waitIdle();
  } catch(fr::frVulkanException &ex) {
    fprintf(stderr, "[Renderer]: %s\n", ex.what());
    return 1;
  }

  cleanupSwapchain();

  for (auto buf : uboBuffers) delete buf;
  for (auto ubo : ubos) delete ubo;

  delete textureSampler;
  delete textureImage;
  delete texture;

  delete squareVBuf;
  delete squareIBuf;

  delete descriptors;

  delete uboLayout;
  delete textureLayout;

  delete pipeline;
  delete renderPass;

  for (auto sync : synchronizations) delete sync;

  delete commands;

  delete renderer;
  delete window;

  return 0;
}

bool recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex) {
  updateUbos();

  frCommands::begin(cmdBuf);

  std::vector<VkClearValue> clearValues = {};
  clearValues.push_back({{{0.0f, 0.0f, 0.0f, 1.0f}}});
  clearValues.push_back(VkClearValue{{1.0f, 0}});

  renderPass->begin(cmdBuf, swapchain->extent(), swapchainFramebuffers[imageIndex], clearValues);

  pipeline->bind(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS);

  auto scExtent = swapchain->extent();

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(scExtent.width);
  viewport.height = static_cast<float>(scExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = scExtent;
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  VkDeviceSize offsets[] = {0};
  VkBuffer vbufs[] = { squareVBuf->get() };
  vkCmdBindVertexBuffers(cmdBuf, 0, 1, vbufs, offsets);

  vkCmdBindIndexBuffer(cmdBuf, squareIBuf->get(), 0, VK_INDEX_TYPE_UINT32);

  pipeline->bindDescriptor(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, ubos[frame]);
  pipeline->bindDescriptor(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, texture);

  vkCmdDrawIndexed(cmdBuf, static_cast<uint32_t>(cubeIndices.size()), 1, 0, 0, 0);

  renderPass->end(cmdBuf);

  frCommands::end(cmdBuf);

  return true;
}

void createSwapchainFirst() {
  swapchain = new frSwapchain();
  swapchain->setDesiredPresentModes({VK_PRESENT_MODE_MAILBOX_KHR});
  swapchain->setDesiredFormats({
    {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
  });
  swapchain->initialize(renderer, window);

  int width, height;
  swapchain->getSize(&width, &height);
  projection = glm::perspective(
    glm::radians(45.0f),
    static_cast<float>(width / height),
    0.1f,
    100.0f);
  projection[1][1] *= -1;
}

void createSwapchainLast() {
  swapchainImageCount = swapchain->imageCount();
  swapchainImages = std::vector<frImage*>(swapchainImageCount);
  offscreenImages = std::vector<frImage*>(swapchainImageCount);
  depthImages = std::vector<frImage*>(swapchainImageCount);
  swapchainFramebuffers = std::vector<frFramebuffer*>(swapchainImageCount);
  int width, height;
  swapchain->getSize(&width, &height);
  for (uint32_t i = 0; i < swapchainImageCount; ++i) {
    {
      frImage *image = new frImage();
      image->initialize(renderer, swapchain->getImage(i), frImage::frImageInfo{
        width, height, swapchain->format(),
        (VkImageUsageFlagBits)0,
        false, 0
      });
      std::string name = "SwapchainImage" + std::to_string(i);
      image->setName(renderer, name.c_str());
      swapchainImages[i] = image;
    }

    {
      frImage *image = new frImage();
      image->initialize(renderer, frImage::frImageInfo{
        width, height, swapchain->format(),
        (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        true, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, false, 1,
        sampleCount
      });
      std::string name = "OffscreenImage" + std::to_string(i);
      image->setName(renderer, name.c_str());
      // TODO?
      // image->transitionLayout(renderer, commands, frImage::frImageTransitionInfo{

      // });
      offscreenImages[i] = image;
    }

    {
      frImage *image = new frImage();
      image->initialize(renderer, frImage::frImageInfo{
        width, height, depthFormat,
        (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        true, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT, false, 1,
        sampleCount
      });
      std::string name = "DepthImage" + std::to_string(i);
      image->setName(renderer, name.c_str());
      depthImages[i] = image;
    }

    {
      std::string name = "SwapchainFramebuffer" + std::to_string(i);
      frFramebuffer *fb = new frFramebuffer();
      fb->initialize(renderer, width, height, renderPass, { offscreenImages[i], depthImages[i], swapchainImages[i] });
      fb->setName(renderer, name.c_str());
      swapchainFramebuffers[i] = fb;
    }
  }
}

void cleanupSwapchain() {
  for (uint32_t i = 0; i < swapchainImageCount; ++i) {
    delete swapchainImages[i];
    delete depthImages[i];
    delete offscreenImages[i];
    delete swapchainFramebuffers[i];
  }
  delete swapchain;
}

void recreateSwapchain() {
  auto p = window->getSize();
  int w = p.first, h = p.second;
  while (w == 0 || h == 0) {
    p = window->getSize();
    w = p.first; h = p.second;
    glfwWaitEvents();
  }

  renderer->waitIdle();

  cleanupSwapchain();
  createSwapchainFirst();
  createSwapchainLast();
}

void updateUbos() {
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  UBO ubo = {
    projection,
    glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f))
  };

  for (auto buf : uboBuffers) buf->copyData(0, sizeof(ubo), (void*)&ubo);
}