#include <alloca.h>
#include <cstddef>
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

// how many frames we can render at once
#define MAX_FRAMES_IN_FLIGHT 2

const char *deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

char *read_whole_file(const char *file_name, long *size_write_to) {
  FILE *file = fopen(file_name, "r");
  if (file == NULL)
    printf("ERROR: could not find file %s!\n", file_name);
  fseek(file, 0L, SEEK_END);
  long size = ftell(file);
  *size_write_to = size;
  rewind(file);
  char *buf = (char *)malloc(size);
  fread(buf, 1, size, file);
  fclose(file);
  return buf;
}

VkShaderModule create_shader_module(VkDevice device, const char *file_name) {
  VkShaderModule shader_module;
  long size;
  char *buf = read_whole_file(file_name, &size);

  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = size;
  createInfo.pCode = (uint32_t *)buf; // safe bc of malloc default alignment
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shader_module) !=
      VK_SUCCESS) {
    printf("ERROR: could not create %s shader module!\n", file_name);
  }
  free(buf);
  return shader_module;
}

struct MyVk {
  GLFWwindow *window;
  VkInstance instance; // info about my computer and the application and stuff

  VkPhysicalDevice phys_device;
  VkSurfaceFormatKHR format;
  VkPresentModeKHR present_mode;

  VkDevice device;

  VkSurfaceKHR surface;

  VkSurfaceCapabilitiesKHR capabilites;

  int64_t queue_graphics_idx, queue_present_idx;
  VkQueue graphicsQueue, presentQueue;

  VkExtent2D extent;

  VkSwapchainKHR swapchain;
  VkImage *swapchain_images;
  uint32_t swapchain_images_count;

  VkImageView *image_views; // views into the swapchain images

  VkPipelineShaderStageCreateInfo shaderStages[2];
  VkShaderModule vert_shader_module, frag_shader_module;

  VkPipelineDynamicStateCreateInfo dstate{};
  VkViewport viewport{};
  VkRect2D scissor{};

  static const uint32_t dynamicStateCount = 2;
  VkPipelineViewportStateCreateInfo viewportState{};
  VkDynamicState dynamicStates[dynamicStateCount];

  VkRenderPass renderPass;
  VkFramebuffer *swapchainFramebuffers;

  VkPipelineLayout pipelineLayout{};

  VkPipeline graphicsPipeline;

  VkCommandPool commandPool;
  VkCommandBuffer *commandBuffers;

  VkSemaphore *imageAvailableSemaphores;
  VkSemaphore *renderFinishedSemaphores;
  VkFence *inFlightFences;

  uint32_t currentFrame = 0; // what frame we are rendering
  bool framebuffer_resized = false;
};

void framebuffer_resize_callback(GLFWwindow *window, int width, int height) {
  MyVk *m = (MyVk *)glfwGetWindowUserPointer(window);
  m->framebuffer_resized = true;
}

void my_vk_create_window(MyVk *m) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1); // Set major version to 1
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Set minor version to 3
  m->window = glfwCreateWindow(800, 600, APPLICATION_NAME, nullptr, nullptr);
  glfwSetWindowUserPointer(m->window, m);
  glfwSetFramebufferSizeCallback(m->window, framebuffer_resize_callback);
}

