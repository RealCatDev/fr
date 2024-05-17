#ifndef   FISSION_RENDER_HPP_
#define   FISSION_RENDER_HPP_

#include <iostream>
#include <vector>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace fr {

  class frWindowException : public std::exception {
  public:
    frWindowException(const char *msg):
      mMsg(msg) {}

    char *what() {
      return (char*)mMsg;
    }
  private:
    const char *mMsg;
  };

  class frVulkanException : public std::exception {
  public:
    frVulkanException(const char *msg):
      mMsg(msg) {}

    char *what() {
      return (char*)mMsg;
    }
  private:
    const char *mMsg;
  };

  class frSwapchainResizeException : public std::exception {
  public:
    frSwapchainResizeException()
    {}

    char *what() {
      return (char*)"frSwapchainResizeException";
    }
  };

  class frRenderer;
  class frWindow {
  public:
    frWindow(const char *title, int width, int height);
    frWindow(std::string title, int width, int height);
    ~frWindow();

    void cleanup();

    std::pair<int, int> getSize();

    bool shouldClose() const { return glfwWindowShouldClose(mWindow); }

    void addExtensions(frRenderer *renderer);

    GLFWwindow *get() const { return mWindow; }
  private:
    GLFWwindow *mWindow;
  };

  class frSwapchain {
    friend class frRenderer;
    friend class frSampler;
  public:
    frSwapchain();
    ~frSwapchain();

    void setDesiredFormats(std::vector<VkSurfaceFormatKHR> formats)         { mDesiredFormats = formats; }
    void setDesiredPresentModes(std::vector<VkPresentModeKHR> presentModes) { mDesiredPresentModes = presentModes; }

    void initialize(frRenderer *renderer, frWindow *window);
    void cleanup();
  public: // getters
    VkFormat   format() const { return mFormat.format; }
    VkExtent2D extent() const { return mExtent; }
    uint32_t   imageCount() const { return mImageCount; }
    VkImage    getImage(int image) const { return mImages[image]; }
    void       getSize(int *w, int *h) { *w = mExtent.width; *h = mExtent.height; }
  private:
    struct frSwapChainSupportDetails {
      VkSurfaceCapabilitiesKHR capabilities;
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR> presentModes;
    } mSupportDetails;

    std::vector<VkSurfaceFormatKHR> mDesiredFormats;
    std::vector<VkPresentModeKHR> mDesiredPresentModes;

    VkSurfaceFormatKHR   mFormat;
    VkPresentModeKHR     mPresentMode;
    VkExtent2D           mExtent;
    uint32_t             mImageCount;

    std::vector<VkImage> mImages;

    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE; // needed by cleanup()
  };

  class frSampler {
    friend class frImage;
  public:
    struct frSamplerInfo {
      VkFilter            magFilter = VK_FILTER_LINEAR;
      VkFilter            minFilter = VK_FILTER_LINEAR;
      VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

      VkBool32 anisotropyEnable = VK_FALSE;
      
      VkBool32    compareEnable = VK_FALSE;
      VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
      
      float mipLodBias = 0.0f;
      float minLod = 0.0f;
      float maxLod = 0.0f;
      
      VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    };
  public:
    frSampler();
    ~frSampler();

    void initialize(frRenderer *renderer, frSamplerInfo info);
    void cleanup();
  public:
    VkSampler get() const { return mSampler; }
  private:
    VkSampler mSampler;

    VkDevice mDevice;
  };

  class frFramebuffer;
  class frRenderPass {
    friend class frFramebuffer;
    friend class frPipeline;
  public:
    frRenderPass();
    ~frRenderPass();

    void initialize(frRenderer *renderer);
    void cleanup();

    void begin(VkCommandBuffer cmdBuf, VkExtent2D extent, frFramebuffer *fb, std::vector<VkClearValue> clearValues);
    void end(VkCommandBuffer cmdBuf);

    void setName(frRenderer *renderer, const char *name);
  public:
    void addAttachment(VkAttachmentDescription attachment) { mAttachments.push_back(attachment); }    
    void addSubpass(VkSubpassDescription subpass)          { mSubpasses.push_back(subpass); }
    void addDependency(VkSubpassDependency dependency)     { mDependencies.push_back(dependency); }
  private:
    std::vector<VkAttachmentDescription> mAttachments{};
    std::vector<VkSubpassDescription>    mSubpasses{};
    std::vector<VkSubpassDependency>     mDependencies{};
  private:
    VkRenderPass mRenderPass = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frImage;
  class frFramebuffer {
    friend class frRenderPass;
  public:
    frFramebuffer();
    ~frFramebuffer();

    void initialize(frRenderer *renderer, int width, int height, frRenderPass *renderPass, std::vector<frImage *> images);
    void cleanup();

    void setName(frRenderer *renderer, const char *name);
  private:
    VkFramebuffer mFramebuffer = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frShader {
    friend class frPipeline;
  public:
    frShader();
    ~frShader();

    bool initialize(frRenderer *renderer, const char *filepath, VkShaderStageFlagBits stage, const char *entry = "main");
    void initialize(frRenderer *renderer, std::vector<char> code, VkShaderStageFlagBits stage, const char *entry = "main");
    void cleanup();
  private:
    VkShaderModule mModule = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo mStageInfo{};

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frDescriptorLayout {
    friend class frDescriptors;
    friend class frPipeline;
  public:
    frDescriptorLayout();
    ~frDescriptorLayout();

    void initialize(frRenderer *renderer);
    void cleanup();
  public:
    void addBinding(VkDescriptorSetLayoutBinding binding) { mBindings.push_back(binding); }
  private:
    VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> mBindings;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frDescriptor {
    friend class frDescriptors;
    friend class frPipeline;
  private:
    frDescriptor(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set);
  public:
    struct frDescriptorWriteInfo {
      uint32_t binding;
      uint32_t dstArrayElement;
      uint32_t descriptorCount;
      VkDescriptorType descriptorType;

      VkDescriptorImageInfo*  imageInfo;
      VkDescriptorBufferInfo* bufferInfo;
      VkBufferView*           texelBufferView;
    };
  public:
    ~frDescriptor();

    void update(frDescriptorWriteInfo writeInfo);

    void cleanup();
  private:
    VkDevice mDevice;

    VkDescriptorPool mPool;
    VkDescriptorSet mSet;
  };

  class frDescriptors {
  public:
    frDescriptors();
    ~frDescriptors();

    void initialize(frRenderer *renderer, std::vector<VkDescriptorPoolSize> poolSizes);
    void cleanup();

    std::vector<frDescriptor*> allocate(uint32_t count, frDescriptorLayout *layout);
  private:
    VkDescriptorPool mPool = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frPipeline {
  public: // Structures
    struct frPipelineMultisamplingInfo {
      VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      VkBool32              sampleShadingEnable = false;
      float                 minSampleShading = 0.0f;
      VkBool32              alphaToCoverageEnable = false;
      VkBool32              alphaToOneEnable = false;
    };
  public:
    frPipeline();
    ~frPipeline();

    void initialize(frRenderer *renderer, frRenderPass *renderPass);
    void cleanup();

    void bind(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint);
    void bindDescriptor(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint, uint32_t firstSet, frDescriptor *descriptor);

    void setName(frRenderer *renderer, const char *name);
  public:
    void addShader(frShader *shader) { mShaders.push_back(shader->mStageInfo); }
    void addDescriptor(frDescriptorLayout *layout) { mDescLayouts.push_back(layout); }

    template <typename vert>
    void setVertexInputState() {
      std::vector<VkVertexInputAttributeDescription> attributes = vert::getAttributeDescriptions();
      auto binding = vert::getBindingDescription();
      VkPipelineVertexInputStateCreateInfo *info = new VkPipelineVertexInputStateCreateInfo({
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, VK_NULL_HANDLE, 0,
        1, binding,
        static_cast<uint32_t>(attributes.size()), attributes.data()
      });

      mVertexInputState = info;
    }
    
    void setInputAssemblyState(VkPipelineInputAssemblyStateCreateInfo info) {
      mInputAssemblyState = new VkPipelineInputAssemblyStateCreateInfo();
      memcpy(mInputAssemblyState, &info, sizeof(info));
    }

    void setTessellationState(VkPipelineTessellationStateCreateInfo info) {
      mTessellationState = new VkPipelineTessellationStateCreateInfo();
      memcpy(mTessellationState, &info, sizeof(info));
    }

    void setViewportState(VkPipelineViewportStateCreateInfo info) {
      mViewportState = new VkPipelineViewportStateCreateInfo();
      memcpy(mViewportState, &info, sizeof(info));
    }

    void setRasterizationState(VkPipelineRasterizationStateCreateInfo info) {
      mRasterizationState = new VkPipelineRasterizationStateCreateInfo();
      memcpy(mRasterizationState, &info, sizeof(info));
    }

    void setMultisampleInfo(VkPipelineMultisampleStateCreateInfo info) {
      mMultisampleInfo = new VkPipelineMultisampleStateCreateInfo();
      memcpy(mMultisampleInfo, &info, sizeof(info));
    }

    void setDepthStencilState(VkPipelineDepthStencilStateCreateInfo info) {
      mDepthStencilState = new VkPipelineDepthStencilStateCreateInfo();
      memcpy(mDepthStencilState, &info, sizeof(info));
    }

    void setColorBlendState(VkPipelineColorBlendStateCreateInfo info) {
      mColorBlendState = new VkPipelineColorBlendStateCreateInfo();
      memcpy(mColorBlendState, &info, sizeof(info));
    }

    void setDynamicState(VkPipelineDynamicStateCreateInfo info) {
      mDynamicState = new VkPipelineDynamicStateCreateInfo();
      memcpy(mDynamicState, &info, sizeof(info));
    }
  private:
    std::vector<VkPipelineShaderStageCreateInfo> mShaders{};
    std::vector<frDescriptorLayout*> mDescLayouts{};

    VkPipelineVertexInputStateCreateInfo   *mVertexInputState = VK_NULL_HANDLE;
    VkPipelineInputAssemblyStateCreateInfo *mInputAssemblyState = VK_NULL_HANDLE;
    VkPipelineTessellationStateCreateInfo  *mTessellationState = VK_NULL_HANDLE;
    VkPipelineViewportStateCreateInfo      *mViewportState = VK_NULL_HANDLE;
    VkPipelineRasterizationStateCreateInfo *mRasterizationState = VK_NULL_HANDLE;
    VkPipelineMultisampleStateCreateInfo   *mMultisampleInfo = VK_NULL_HANDLE;
    VkPipelineDepthStencilStateCreateInfo  *mDepthStencilState = VK_NULL_HANDLE;
    VkPipelineColorBlendStateCreateInfo    *mColorBlendState = VK_NULL_HANDLE;
    VkPipelineDynamicStateCreateInfo       *mDynamicState = VK_NULL_HANDLE;
  private:
    VkPipelineLayout mLayout;
    VkPipeline mPipeline;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frSynchronization;
  class frCommands {
  public:
    frCommands();
    ~frCommands();

    void initialize(frRenderer *renderer);
    void cleanup();

    VkCommandBuffer *allocateBuffers(VkCommandBufferLevel level, uint32_t count = 1);

    VkCommandBuffer beginSingleTime();
    void endSingleTime(frRenderer *renderer, VkCommandBuffer cmdBuf);
  public:
    static void begin(VkCommandBuffer cmdBuf, VkCommandBufferUsageFlags flags = 0);
    static void end(VkCommandBuffer cmdBuf);

    static void submit(frRenderer *renderer, VkCommandBuffer cmdBuf, frSynchronization *sync=nullptr);
  private:
    VkCommandPool mPool = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frSynchronization {
    friend class frRenderer;
    friend class frCommands;
  public:
    frSynchronization();
    ~frSynchronization();

    void initialize(frRenderer *renderer);
    void cleanup();

    void wait();
    void reset();
  private:
    VkSemaphore mImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore mRenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence     mInFlightFence = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frBuffer {
  public:
    struct frBufferInfo {
      VkDeviceSize size;
      VkBufferUsageFlagBits usage;
      VkMemoryPropertyFlags properties;
      std::vector<uint32_t> queueFamilyIndices;
    };
  public:
    frBuffer();
    ~frBuffer();

    void copyData(VkDeviceSize offset, VkDeviceSize size, void *data);
    void copyFromBuffer(frRenderer *renderer, frCommands *commands, frBuffer *buffer, VkDeviceSize size);

    void initialize(frRenderer *renderer, frBufferInfo info, bool bindMemory = true);
    void cleanup();
  public:
    VkBuffer get() const { return mBuffer; }
  private:
    VkBuffer       mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;

    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frImage {
    friend class frFramebuffer;
  public:
    struct frImageInfo {
      int width, height;                      // Size of the image.
      VkFormat format;                        // Format of the image.
      VkImageUsageFlagBits usage;             // Image usage flags
      bool memory;                            // Determine whether create mImageMemory or not.
      VkMemoryPropertyFlags memoryProperties; // Memory properties, if (memory == false) continue;
      VkImageAspectFlagBits imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
      bool generateMipmaps = false;           // Generate mipmaps
      uint32_t mipLevels = 1;
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    struct frImageTransitionInfo {
      VkImageLayout        oldLayout;
      VkImageLayout        newLayout;
      VkPipelineStageFlags srcStage;
      VkPipelineStageFlags dstStage;
      VkAccessFlags        srcAccess;
      VkAccessFlags        dstAccess;
    };
  public:
    frImage();
    ~frImage();

    void initialize(frRenderer *renderer, frImageInfo info);
    // `image` will not be cleaned up on delete!
    void initialize(frRenderer *renderer, VkImage image, frImageInfo info);
    void cleanup();

    void transitionLayout(frRenderer *renderer, frCommands *commands, frImageTransitionInfo info);
    void generateMipmaps(frRenderer *renderer, frCommands *commands);
    void copyFromBuffer(frRenderer *renderer, frCommands *commands, frBuffer *buffer, VkDeviceSize size);

    void setName(frRenderer *renderer, const char *imageName);
  public:
    VkImageView getView() const { return mImageView; }

    uint32_t getMipLevels() const { return mInfo.mipLevels; }
  private:
    void createView(frImageInfo info);

    bool hasStencilComponent(VkFormat format);
  private:
    frImageInfo mInfo{};

    bool           mDestroyImage = true;
    VkImage        mImage        = VK_NULL_HANDLE;
    VkDeviceMemory mImageMemory  = VK_NULL_HANDLE;
    VkImageView    mImageView    = VK_NULL_HANDLE;
    
    VkDevice mDevice = VK_NULL_HANDLE;
  };

  class frRenderer {
    friend class frSwapchain;
    friend class frSampler;
    friend class frImage;
    friend class frRenderPass;
    friend class frFramebuffer;
    friend class frShader;
    friend class frDescriptorLayout;
    friend class frDescriptors;
    friend class frDescriptor;
    friend class frPipeline;
    friend class frCommands;
    friend class frSynchronization;
    friend class frBuffer;
  public:
    frRenderer();
    ~frRenderer();

    void initialize(frWindow *window, VkPhysicalDeviceFeatures *deviceFeatures);
    void cleanup();

    void addLayer(const char *layerName)         { mLayers.push_back(layerName); }
    void addExtension(const char *extensionName) { mExtensions.push_back(extensionName); }
    void setApplicationName(const char *appName) { mApplicationName = appName; }
    void enableValidation() { mValidation = true; }

    void setSurfaceFormat(VkFormat format) { mSurfaceFormat = format; }

    uint32_t acquireNextImage(frSwapchain *swapchain, frSynchronization *sync);

    void present(frSwapchain *swapchain, frSynchronization *sync, uint32_t *imageIndex);

    void waitIdle();
  public: // Utilities
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlagBits usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory *bufferMemory);
    VkSampleCountFlagBits GetMaxUsableSampleCount();
    VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDeviceProperties properties);
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
  private:
    frWindow *mWindow = nullptr;

    std::vector<const char *> mLayers = {};
    std::vector<const char *> mExtensions = {};
    const char *mApplicationName = nullptr;

    bool mValidation = false;
    
    std::vector<const char *> mDeviceLayers = {};
    std::vector<const char *> mDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkFormat mSurfaceFormat = VK_FORMAT_UNDEFINED;
  private:
    VkInstance mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;

    VkQueue mGraphicsQueue        = VK_NULL_HANDLE;
    uint32_t mGraphicsQueueFamily = 0;
    bool mGraphicsQueueSet        = false;
    
    VkQueue mPresentQueue        = VK_NULL_HANDLE;
    uint32_t mPresentQueueFamily = 0;
    bool mPresentQueueSet        = false;
  public: // Debug Utilities
    PFN_vkSetDebugUtilsObjectNameEXT getSetDebugUtilsObjectNameFunc() {
      static PFN_vkSetDebugUtilsObjectNameEXT sSetDebugUtilsObjectNameFunc;
      if (!sSetDebugUtilsObjectNameFunc) {
        sSetDebugUtilsObjectNameFunc = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(mInstance, "vkSetDebugUtilsObjectNameEXT"));
      }
      return sSetDebugUtilsObjectNameFunc;
    }
  };

}

#endif // FISSION_RENDER_HPP_