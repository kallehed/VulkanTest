#include <alloca.h>
#include <cstdio>
#include <cstring>
#include <glm/common.hpp>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <stdio.h>

#define APPLICATION_NAME "Vulkan window"

#define ENABLE_VALIDATION_LAYERS 1

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1); // Set major version to 1
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Set minor version to 3
  GLFWwindow *window =
      glfwCreateWindow(800, 600, APPLICATION_NAME, nullptr, nullptr);

  {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    printf("%d extensions supported\n", extensionCount);
    VkExtensionProperties *extensions = (VkExtensionProperties *)malloc(
        sizeof(VkExtensionProperties) * extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                           extensions);

    for (unsigned int i = 0; i < extensionCount; ++i) {
      printf("extension: `%s` with ver: `%d`\n", extensions[i].extensionName,
             extensions[i].specVersion);
    }
    free(extensions);
  }

  VkInstance instance;
  {
    const char *wanted_layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    VkLayerProperties *availableLayers =
        (VkLayerProperties *)alloca(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
    for (uint32_t want_idx = 0;
         want_idx < sizeof(wanted_layers) / sizeof(char *); ++want_idx) {
      bool available = false;
      for (uint32_t layer_idx = 0; layer_idx < layerCount; ++layer_idx) {
        if (strcmp(wanted_layers[want_idx],
                   availableLayers[layer_idx].layerName) == 0) {
          available = true;
          break;
        }
      }
      if (!available) {
        printf("ERROR: `%s` validation layer is not supported!\n",
               wanted_layers[want_idx]);
      }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = APPLICATION_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (ENABLE_VALIDATION_LAYERS) {
      createInfo.enabledLayerCount = sizeof(wanted_layers) / sizeof(char *);
      createInfo.ppEnabledLayerNames = wanted_layers;
    } else {
      createInfo.enabledLayerCount = 0;
    }

    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      printf("ext: %s\n", glfwExtensions[i]);
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      fprintf(stderr, "ERROR: failed to create instance!");
    }
  }

  // Create window surface
  VkSurfaceKHR surface;
  {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
        VK_SUCCESS) {
      printf("ERROR: failed to create window surface!");
    }
  }

  const char *deviceExtensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  // look at physical devices
  VkPhysicalDevice phys_device = VK_NULL_HANDLE;
  VkSurfaceFormatKHR format;
  VkPresentModeKHR present_mode;
  {
    uint32_t devices;
    vkEnumeratePhysicalDevices(instance, &devices, nullptr);
    VkPhysicalDevice *devs =
        (VkPhysicalDevice *)alloca(sizeof(VkPhysicalDevice) * devices);
    vkEnumeratePhysicalDevices(instance, &devices, devs);
    printf("found %d devices\n", devices);
    for (uint32_t i = 0; i < devices; ++i) {
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(devs[i], &props);
      VkPhysicalDeviceFeatures devFeatures;
      vkGetPhysicalDeviceFeatures(devs[i], &devFeatures);

      uint32_t extensionCount;
      vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &extensionCount,
                                           nullptr);
      VkExtensionProperties *extProps = (VkExtensionProperties *)alloca(
          sizeof(VkExtensionProperties) * extensionCount);
      vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &extensionCount,
                                           extProps);
      bool has_all_extensions = true;
      for (uint32_t want_idx = 0;
           want_idx < sizeof(deviceExtensions) / sizeof(char *); ++want_idx) {
        bool has = false;
        for (uint32_t actual_idx = 0; actual_idx < extensionCount;
             ++actual_idx) {
          if (strcmp(deviceExtensions[want_idx],
                     extProps[actual_idx].extensionName) == 0) {
            has = true;
            break;
          }
        }
        if (!has) {
          printf("device below doesn't have ext: %s\n",
                 deviceExtensions[want_idx]);
          has_all_extensions = false;
        }
      }
      printf("device::: name: %s, vulkan minor version: %d, tessellation "
             "shader: %d, \n",
             props.deviceName, VK_API_VERSION_MINOR(props.apiVersion),
             devFeatures.tessellationShader);
      if (!has_all_extensions) {
        continue;
      }

      // check formats
      uint32_t formatCount;
      vkGetPhysicalDeviceSurfaceFormatsKHR(devs[i], surface, &formatCount,
                                           nullptr);
      printf("Supported formats: %d\n", formatCount);
      VkSurfaceFormatKHR best_format;
      bool set_the_format = false;
      if (formatCount != 0) {
        VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)alloca(
            sizeof(VkSurfaceFormatKHR) * formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(devs[i], surface, &formatCount,
                                             formats);
        for (uint32_t i = 0; i < formatCount; ++i) {
          printf("format: %d, colorspace: %d\n", formats[i].format,
                 formats[i].colorSpace);
          if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
              formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
              !set_the_format) {
            best_format = formats[i];
            set_the_format = true;
          }
        }
        if (!set_the_format) {
          best_format = formats[0];
        }
      }

      // check for presentModes
      uint32_t presentModeCount;
      VkPresentModeKHR best_present;
      {
        bool set_present_Mode = false;
        vkGetPhysicalDeviceSurfacePresentModesKHR(devs[i], surface,
                                                  &presentModeCount, nullptr);
        printf("Supported present modes: %d\n", presentModeCount);
        if (presentModeCount != 0) {
          VkPresentModeKHR *presentModes = (VkPresentModeKHR *)alloca(
              sizeof(VkPresentModeKHR) * presentModeCount);
          vkGetPhysicalDeviceSurfacePresentModesKHR(
              devs[i], surface, &presentModeCount, presentModes);
          for (uint32_t i = 0; i < presentModeCount; ++i) {
            printf("present mode: %d\n", presentModes[i]);
            if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
              // prefer immediate
              best_present = presentModes[i];
              set_present_Mode = true;
            }
          }
          if (!set_present_Mode) {
            best_present = VK_PRESENT_MODE_FIFO_KHR; // always available
          }
        }
      }

      if (has_all_extensions && formatCount != 0 && presentModeCount != 0 &&
          (phys_device == VK_NULL_HANDLE ||
           props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)) {
        phys_device = devs[i];
        format = best_format;
        present_mode = best_present;
        printf("chose this one above me!\n");
      }
    }
  }

  // look for available queues in physical device
  int64_t queue_graphics_idx = -1, queue_present_idx = -1;
  {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queueFamilyCount,
                                             nullptr);
    VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties *)alloca(
        sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queueFamilyCount,
                                             queueFamilies);
    printf("found %d queue families!\n", queueFamilyCount);
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      VkQueueFamilyProperties q = queueFamilies[i];
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface,
                                           &presentSupport);
      printf(
          "count: %d, graphics: %d, compute: %d, transfer: %d, present: %d\n",
          q.queueCount, q.queueFlags & VK_QUEUE_GRAPHICS_BIT,
          0 != (q.queueFlags & VK_QUEUE_COMPUTE_BIT),
          0 != (q.queueFlags & VK_QUEUE_TRANSFER_BIT), presentSupport);
      if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_graphics_idx == -1) {
        queue_graphics_idx = i;
      }
      // pick the first one
      if (presentSupport && queue_present_idx == -1) {
        queue_present_idx = i;
      }
    }
  }
  printf("graphics queue idx: %ld, present queue idx: %ld\n",
         queue_graphics_idx, queue_present_idx);

  // create logical device
  VkDevice device;
  {
    int number_of_queues = 2;
    int64_t queue_idxs[] = {queue_graphics_idx, queue_present_idx};
    if (queue_graphics_idx == queue_present_idx) {
      number_of_queues = 1;
    }
    VkDeviceQueueCreateInfo queueCreateInfos[2]{};
    VkPhysicalDeviceFeatures deviceFeatures[2]{};
    float queuePriority = 1.0f;
    for (int queue_idx = 0; queue_idx < number_of_queues; ++queue_idx) {
      queueCreateInfos[queue_idx].sType =
          VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[queue_idx].queueFamilyIndex = queue_idxs[queue_idx];
      queueCreateInfos[queue_idx].queueCount = 1;
      queueCreateInfos[queue_idx].pQueuePriorities = &queuePriority;
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.queueCreateInfoCount = number_of_queues;

    createInfo.pEnabledFeatures = deviceFeatures;

    createInfo.enabledExtensionCount =
        sizeof(deviceExtensions) / sizeof(char *);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(phys_device, &createInfo, nullptr, &device) !=
        VK_SUCCESS) {
      printf("ERROR: Could not create logical device!");
    }
  }

  // get queue
  {
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queue_graphics_idx, 0, &graphicsQueue);
    VkQueue presentQueue;
    vkGetDeviceQueue(device, queue_present_idx, 0, &presentQueue);
  }

  // check for phys device surface capabilites
  VkSurfaceCapabilitiesKHR capabilites;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &capabilites);

  // choose swap extent (resolution in pixels of swap buffer)
  VkExtent2D our_extent;
  {
    printf("Range of swap extent: min wh: %d, %d, cur wh: %d, %d, max wh: %d "
           "%d\n. minImageCount: %d, maxImageCount: %d\n",
           capabilites.minImageExtent.width, capabilites.minImageExtent.height,
           capabilites.currentExtent.width, capabilites.currentExtent.height,
           capabilites.maxImageExtent.width, capabilites.maxImageExtent.height,
           capabilites.minImageCount, capabilites.maxImageCount);
    if (capabilites.currentExtent.width != UINT32_MAX) {
      our_extent = capabilites.currentExtent;
    } else {
      // if we got 0xFFFFF... then we can choose the pixel amount that fits us
      uint32_t width, height;
      glfwGetFramebufferSize(window, (int *)&width, (int *)&height);
      if (width < capabilites.minImageExtent.width) {
        width = capabilites.minImageExtent.width;
      }
      if (width > capabilites.maxImageExtent.width) {
        width = capabilites.maxImageExtent.width;
      }
      if (height < capabilites.minImageExtent.height) {
        height = capabilites.minImageExtent.height;
      }
      if (height > capabilites.maxImageExtent.height) {
        height = capabilites.maxImageExtent.height;
      }
      our_extent = VkExtent2D{width, height};
    }
  }

  // create actual swap chain
  VkSwapchainKHR swapchain;
  uint32_t imageCount = capabilites.minImageCount + 1;
  VkImage *swapchain_images;
  {
    // select image count for swapchain buffering
    // if 0, can have infinite, otherwise check that we don't have too many imgs
    if (capabilites.maxImageCount > 0 &&
        imageCount > capabilites.maxImageCount) {
      imageCount = capabilites.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = our_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // render to
    uint32_t queue_idxs[] = {(uint32_t)queue_graphics_idx,
                             (uint32_t)queue_present_idx};
    if (queue_graphics_idx == queue_present_idx) {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;
      createInfo.pQueueFamilyIndices = nullptr;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queue_idxs;
    }
    createInfo.preTransform = capabilites.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = present_mode;
    // allow other windows to be in front
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) !=
        VK_SUCCESS) {
      printf("ERROR: failed to create swap chain!\n");
    }

    // get swapchain images (could be dif amount of images now)
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    // TODO free this malloc later somehow
    swapchain_images = (VkImage *)malloc(sizeof(VkImage) * imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchain_images);
  }

  printf("created %d images\n", imageCount);
  // create image views
  VkImageView *image_views;
  {
    image_views = (VkImageView *)malloc(sizeof(VkImageView) * imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapchain_images[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = format.format;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;
      if (vkCreateImageView(device, &createInfo, nullptr, &image_views[i]) !=
          VK_SUCCESS) {
        printf("ERROR: could'nt create image view!");
      }
    }
  }

  // load spirv .spv files
  VkPipelineShaderStageCreateInfo shaderStages[2];
  VkShaderModule vert_shader_module;
  VkShaderModule frag_shader_module;
  {
    FILE *file = fopen("shaders/vert.spv", "r");
    if (file == NULL)
      printf("ERROR: could not find file vert.spv!\n");
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *buf = (char *)malloc(size);
    fread(buf, 1, size, file);
    fclose(file);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = (uint32_t *)buf; // safe bc of malloc default alignment
    if (vkCreateShaderModule(device, &createInfo, nullptr,
                             &vert_shader_module) != VK_SUCCESS) {
      printf("ERROR: could not create vertex shader shader module!\n");
    }
    free(buf);

    file = fopen("shaders/frag.spv", "r");
    if (file == NULL)
      printf("ERROR: could not find file frag.spv!\n");
    fseek(file, 0L, SEEK_END);
    size = ftell(file);
    rewind(file);
    buf = (char *)malloc(size);
    fread(buf, 1, size, file);
    fclose(file);

    createInfo.codeSize = size;
    createInfo.pCode = (uint32_t *)buf; // safe bc of malloc default alignment
    if (vkCreateShaderModule(device, &createInfo, nullptr,
                             &frag_shader_module) != VK_SUCCESS) {
      printf("ERROR: could not create frag shader shader module!\n");
    }

    // shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // vert shader
    vertShaderStageInfo.module = vert_shader_module;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr; // special constants
                                                       //
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // vert shader
    fragShaderStageInfo.module = frag_shader_module;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr; // special constants

    shaderStages[0] = vertShaderStageInfo;
    shaderStages[1] = fragShaderStageInfo;
  }

  // dynamic state
  VkPipelineDynamicStateCreateInfo dstate{};
  VkViewport viewport{};
  VkRect2D scissor{};
  VkPipelineViewportStateCreateInfo viewportState{};
  {
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    dstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dstate.dynamicStateCount = sizeof(dynamicStates) / sizeof(VkDynamicState);
    dstate.pDynamicStates = dynamicStates;

    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(our_extent.width);
    viewport.height = static_cast<float>(our_extent.height);
    viewport.minDepth = 0.f; // standard depth values
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = our_extent;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
  }

  // vertex input
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  {
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = NULL;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = NULL;
  }

  // Pipeline input assembly state
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  {
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
  }

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  {
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f; // for polygon mode line or point
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    rasterizer.depthBiasEnable = VK_FALSE;
  }
  // Multisampling disable
  VkPipelineMultisampleStateCreateInfo multisampling{};
  {
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  }

  // color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  {
    // for our framebuffer
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // for everything
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
  }

  // pipeline layout, for uniforms
  VkPipelineLayout pipelineLayout{};
  {
    VkPipelineLayoutCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeInfo.setLayoutCount = 0;
    pipeInfo.pSetLayouts = nullptr;
    pipeInfo.pushConstantRangeCount = 0;
    pipeInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &pipeInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
      printf("ERROR: failed to create pipeline layout!\n");
    }
  }

  // render passes
  VkRenderPass renderPass;
  {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR; // clear buffer to black after drawing it
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // idx
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
        VK_SUCCESS) {
      printf("ERROR: could not create render pass!\n");
    }
  }

  // create graphics pipeline
  VkPipeline graphicsPipeline;
  {
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; // vert and frag
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dstate;

    pipelineInfo.layout = pipelineLayout; // lives longer
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0; // idx of subpass that renders
                              //
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // derive from
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &graphicsPipeline) != VK_SUCCESS) {
      printf("ERROR: could not create graphics pipeline!\n");
    }
    // destroy shader modules
    vkDestroyShaderModule(device, frag_shader_module, nullptr);
    vkDestroyShaderModule(device, vert_shader_module, nullptr);
  }

  // create VkFramebuffer objects
  VkFramebuffer *swapchainFramebuffers;
  {
    swapchainFramebuffers =
        (VkFramebuffer *)malloc(sizeof(VkFramebuffer) * imageCount);

    for (uint32_t i = 0; i < imageCount; ++i) {

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = &image_views[i];
      framebufferInfo.width = our_extent.width;
      framebufferInfo.height = our_extent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                              &swapchainFramebuffers[i]) != VK_SUCCESS) {
        printf("ERROR: failed to create nbr %i framebuffer!\n", i);
      }
    }
  }

  glm::mat4 matrix;
  glm::vec4 vec;
  auto test = matrix * vec;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }

  for (uint32_t i = 0; i < imageCount; ++i) {
    vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
  }

  for (uint32_t i = 0; i < imageCount; ++i) {
    vkDestroyImageView(device, image_views[i], nullptr);
  }

  vkDestroyPipeline(device, graphicsPipeline, nullptr);
  vkDestroyRenderPass(device, renderPass, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroySwapchainKHR(device, swapchain, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