void my_vk_create_instance(MyVk *m) {
  const char *wanted_layers[] = {"VK_LAYER_KHRONOS_validation"};
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  VkLayerProperties *availableLayers =
      (VkLayerProperties *)alloca(sizeof(VkLayerProperties) * layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
  for (uint32_t want_idx = 0; want_idx < sizeof(wanted_layers) / sizeof(char *);
       ++want_idx) {
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

  if (vkCreateInstance(&createInfo, nullptr, &m->instance) != VK_SUCCESS) {
    fprintf(stderr, "ERROR: failed to create instance!");
  }
}

void my_vk_create_surface(MyVk *m) {

  // Create window surface
  {
    if (glfwCreateWindowSurface(m->instance, m->window, nullptr, &m->surface) !=
        VK_SUCCESS) {
      printf("ERROR: failed to create window surface!");
    }
  }
}

void my_vk_create_phys_device(MyVk *m) {

  // look at physical devices
  m->phys_device = VK_NULL_HANDLE;
  uint32_t devices;
  vkEnumeratePhysicalDevices(m->instance, &devices, nullptr);
  VkPhysicalDevice *devs =
      (VkPhysicalDevice *)alloca(sizeof(VkPhysicalDevice) * devices);
  vkEnumeratePhysicalDevices(m->instance, &devices, devs);
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
      for (uint32_t actual_idx = 0; actual_idx < extensionCount; ++actual_idx) {
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
    vkGetPhysicalDeviceSurfaceFormatsKHR(devs[i], m->surface, &formatCount,
                                         nullptr);
    printf("Supported formats: %d\n", formatCount);
    VkSurfaceFormatKHR best_format;
    bool set_the_format = false;
    if (formatCount != 0) {
      VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)alloca(
          sizeof(VkSurfaceFormatKHR) * formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(devs[i], m->surface, &formatCount,
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
      vkGetPhysicalDeviceSurfacePresentModesKHR(devs[i], m->surface,
                                                &presentModeCount, nullptr);
      printf("Supported present modes: %d\n", presentModeCount);
      if (presentModeCount != 0) {
        VkPresentModeKHR *presentModes = (VkPresentModeKHR *)alloca(
            sizeof(VkPresentModeKHR) * presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            devs[i], m->surface, &presentModeCount, presentModes);
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
        (m->phys_device == VK_NULL_HANDLE ||
         props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)) {
      m->phys_device = devs[i];
      m->format = best_format;
      m->present_mode = best_present;
      printf("chose this one above me!\n");
    }
  }
}

void my_vk_get_queue_indices(MyVk *m) {

  // look for available queues in physical device
  m->queue_graphics_idx = -1;
  m->queue_present_idx = -1;
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m->phys_device, &queueFamilyCount,
                                           nullptr);
  VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties *)alloca(
      sizeof(VkQueueFamilyProperties) * queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m->phys_device, &queueFamilyCount,
                                           queueFamilies);
  printf("found %d queue families!\n", queueFamilyCount);
  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    VkQueueFamilyProperties q = queueFamilies[i];
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m->phys_device, i, m->surface,
                                         &presentSupport);
    printf("count: %d, graphics: %d, compute: %d, transfer: %d, present: %d\n",
           q.queueCount, q.queueFlags & VK_QUEUE_GRAPHICS_BIT,
           0 != (q.queueFlags & VK_QUEUE_COMPUTE_BIT),
           0 != (q.queueFlags & VK_QUEUE_TRANSFER_BIT), presentSupport);
    if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT && m->queue_graphics_idx == -1) {
      m->queue_graphics_idx = i;
    }
    // pick the first one
    if (presentSupport && m->queue_present_idx == -1) {
      m->queue_present_idx = i;
    }
  }
  printf("graphics queue idx: %ld, present queue idx: %ld\n",
         m->queue_graphics_idx, m->queue_present_idx);
}

void my_vk_create_device(MyVk *m) {

  // create logical device
  int number_of_queues = 2;
  int64_t queue_idxs[] = {m->queue_graphics_idx, m->queue_present_idx};
  if (m->queue_graphics_idx == m->queue_present_idx) {
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

  createInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(char *);
  createInfo.ppEnabledExtensionNames = deviceExtensions;

  if (vkCreateDevice(m->phys_device, &createInfo, nullptr, &m->device) !=
      VK_SUCCESS) {
    printf("ERROR: Could not create logical device!");
  }
}

void my_vk_get_capabilites(MyVk *m) {

  // check for phys device surface capabilites
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m->phys_device, m->surface,
                                            &m->capabilites);
}

void my_vk_create_extent(MyVk *m) {
  // choose swap extent (resolution in pixels of swap buffer)
  printf(
      "Range of swap extent: min wh: %d, %d, cur wh: %d, %d, max wh: %d "
      "%d\n. minImageCount: %d, maxImageCount: %d\n",
      m->capabilites.minImageExtent.width, m->capabilites.minImageExtent.height,
      m->capabilites.currentExtent.width, m->capabilites.currentExtent.height,
      m->capabilites.maxImageExtent.width, m->capabilites.maxImageExtent.height,
      m->capabilites.minImageCount, m->capabilites.maxImageCount);
  if (m->capabilites.currentExtent.width != UINT32_MAX) {
    m->extent = m->capabilites.currentExtent;
  } else {
    // if we got 0xFFFFF... then we can choose the pixel amount that fits us
    uint32_t width, height;
    glfwGetFramebufferSize(m->window, (int *)&width, (int *)&height);
    if (width < m->capabilites.minImageExtent.width) {
      width = m->capabilites.minImageExtent.width;
    }
    if (width > m->capabilites.maxImageExtent.width) {
      width = m->capabilites.maxImageExtent.width;
    }
    if (height < m->capabilites.minImageExtent.height) {
      height = m->capabilites.minImageExtent.height;
    }
    if (height > m->capabilites.maxImageExtent.height) {
      height = m->capabilites.maxImageExtent.height;
    }
    m->extent = VkExtent2D{width, height};
  }
}

void my_vk_create_swapchain(MyVk *m) {
  my_vk_get_capabilites(m);
  my_vk_create_extent(m);

  // create actual swap chain
  m->swapchain_images_count = m->capabilites.minImageCount + 1;
  // select image count for swapchain buffering
  // if 0, can have infinite, otherwise check that we don't have too many imgs
  if (m->capabilites.maxImageCount > 0 &&
      m->swapchain_images_count > m->capabilites.maxImageCount) {
    m->swapchain_images_count = m->capabilites.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m->surface;
  createInfo.minImageCount = m->swapchain_images_count;
  createInfo.imageFormat = m->format.format;
  createInfo.imageColorSpace = m->format.colorSpace;
  createInfo.imageExtent = m->extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // render to
  uint32_t queue_idxs[] = {(uint32_t)m->queue_graphics_idx,
                           (uint32_t)m->queue_present_idx};
  if (m->queue_graphics_idx == m->queue_present_idx) {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queue_idxs;
  }
  createInfo.preTransform = m->capabilites.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = m->present_mode;
  // allow other windows to be in front
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(m->device, &createInfo, nullptr, &m->swapchain) !=
      VK_SUCCESS) {
    printf("ERROR: failed to create swap chain!\n");
  }

  // get swapchain images (could be dif amount of images now)
  vkGetSwapchainImagesKHR(m->device, m->swapchain, &m->swapchain_images_count,
                          nullptr);
  // TODO free this malloc later somehow
  m->swapchain_images =
      (VkImage *)malloc(sizeof(VkImage) * m->swapchain_images_count);
  vkGetSwapchainImagesKHR(m->device, m->swapchain, &m->swapchain_images_count,
                          m->swapchain_images);
  printf("created %d swapchain images\n", m->swapchain_images_count);
}

void my_vk_create_queues(MyVk *m) {
  // get queue
  vkGetDeviceQueue(m->device, m->queue_graphics_idx, 0, &m->graphicsQueue);
  vkGetDeviceQueue(m->device, m->queue_present_idx, 0, &m->presentQueue);
}

void my_vk_create_image_views(MyVk *m) {
  // create image views
  m->image_views =
      (VkImageView *)malloc(sizeof(VkImageView) * m->swapchain_images_count);
  for (uint32_t i = 0; i < m->swapchain_images_count; ++i) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m->swapchain_images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m->format.format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m->device, &createInfo, nullptr,
                          &m->image_views[i]) != VK_SUCCESS) {
      printf("ERROR: could'nt create image view!");
    }
  }
}

void my_vk_create_shader_modules(MyVk *m) {
  // load spirv .spv files
  m->vert_shader_module = create_shader_module(m->device, "shaders/vert.spv");
  m->frag_shader_module = create_shader_module(m->device, "shaders/frag.spv");

  // shader stage creation
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // vert shader
  vertShaderStageInfo.module = m->vert_shader_module;
  vertShaderStageInfo.pName = "main";
  vertShaderStageInfo.pSpecializationInfo = nullptr; // special constants
                                                     //
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // vert shader
  fragShaderStageInfo.module = m->frag_shader_module;
  fragShaderStageInfo.pName = "main";
  fragShaderStageInfo.pSpecializationInfo = nullptr; // special constants

  m->shaderStages[0] = vertShaderStageInfo;
  m->shaderStages[1] = fragShaderStageInfo;
}

void my_vk_create_dynamic_state(MyVk *m) {
  // dynamic state
  m->dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
  m->dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

  m->dstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  m->dstate.dynamicStateCount = m->dynamicStateCount;
  m->dstate.pDynamicStates = m->dynamicStates;

  m->viewport.x = 0.f;
  m->viewport.y = 0.f;
  m->viewport.width = static_cast<float>(m->extent.width);
  m->viewport.height = static_cast<float>(m->extent.height);
  m->viewport.minDepth = 0.f; // standard depth values
  m->viewport.maxDepth = 1.0f;

  m->scissor.offset = {0, 0};
  m->scissor.extent = m->extent;

  m->viewportState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  m->viewportState.viewportCount = 1;
  m->viewportState.pViewports = &m->viewport;
  m->viewportState.scissorCount = 1;
  m->viewportState.pScissors = &m->scissor;
}

void my_vk_create_render_pipeline(MyVk *m) {
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
  {
    VkPipelineLayoutCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeInfo.setLayoutCount = 0;
    pipeInfo.pSetLayouts = nullptr;
    pipeInfo.pushConstantRangeCount = 0;
    pipeInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m->device, &pipeInfo, nullptr,
                               &m->pipelineLayout) != VK_SUCCESS) {
      printf("ERROR: failed to create pipeline layout!\n");
    }
  }

  // render passes
  {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m->format.format;
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

    VkSubpassDependency dependency{};
    {
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // before
      dependency.dstSubpass = 0;                   // this one
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.srcAccessMask = 0;
      dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m->device, &renderPassInfo, nullptr,
                           &m->renderPass) != VK_SUCCESS) {
      printf("ERROR: could not create render pass!\n");
    }
  }

  // create graphics pipeline
  {
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; // vert and frag
    pipelineInfo.pStages = m->shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &m->viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &m->dstate;

    pipelineInfo.layout = m->pipelineLayout; // lives longer
    pipelineInfo.renderPass = m->renderPass;
    pipelineInfo.subpass = 0; // idx of subpass that renders
                              //
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // derive from
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m->device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr,
                                  &m->graphicsPipeline) != VK_SUCCESS) {
      printf("ERROR: could not create graphics pipeline!\n");
    }
    // destroy shader modules
    vkDestroyShaderModule(m->device, m->frag_shader_module, nullptr);
    vkDestroyShaderModule(m->device, m->vert_shader_module, nullptr);
  }
}

void my_vk_create_swapchain_framebuffers(MyVk *m) {
  // create VkFramebuffer objects
  m->swapchainFramebuffers = (VkFramebuffer *)malloc(sizeof(VkFramebuffer) *
                                                     m->swapchain_images_count);

  for (uint32_t i = 0; i < m->swapchain_images_count; ++i) {

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m->renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &m->image_views[i];
    framebufferInfo.width = m->extent.width;
    framebufferInfo.height = m->extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m->device, &framebufferInfo, nullptr,
                            &m->swapchainFramebuffers[i]) != VK_SUCCESS) {
      printf("ERROR: failed to create nbr %i framebuffer!\n", i);
    }
  }
}

void my_vk_create_command_pool(MyVk *m) {
  // create command pool, where we can have buffers later
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  // allow command buffers to be rerecorded individually
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m->queue_graphics_idx;

  if (vkCreateCommandPool(m->device, &poolInfo, nullptr, &m->commandPool) !=
      VK_SUCCESS) {
    printf("ERROR: could not create command pool for graphics\n");
  }
}

void my_vk_create_command_buffers(MyVk *m) {
  // Create command buffer
  m->commandBuffers =
      (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m->commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  if (vkAllocateCommandBuffers(m->device, &allocInfo, m->commandBuffers) !=
      VK_SUCCESS) {
    printf("ERROR: failed to allocate commmand buffers in pool\n");
  }
}

void my_vk_create_semaphores(MyVk *m) {
  // create semaphores
  m->imageAvailableSemaphores =
      (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  m->renderFinishedSemaphores =
      (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  m->inFlightFences = (VkFence *)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);

  // reuse these create info structs
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {

    if (vkCreateSemaphore(m->device, &semaphoreInfo, nullptr,
                          &m->imageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(m->device, &semaphoreInfo, nullptr,
                          &m->renderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m->device, &fenceInfo, nullptr, &m->inFlightFences[i]) !=
            VK_SUCCESS) {
      printf("ERROR: could not create semaphores!\n");
    }
  }
}

void my_vk_deinit_swapchain(MyVk *m) {
  for (uint32_t i = 0; i < m->swapchain_images_count; ++i) {
    vkDestroyFramebuffer(m->device, m->swapchainFramebuffers[i], nullptr);
  }

  for (uint32_t i = 0; i < m->swapchain_images_count; ++i) {
    vkDestroyImageView(m->device, m->image_views[i], nullptr);
  }
  vkDestroySwapchainKHR(m->device, m->swapchain, nullptr);
  free(m->swapchainFramebuffers);
  free(m->image_views);
}

void my_vk_recreate_swapchain(MyVk *m) {

  int width = 0, height = 0;
  glfwGetFramebufferSize(m->window, &width, &height);
  while (width == 0 || height == 0) { // handle minimization
    glfwGetFramebufferSize(m->window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(m->device);

  my_vk_deinit_swapchain(m);

  my_vk_create_swapchain(m);
  my_vk_create_image_views(m);
  my_vk_create_swapchain_framebuffers(m);
}

void my_vk_draw(MyVk *m) {
  // draw
  // wait for the previous frame to be rendered
  vkWaitForFences(m->device, 1, &m->inFlightFences[m->currentFrame], VK_TRUE,
                  UINT64_MAX);

  // get image from swapchain
  uint32_t imageIndex;
  {

    VkResult res =
        vkAcquireNextImageKHR(m->device, m->swapchain, UINT64_MAX,
                              m->imageAvailableSemaphores[m->currentFrame],
                              VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
      // recreate the swapchain
      m->framebuffer_resized = false;
      my_vk_recreate_swapchain(m);
      return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
      printf("ERROR: Failed to acquire swap chain image!\n");
    }
  }
  vkResetFences(m->device, 1, &m->inFlightFences[m->currentFrame]);

  // record command buffer
  vkResetCommandBuffer(m->commandBuffers[m->currentFrame], 0);
  // record to command buffer
  uint32_t cur_frame_buffer = imageIndex;
  {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL;
    if (vkBeginCommandBuffer(m->commandBuffers[m->currentFrame], &beginInfo) !=
        VK_SUCCESS) {
      printf("ERROR: could not begin command buffer\n");
    }

    // begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m->renderPass;
    renderPassInfo.framebuffer = m->swapchainFramebuffers[cur_frame_buffer];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m->extent;

    VkClearValue clearColor = {
        {{(float)fabs(sin(glfwGetTime())), 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m->commandBuffers[m->currentFrame], &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m->commandBuffers[m->currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, m->graphicsPipeline);

    vkCmdSetViewport(m->commandBuffers[m->currentFrame], 0, 1, &m->viewport);
    vkCmdSetScissor(m->commandBuffers[m->currentFrame], 0, 1, &m->scissor);

    vkCmdDraw(m->commandBuffers[m->currentFrame], 3, 1, 0, 0);

    vkCmdEndRenderPass(m->commandBuffers[m->currentFrame]);

    if (vkEndCommandBuffer(m->commandBuffers[m->currentFrame]) != VK_SUCCESS) {
      printf("ERROR: failed to end comman buffer!\n");
    }
  }
  // submit command buffer
  VkSubmitInfo submitInfo{};
  {
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitFlag =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m->imageAvailableSemaphores[m->currentFrame];
    submitInfo.pWaitDstStageMask = &waitFlag;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m->commandBuffers[m->currentFrame];
    // make the render signal when it's done rendering
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores =
        &m->renderFinishedSemaphores[m->currentFrame];
    if (vkQueueSubmit(m->graphicsQueue, 1, &submitInfo,
                      m->inFlightFences[m->currentFrame]) != VK_SUCCESS) {
      printf("ERROR: Could not submit command buffer to command graphics "
             "queue!\n");
    }
  }
  VkPresentInfoKHR presentInfo{};
  {
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m->renderFinishedSemaphores[m->currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m->swapchain;
    presentInfo.pImageIndices = &imageIndex;
  }

  VkResult res = vkQueuePresentKHR(m->presentQueue, &presentInfo);
  // TODO : adding the out commented check made it change swapchain twice for
  // every resize, fix somehow? this solution will possibly not work on all
  // devices
  if (res == VK_ERROR_OUT_OF_DATE_KHR ||
      res == VK_SUBOPTIMAL_KHR /* || m->framebuffer_resized  */) {
    // recreate the swapchain
    m->framebuffer_resized = false;
    my_vk_recreate_swapchain(m);
    return;
  } else if (res != VK_SUCCESS) {
    printf("ERROR: failed to queue present KHR!\n");
  }

  m->currentFrame = (m->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int main() {
  glfwInit();

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

  MyVk my_vk{};
  MyVk *m = &my_vk;

  my_vk_create_window(m);
  my_vk_create_instance(m);
  my_vk_create_surface(m);
  my_vk_create_phys_device(m);
  my_vk_get_queue_indices(m);
  my_vk_create_device(m);
  my_vk_create_swapchain(m);
  my_vk_create_queues(m);
  my_vk_create_image_views(m);
  my_vk_create_shader_modules(m);
  my_vk_create_dynamic_state(m);
  my_vk_create_render_pipeline(m);
  my_vk_create_swapchain_framebuffers(m);
  my_vk_create_command_pool(m);
  my_vk_create_command_buffers(m);
  my_vk_create_semaphores(m);

  glm::mat4 matrix;
  glm::vec4 vec;
  auto test = matrix * vec;

  while (!glfwWindowShouldClose(m->window)) {
    glfwPollEvents();

    my_vk_draw(m);
  }
  // EXIT
  vkDeviceWaitIdle(m->device);

  my_vk_deinit_swapchain(m);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(m->device, m->imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(m->device, m->renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(m->device, m->inFlightFences[i], nullptr);
  }
  vkDestroyCommandPool(m->device, m->commandPool, nullptr);
  vkDestroyPipeline(m->device, m->graphicsPipeline, nullptr);
  vkDestroyRenderPass(m->device, m->renderPass, nullptr);
  vkDestroyPipelineLayout(m->device, m->pipelineLayout, nullptr);
  vkDestroyDevice(m->device, nullptr);
  vkDestroySurfaceKHR(m->instance, m->surface, nullptr);
  vkDestroyInstance(m->instance, nullptr);
  glfwDestroyWindow(m->window);
  glfwTerminate();

  return 0;
}
