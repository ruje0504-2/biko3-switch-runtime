#include "render/renderer.h"
#include "core/matrix.h"
#ifdef __SWITCH__
#include <switch.h>
#define VK_USE_PLATFORM_VI_NN
#endif
#include "shaders.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#define MAX_IMAGES 8
#define VERTEX_CAPACITY (1024 * 1024)
#define VK_TRY(call)                                                           \
  do {                                                                         \
    VkResult vr_ = (call);                                                     \
    if (vr_ != VK_SUCCESS) {                                                   \
      snprintf(error, 256, "%s: VkResult %d", #call, (int)vr_);                \
      goto fail;                                                               \
    }                                                                          \
  } while (0)

typedef struct {
  VkBuffer handle;
  VkDeviceMemory memory;
  void *mapped;
  VkDeviceSize allocation;
  int coherent;
} Buffer;
typedef struct {
  VkImage handle;
  VkImageView view;
  VkDeviceMemory memory;
  VkDeviceSize allocation;
} Image;
struct BkTexture {
  BkRenderer *owner;
  uint32_t sort_key, width, height;
  Image image;
  VkDescriptorSet descriptor;
  Buffer upload;
  struct BkTexture *next_upload;
  int pending_upload;
};
struct BkGpuMesh {
  BkRenderer *owner;
  Buffer vertices, indices;
  unsigned index_count, vertex_count, references;
  int lit;
  BkSkinPalette *palette;
  Buffer skin_source, skin_weights;
  VkDescriptorSet skin_descriptor;
  struct BkGpuMesh *next_skin, *next_palette;
  int skin_pending, skin_replay; /* replay retains completed draw output */
};
struct BkSkinPalette {
  BkRenderer *owner;
  Buffer matrices;
  unsigned count, references;
  uint8_t *used;
  BkGpuMesh *meshes;
};
struct BkLightSet {
  BkRenderer *owner;
  Buffer uniform;
  VkDescriptorSet descriptor;
};
struct BkRenderer {
  VkInstance instance;
  VkPhysicalDevice physical;
  VkDevice device;
  VkQueue queue;
  uint32_t family, width, height, image_count, index, texture_serial;
  VkFormat format;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage images[MAX_IMAGES];
  VkImageView views[MAX_IMAGES];
  VkFramebuffer frames[MAX_IMAGES];
  Image offscreen, depth;
  VkRenderPass pass, resume_pass;
  VkPipeline pipelines[2][BK_BLEND_COUNT][2][2];
  VkPipelineLayout layout;
  VkDescriptorSetLayout descriptor_layout, lighting_layout;
  VkDescriptorPool descriptors;
  VkSampler samplers[2];
  VkCommandPool command_pool;
  VkCommandBuffer command;
  VkFence fence;
  VkSemaphore acquired, finished[MAX_IMAGES];
  Buffer vertices, readback;
  unsigned vertex_count;
  int active, submitted, rendered, acquire_pending;
  BkViewport viewport;
  FILE *log;
  BkRenderStats stats;
  BkTexture *texture_uploads;
  BkGpuMesh *skin_updates;
  VkDescriptorSetLayout skin_descriptor_layout;
  VkPipelineLayout skin_layout;
  VkPipeline skin_pipeline;
  VkDescriptorSetLayout transfer_descriptor_layout;
  VkPipelineLayout transfer_layout;
  VkPipeline transfer_pipeline;
  VkPipeline bound_pipeline;
  VkDescriptorSet bound_descriptors[2];
  VkBuffer bound_vertices, bound_indices;
};
/* All graphics pipelines share r->layout. These bindings belong to one
 * command-buffer recording; compute has independent pipeline/descriptor
 * bindings. A screenshot starts a new recording and must reset them too. */
static void reset_graphics_bindings(BkRenderer *r) {
  r->bound_pipeline = VK_NULL_HANDLE;
  r->bound_descriptors[0] = r->bound_descriptors[1] = VK_NULL_HANDLE;
  r->bound_vertices = r->bound_indices = VK_NULL_HANDLE;
}
static void bind_graphics_pipeline(BkRenderer *r, VkPipeline pipeline) {
  if (r->bound_pipeline != pipeline) {
    vkCmdBindPipeline(r->command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    r->bound_pipeline = pipeline;
  }
}
static void bind_graphics_descriptor(BkRenderer *r, unsigned set,
                                      VkDescriptorSet descriptor) {
  if (r->bound_descriptors[set] != descriptor) {
    vkCmdBindDescriptorSets(r->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->layout, set, 1, &descriptor, 0, NULL);
    r->bound_descriptors[set] = descriptor;
  }
}
static void bind_graphics_vertices(BkRenderer *r, VkBuffer buffer) {
  if (r->bound_vertices != buffer) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(r->command, 0, 1, &buffer, &offset);
    r->bound_vertices = buffer;
  }
}
static void bind_graphics_indices(BkRenderer *r, VkBuffer buffer) {
  if (r->bound_indices != buffer) {
    vkCmdBindIndexBuffer(r->command, buffer, 0, VK_INDEX_TYPE_UINT16);
    r->bound_indices = buffer;
  }
}
static double performance_seconds(void) {
#ifdef __SWITCH__
  return (double)armTicksToNs(armGetSystemTick()) * 1e-9;
#else
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
#endif
}
BkRenderStats bk_renderer_stats(const BkRenderer *r) {
  return r ? r->stats : (BkRenderStats){0};
}
const float bk_identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

static int memory_type(BkRenderer *r, uint32_t bits, VkMemoryPropertyFlags need,
                       uint32_t *type, int *coherent) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(r->physical, &props);
  for (unsigned i = 0; i < props.memoryTypeCount; i++)
    if ((bits & (1U << i)) &&
        (props.memoryTypes[i].propertyFlags & need) == need) {
      *type = i;
      if (coherent)
        *coherent = !!(props.memoryTypes[i].propertyFlags &
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      return 1;
    }
  return 0;
}
static void buffer_destroy(BkRenderer *r, Buffer *b) {
  if (b->mapped)
    vkUnmapMemory(r->device, b->memory);
  if (b->handle)
    vkDestroyBuffer(r->device, b->handle, NULL);
  if (b->memory) {
    vkFreeMemory(r->device, b->memory, NULL);
    r->stats.live_allocations--;
    r->stats.live_bytes -= b->allocation;
  }
  memset(b, 0, sizeof(*b));
}
static void memory_allocated(BkRenderer *r, VkDeviceSize bytes) {
  r->stats.live_allocations++;
  r->stats.live_bytes += bytes;
  if (r->stats.live_allocations > r->stats.peak_allocations)
    r->stats.peak_allocations = r->stats.live_allocations;
  if (r->stats.live_bytes > r->stats.peak_bytes)
    r->stats.peak_bytes = r->stats.live_bytes;
}
static int buffer_create(BkRenderer *r, Buffer *b, VkDeviceSize size,
                         VkBufferUsageFlags usage, char error[256]) {
  VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = size,
                           .usage = usage,
                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  VK_TRY(vkCreateBuffer(r->device, &ci, NULL, &b->handle));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(r->device, b->handle, &req);
  uint32_t type;
  if (!memory_type(r, req.memoryTypeBits,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   &type, &b->coherent) &&
      !memory_type(r, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                   &type, &b->coherent)) {
    snprintf(error, 256, "no host-visible Vulkan buffer memory");
    goto fail;
  }
  VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
  b->allocation = req.size;
  VK_TRY(vkAllocateMemory(r->device, &ai, NULL, &b->memory));
  memory_allocated(r, req.size);
  VK_TRY(vkBindBufferMemory(r->device, b->handle, b->memory, 0));
  VK_TRY(vkMapMemory(r->device, b->memory, 0, VK_WHOLE_SIZE, 0, &b->mapped));
  return 1;
fail:
  buffer_destroy(r, b);
  return 0;
}
static VkResult buffer_flush(BkRenderer *r, Buffer *b, int invalidate) {
  if (b->coherent)
    return VK_SUCCESS;
  VkMappedMemoryRange range = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                               .memory = b->memory,
                               .size = VK_WHOLE_SIZE};
  return invalidate ? vkInvalidateMappedMemoryRanges(r->device, 1, &range)
                    : vkFlushMappedMemoryRanges(r->device, 1, &range);
}
static void image_destroy(BkRenderer *r, Image *im) {
  if (im->view)
    vkDestroyImageView(r->device, im->view, NULL);
  if (im->handle)
    vkDestroyImage(r->device, im->handle, NULL);
  if (im->memory) {
    vkFreeMemory(r->device, im->memory, NULL);
    r->stats.live_allocations--;
    r->stats.live_bytes -= im->allocation;
  }
  memset(im, 0, sizeof(*im));
}
static int image_create(BkRenderer *r, Image *im, unsigned w, unsigned h,
                        VkFormat format, VkImageUsageFlags usage,
                        VkImageAspectFlags aspect, char error[256]) {
  VkImageCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                          .imageType = VK_IMAGE_TYPE_2D,
                          .format = format,
                          .extent = {w, h, 1},
                          .mipLevels = 1,
                          .arrayLayers = 1,
                          .samples = VK_SAMPLE_COUNT_1_BIT,
                          .tiling = VK_IMAGE_TILING_OPTIMAL,
                          .usage = usage,
                          .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  VK_TRY(vkCreateImage(r->device, &ci, NULL, &im->handle));
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(r->device, im->handle, &req);
  uint32_t type;
  if (!memory_type(r, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   &type, NULL) &&
      !memory_type(r, req.memoryTypeBits, 0, &type, NULL)) {
    snprintf(error, 256, "no Vulkan image memory");
    goto fail;
  }
  VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
  VK_TRY(vkAllocateMemory(r->device, &ai, NULL, &im->memory));
  im->allocation = req.size;
  memory_allocated(r, req.size);
  VK_TRY(vkBindImageMemory(r->device, im->handle, im->memory, 0));
  VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                              .image = im->handle,
                              .viewType = VK_IMAGE_VIEW_TYPE_2D,
                              .format = format,
                              .subresourceRange = {aspect, 0, 1, 0, 1}};
  VK_TRY(vkCreateImageView(r->device, &vi, NULL, &im->view));
  return 1;
fail:
  image_destroy(r, im);
  return 0;
}
static void barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout before,
                    VkImageLayout after, VkAccessFlags source,
                    VkAccessFlags dest, VkPipelineStageFlags from,
                    VkPipelineStageFlags to) {
  VkImageMemoryBarrier b = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = source,
      .dstAccessMask = dest,
      .oldLayout = before,
      .newLayout = after,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, from, to, 0, 0, NULL, 0, NULL, 1, &b);
}
static int wait_frame(BkRenderer *r, char error[256]) {
  if (r->submitted) {
    double start = performance_seconds();
    VK_TRY(vkWaitForFences(r->device, 1, &r->fence, VK_TRUE, UINT64_MAX));
    r->stats.fence_seconds += performance_seconds() - start;
    r->submitted = 0;
  }
  return 1;
fail:
  return 0;
}
#include "render/vulkan/skin_internal.h"
#include "render/vulkan/transfer_internal.h"

static int extension(VkExtensionProperties *p, uint32_t n, const char *name) {
  for (uint32_t i = 0; i < n; i++)
    if (!strcmp(p[i].extensionName, name))
      return 1;
  return 0;
}

BkRenderer *bk_renderer_create(unsigned width, unsigned height, FILE *log,
                               char error[256]) {
  BkRenderer *r = calloc(1, sizeof(*r));
  if (!r) {
    snprintf(error, 256, "renderer allocation failed");
    return NULL;
  }
  VkExtensionProperties *extensions = NULL;
  VkPhysicalDevice *devices = NULL;
  VkQueueFamilyProperties *families = NULL;
  VkSurfaceFormatKHR *formats = NULL;
  VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE,
                 lit_vert = VK_NULL_HANDLE, lit_frag = VK_NULL_HANDLE;
  r->width = width;
  r->height = height;
  r->format = VK_FORMAT_R8G8B8A8_UNORM;
  r->log = log ? log : stderr;
  uint32_t n = 0;
  VK_TRY(vkEnumerateInstanceExtensionProperties(NULL, &n, NULL));
  extensions = calloc(n ? n : 1, sizeof(*extensions));
  if (!extensions)
    goto oom;
  VK_TRY(vkEnumerateInstanceExtensionProperties(NULL, &n, extensions));
  const char *iext[3];
  uint32_t in = 0;
  VkInstanceCreateFlags iflags = 0;
#ifdef __SWITCH__
  iext[in++] = VK_KHR_SURFACE_EXTENSION_NAME;
  iext[in++] = VK_NN_VI_SURFACE_EXTENSION_NAME;
#else
  if (extension(extensions, n, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    iext[in++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    iflags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }
#endif
  for (uint32_t i = 0; i < in; i++)
    if (!extension(extensions, n, iext[i])) {
      snprintf(error, 256, "missing instance extension %s", iext[i]);
      goto fail;
    }
  free(extensions);
  extensions = NULL;
  VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                           .pApplicationName = "Biko3 native port bring-up",
                           .applicationVersion = 1,
                           .pEngineName = "bk3",
                           .engineVersion = 1,
                           .apiVersion = VK_API_VERSION_1_2};
  VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                              .pApplicationInfo = &app,
                              .flags = iflags,
                              .enabledExtensionCount = in,
                              .ppEnabledExtensionNames = iext};
  VK_TRY(vkCreateInstance(&ici, NULL, &r->instance));
#ifdef __SWITCH__
  VkViSurfaceCreateInfoNN sci = {
      .sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN,
      .window = nwindowGetDefault()};
  VK_TRY(vkCreateViSurfaceNN(r->instance, &sci, NULL, &r->surface));
#endif
  n = 0;
  VK_TRY(vkEnumeratePhysicalDevices(r->instance, &n, NULL));
  if (!n) {
    snprintf(error, 256, "no Vulkan physical device");
    goto fail;
  }
  devices = calloc(n, sizeof(*devices));
  if (!devices)
    goto oom;
  VK_TRY(vkEnumeratePhysicalDevices(r->instance, &n, devices));
  for (uint32_t d = 0; d < n && !r->physical; d++) {
    uint32_t fn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &fn, NULL);
    families = calloc(fn ? fn : 1, sizeof(*families));
    if (!families)
      goto oom;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &fn, families);
    for (uint32_t f = 0; f < fn; f++)
      if (families[f].queueCount &&
          ((families[f].queueFlags &
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
           (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
        VkBool32 present = VK_TRUE;
        if (r->surface)
          VK_TRY(vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], f, r->surface,
                                                      &present));
        if (present) {
          r->physical = devices[d];
          r->family = f;
          break;
        }
      }
    free(families);
    families = NULL;
  }
  free(devices);
  devices = NULL;
  if (!r->physical) {
    snprintf(error, 256, "no graphics/presentation queue");
    goto fail;
  }
  VkPhysicalDeviceDriverProperties driver = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
  VkPhysicalDeviceProperties2 props = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &driver};
  vkGetPhysicalDeviceProperties2(r->physical, &props);
  fprintf(r->log, "Vulkan device: %s; driver: %s (%s), ID %u\n",
          props.properties.deviceName, driver.driverName, driver.driverInfo,
          driver.driverID);
  fflush(r->log);
#ifdef __SWITCH__
  if (driver.driverID != VK_DRIVER_ID_MESA_NVK) {
    snprintf(error, 256, "expected Mesa NVK; got driver ID %u",
             driver.driverID);
    goto fail;
  }
#endif
  n = 0;
  VK_TRY(vkEnumerateDeviceExtensionProperties(r->physical, NULL, &n, NULL));
  extensions = calloc(n ? n : 1, sizeof(*extensions));
  if (!extensions)
    goto oom;
  VK_TRY(
      vkEnumerateDeviceExtensionProperties(r->physical, NULL, &n, extensions));
  const char *dext[2];
  uint32_t dn = 0;
  if (r->surface)
    dext[dn++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  if (extension(extensions, n, "VK_KHR_portability_subset"))
    dext[dn++] = "VK_KHR_portability_subset";
  for (uint32_t i = 0; i < dn; i++)
    if (!extension(extensions, n, dext[i])) {
      snprintf(error, 256, "missing device extension %s", dext[i]);
      goto fail;
    }
  free(extensions);
  extensions = NULL;
  float priority = 1;
  VkDeviceQueueCreateInfo qi = {.sType =
                                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                .queueFamilyIndex = r->family,
                                .queueCount = 1,
                                .pQueuePriorities = &priority};
  VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                           .queueCreateInfoCount = 1,
                           .pQueueCreateInfos = &qi,
                           .enabledExtensionCount = dn,
                           .ppEnabledExtensionNames = dext};
  VK_TRY(vkCreateDevice(r->physical, &di, NULL, &r->device));
  vkGetDeviceQueue(r->device, r->family, 0, &r->queue);
  if (r->surface) {
    VkSurfaceCapabilitiesKHR caps;
    VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->physical, r->surface,
                                                     &caps));
    n = 0;
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical, r->surface, &n,
                                                NULL));
    if (!n) {
      snprintf(error, 256, "no surface formats");
      goto fail;
    }
    formats = calloc(n, sizeof(*formats));
    if (!formats)
      goto oom;
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical, r->surface, &n,
                                                formats));
    uint32_t fi = UINT32_MAX;
    for (uint32_t i = 0; i < n; i++)
      if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
          formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
        fi = i;
        break;
      }
    if (fi == UINT32_MAX) {
      snprintf(error, 256, "no UNORM presentation format");
      goto fail;
    }
    r->format = formats[fi].format;
    if (caps.currentExtent.width != UINT32_MAX) {
      r->width = caps.currentExtent.width;
      r->height = caps.currentExtent.height;
    } else {
      if (r->width < caps.minImageExtent.width)
        r->width = caps.minImageExtent.width;
      if (r->width > caps.maxImageExtent.width)
        r->width = caps.maxImageExtent.width;
      if (r->height < caps.minImageExtent.height)
        r->height = caps.minImageExtent.height;
      if (r->height > caps.maxImageExtent.height)
        r->height = caps.maxImageExtent.height;
    }
    uint32_t count = caps.minImageCount;
    if (count < 3)
      count = 3;
    if (caps.maxImageCount && count > caps.maxImageCount)
      count = caps.maxImageCount;
    if ((caps.supportedUsageFlags & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
        !(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
      snprintf(error, 256, "unsupported surface capabilities");
      goto fail;
    }
    VkSwapchainCreateInfoKHR si = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = r->surface,
        .minImageCount = count,
        .imageFormat = r->format,
        .imageColorSpace = formats[fi].colorSpace,
        .imageExtent = {r->width, r->height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE};
    VK_TRY(vkCreateSwapchainKHR(r->device, &si, NULL, &r->swapchain));
    free(formats);
    formats = NULL;
    VK_TRY(vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->image_count,
                                   NULL));
    if (r->image_count > MAX_IMAGES) {
      snprintf(error, 256, "too many swapchain images");
      goto fail;
    }
    VK_TRY(vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->image_count,
                                   r->images));
    for (uint32_t i = 0; i < r->image_count; i++) {
      VkImageViewCreateInfo vi = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = r->images[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = r->format,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      VK_TRY(vkCreateImageView(r->device, &vi, NULL, &r->views[i]));
    }
  } else {
    if (!image_create(r, &r->offscreen, width, height, r->format,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                      VK_IMAGE_ASPECT_COLOR_BIT, error))
      goto fail;
    r->images[0] = r->offscreen.handle;
    r->views[0] = r->offscreen.view;
    r->image_count = 1;
  }
  if (!buffer_create(r, &r->readback, (VkDeviceSize)r->width * r->height * 4,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, error))
    goto fail;
  if (!image_create(r, &r->depth, r->width, r->height, VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT, error))
    goto fail;
  VkAttachmentDescription attachments[2] = {
      {.format = r->format,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      {.format = VK_FORMAT_D32_SFLOAT,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
  VkAttachmentReference color = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                        depth = {
                            1,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub = {.pipelineBindPoint =
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                              .colorAttachmentCount = 1,
                              .pColorAttachments = &color,
                              .pDepthStencilAttachment = &depth};
  VkSubpassDependency dep = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
  VkRenderPassCreateInfo pi = {.sType =
                                   VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                               .attachmentCount = 2,
                               .pAttachments = attachments,
                               .subpassCount = 1,
                               .pSubpasses = &sub,
                               .dependencyCount = 1,
                               .pDependencies = &dep};
  VK_TRY(vkCreateRenderPass(r->device, &pi, NULL, &r->pass));
  /* Load/store-only differences are render-pass compatible with pipelines
   * and framebuffers created for the initial clear pass. */
  attachments[0].loadOp = attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachments[1].initialLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VK_TRY(vkCreateRenderPass(r->device, &pi, NULL, &r->resume_pass));
  for (uint32_t i = 0; i < r->image_count; i++) {
    VkImageView av[] = {r->views[i], r->depth.view};
    VkFramebufferCreateInfo fi = {.sType =
                                      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                  .renderPass = r->pass,
                                  .attachmentCount = 2,
                                  .pAttachments = av,
                                  .width = r->width,
                                  .height = r->height,
                                  .layers = 1};
    VK_TRY(vkCreateFramebuffer(r->device, &fi, NULL, &r->frames[i]));
  }
  VkDescriptorSetLayoutBinding binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
  VkDescriptorSetLayoutCreateInfo dl = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding};
  VK_TRY(
      vkCreateDescriptorSetLayout(r->device, &dl, NULL, &r->descriptor_layout));
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  VK_TRY(
      vkCreateDescriptorSetLayout(r->device, &dl, NULL, &r->lighting_layout));
  VkDescriptorPoolSize pools[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096}};
  VkDescriptorPoolCreateInfo dp = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1568,
      .poolSizeCount = 3,
      .pPoolSizes = pools};
  VK_TRY(vkCreateDescriptorPool(r->device, &dp, NULL, &r->descriptors));
  if (!skin_pipeline_create(r, error))
    goto fail;
  VkSamplerCreateInfo sc = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .maxLod = 0};
  VK_TRY(vkCreateSampler(r->device, &sc, NULL, &r->samplers[BK_WRAP_CLAMP]));
  sc.addressModeU = sc.addressModeV = sc.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_TRY(vkCreateSampler(r->device, &sc, NULL, &r->samplers[BK_WRAP_REPEAT]));
  VkPushConstantRange push = {VK_SHADER_STAGE_VERTEX_BIT, 0, 128};
  VkDescriptorSetLayout layouts[] = {r->descriptor_layout, r->lighting_layout};
  VkPipelineLayoutCreateInfo li = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 2,
      .pSetLayouts = layouts,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push};
  VK_TRY(vkCreatePipelineLayout(r->device, &li, NULL, &r->layout));
  VkShaderModuleCreateInfo sm = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(bk_vert_spv),
      .pCode = bk_vert_spv};
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &vert));
  sm.codeSize = sizeof(bk_frag_spv);
  sm.pCode = bk_frag_spv;
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &frag));
  sm.codeSize = sizeof(bk_lit_vert_spv);
  sm.pCode = bk_lit_vert_spv;
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &lit_vert));
  sm.codeSize = sizeof(bk_lit_frag_spv);
  sm.pCode = bk_lit_frag_spv;
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &lit_frag));
  VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vert,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = frag,
       .pName = "main"}};
  VkVertexInputBindingDescription vb = {0, sizeof(BkVertex),
                                        VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription va[] = {
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(BkVertex, x)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(BkVertex, u)},
      {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BkVertex, r)},
      {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(BkLitVertex, normal)},
      {4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(BkLitVertex, ambient)},
      {5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(BkLitVertex, emissive)},
      {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BkLitVertex, specular)}};
  VkPipelineVertexInputStateCreateInfo vis = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vb,
      .vertexAttributeDescriptionCount = 3,
      .pVertexAttributeDescriptions = va};
  VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineViewportStateCreateInfo vp = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};
  VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1};
  VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineDepthStencilStateCreateInfo ds = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL};
  VkPipelineColorBlendAttachmentState blend = {
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = 15};
  VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend};
  VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn};
  VkGraphicsPipelineCreateInfo gp = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vis,
      .pInputAssemblyState = &ia,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pDepthStencilState = &ds,
      .pColorBlendState = &cb,
      .pDynamicState = &dynamic,
      .layout = r->layout,
      .renderPass = r->pass};
  for (unsigned lit = 0; lit < 2; lit++) {
    stages[0].module = lit ? lit_vert : vert;
    stages[1].module = lit ? lit_frag : frag;
    vb.stride = lit ? sizeof(BkLitVertex) : sizeof(BkVertex);
    vis.vertexAttributeDescriptionCount = lit ? 7 : 3;
    for (unsigned mode = 0; mode < BK_BLEND_COUNT; mode++) {
      blend.blendEnable = mode != BK_BLEND_OPAQUE;
      blend.srcColorBlendFactor = mode == BK_BLEND_INVERSE_COLOR
                                      ? VK_BLEND_FACTOR_ZERO
                                      : VK_BLEND_FACTOR_SRC_ALPHA;
      blend.dstColorBlendFactor = mode == BK_BLEND_ADDITIVE
                                      ? VK_BLEND_FACTOR_ONE
                                  : mode == BK_BLEND_INVERSE_COLOR
                                      ? VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
                                      : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend.srcAlphaBlendFactor = mode == BK_BLEND_UI_ALPHA
                                      ? VK_BLEND_FACTOR_ONE
                                      : blend.srcColorBlendFactor;
      blend.dstAlphaBlendFactor = mode == BK_BLEND_INVERSE_COLOR
                                      ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                                      : blend.dstColorBlendFactor;
      for (unsigned write = 0; write < 2; write++) {
        ds.depthWriteEnable = write;
        /* Authored inverse-color shadow layers can be
         * exactly coplanar with a differently triangulated opaque surface.
         * Reverse D32 still needs a small representable-depth offset there;
         * more precision alone cannot make two rasterized planes identical.
         * The slope term covers subpixel plane setup; keep additive lights,
         * ordinary alpha, opaque geometry and UI unbiased. */
        rs.depthBiasEnable = lit && !write &&
            mode == BK_BLEND_INVERSE_COLOR;
        rs.depthBiasConstantFactor = rs.depthBiasEnable ? 2.f : 0.f;
        rs.depthBiasSlopeFactor = rs.depthBiasEnable ? 1.f / 128.f : 0.f;
        for (unsigned cull = 0; cull < 2; cull++) {
          rs.cullMode = cull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
          rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
          VK_TRY(
              vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &gp, NULL,
                                        &r->pipelines[lit][mode][write][cull]));
        }
      }
    }
  }
  vkDestroyShaderModule(r->device, lit_frag, NULL);
  lit_frag = VK_NULL_HANDLE;
  vkDestroyShaderModule(r->device, lit_vert, NULL);
  lit_vert = VK_NULL_HANDLE;
  vkDestroyShaderModule(r->device, vert, NULL);
  vert = VK_NULL_HANDLE;
  vkDestroyShaderModule(r->device, frag, NULL);
  frag = VK_NULL_HANDLE;
  VkCommandPoolCreateInfo cp = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = r->family};
  VK_TRY(vkCreateCommandPool(r->device, &cp, NULL, &r->command_pool));
  VkCommandBufferAllocateInfo ca = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = r->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VK_TRY(vkAllocateCommandBuffers(r->device, &ca, &r->command));
  VkFenceCreateInfo fc = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VK_TRY(vkCreateFence(r->device, &fc, NULL, &r->fence));
  if (r->swapchain) {
    VkSemaphoreCreateInfo se = {.sType =
                                    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_TRY(vkCreateSemaphore(r->device, &se, NULL, &r->acquired));
    for (uint32_t i = 0; i < r->image_count; i++)
      VK_TRY(vkCreateSemaphore(r->device, &se, NULL, &r->finished[i]));
  }
  if (!buffer_create(r, &r->vertices,
                     (VkDeviceSize)VERTEX_CAPACITY * sizeof(BkVertex),
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, error))
    goto fail;
  fprintf(
      r->log,
      "Renderer ready: %ux%u, %u images, explicit Vulkan graphics pipeline\n",
      r->width, r->height, r->image_count);
  fflush(r->log);
  return r;
oom:
  snprintf(error, 256, "renderer allocation failed");
fail:
  free(extensions);
  free(devices);
  free(families);
  free(formats);
  if (lit_frag)
    vkDestroyShaderModule(r->device, lit_frag, NULL);
  if (lit_vert)
    vkDestroyShaderModule(r->device, lit_vert, NULL);
  if (vert)
    vkDestroyShaderModule(r->device, vert, NULL);
  if (frag)
    vkDestroyShaderModule(r->device, frag, NULL);
  bk_renderer_destroy(r);
  return NULL;
}

enum { BK_PACKED_LIGHTING_FLOATS = 4 + BK_MAX_POINT_LIGHTS * 24 };
static void pack_lighting(float *packed, const BkLighting *lighting) {
  memcpy(packed, lighting->ambient, 3 * sizeof(float));
  packed[3] = (float)(lighting->point_count + lighting->spot_count);
  for (unsigned i = 0; i < lighting->point_count + lighting->spot_count; i++) {
    const BkSpotLight *spot = i < lighting->point_count
                                  ? NULL
                                  : &lighting->spots[i - lighting->point_count];
    const BkPointLight *p = spot ? &spot->point : &lighting->points[i];
    float *out = packed + 4 + i * 24;
    memcpy(out, p->position, 3 * sizeof(float));
    out[3] = p->range;
    memcpy(out + 4, p->diffuse, 3 * sizeof(float));
    out[7] = p->attenuation0;
    memcpy(out + 8, p->ambient, 3 * sizeof(float));
    out[11] = p->attenuation1;
    out[12] = p->attenuation2;
    memcpy(out + 13, p->specular, 3 * sizeof(float));
    if (spot) {
      double norm = 0;
      for (unsigned j = 0; j < 3; ++j)
        norm += (double)spot->direction[j] * spot->direction[j];
      norm = sqrt(norm);
      for (unsigned j = 0; j < 3; ++j)
        out[16 + j] = (float)(spot->direction[j] / norm);
      out[19] = spot->falloff;
      out[20] = cosf(spot->theta * .5f);
      out[21] = cosf(spot->phi * .5f);
      out[22] = 1;
    }
  }
}
BkLightSet *bk_light_set_create(BkRenderer *r, const BkLighting *lighting,
                                char error[256]) {
  if (!r || r->active) {
    snprintf(error, 256, "invalid light upload state");
    return NULL;
  }
  if (!bk_lighting_validate(lighting, error) || !wait_frame(r, error))
    return NULL;
  BkLightSet *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(error, 256, "light set allocation failed");
    return NULL;
  }
  s->owner = r;
  /* Explicit std140 packing, independent of public C struct padding. */
  float packed[BK_PACKED_LIGHTING_FLOATS + 4 + 16] = {0};
  pack_lighting(packed, lighting);
  if (!buffer_create(r, &s->uniform, sizeof(packed),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, error))
    goto fail;
  memcpy(s->uniform.mapped, packed, sizeof(packed));
  VK_TRY(buffer_flush(r, &s->uniform, 0));
  VkDescriptorSetAllocateInfo da = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = r->descriptors,
      .descriptorSetCount = 1,
      .pSetLayouts = &r->lighting_layout};
  VK_TRY(vkAllocateDescriptorSets(r->device, &da, &s->descriptor));
  VkDescriptorBufferInfo bi = {s->uniform.handle, 0, sizeof(packed)};
  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = s->descriptor,
                                .dstBinding = 0,
                                .descriptorCount = 1,
                                .descriptorType =
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .pBufferInfo = &bi};
  vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
  return s;
fail:
  bk_light_set_destroy(r, s);
  return NULL;
}
int bk_light_set_update(BkRenderer *r, BkLightSet *s,
                        const BkLighting *lighting, char error[256]) {
  if (!r || r->active || !s || s->owner != r) {
    snprintf(error, 256, "invalid light update state");
    return 0;
  }
  if (!bk_lighting_validate(lighting, error) || !wait_frame(r, error))
    return 0;
  float packed[BK_PACKED_LIGHTING_FLOATS] = {0};
  pack_lighting(packed, lighting);
  memcpy(s->uniform.mapped, packed, sizeof(packed));
  VK_TRY(buffer_flush(r, &s->uniform, 0));
  return 1;
fail:
  return 0;
}
int bk_light_set_view(BkRenderer *r, BkLightSet *s, const float position[3],
                      char error[256]) {
  if (!r || r->active || !s || s->owner != r || !position ||
      !isfinite(position[0]) || !isfinite(position[1]) ||
      !isfinite(position[2])) {
    snprintf(error, 256, "invalid light viewer state/position");
    return 0;
  }
  if (!wait_frame(r, error))
    return 0;
  memcpy((float *)s->uniform.mapped + BK_PACKED_LIGHTING_FLOATS, position,
         3 * sizeof(float));
  VK_TRY(buffer_flush(r, &s->uniform, 0));
  return 1;
fail:
  return 0;
}
int bk_light_set_fog(BkRenderer *r, BkLightSet *s, const BkFog *fog,
                     const float view[16], char error[256]) {
  if (!r || r->active || !s || s->owner != r || !view) {
    snprintf(error, 256, "invalid light fog state/view");
    return 0;
  }
  if (!bk_fog_validate(fog, error))
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(view[i])) {
      snprintf(error, 256, "nonfinite fog view");
      return 0;
    }
  if (!wait_frame(r, error))
    return 0;
  float packed[16] = {((fog->color >> 16) & 255) / 255.f,
                      ((fog->color >> 8) & 255) / 255.f,
                      (fog->color & 255) / 255.f,
                      fog->enabled ? (float)fog->mode : 0,
                      fog->start,
                      fog->end,
                      fog->density,
                      (float)fog->table,
                      view[2],
                      view[6],
                      view[10],
                      view[14],
                      (float)fog->range_based,
                      0,
                      0,
                      0};
  memcpy((float *)s->uniform.mapped + BK_PACKED_LIGHTING_FLOATS + 4, packed,
         sizeof(packed));
  VK_TRY(buffer_flush(r, &s->uniform, 0));
  return 1;
fail:
  return 0;
}
void bk_light_set_destroy(BkRenderer *r, BkLightSet *s) {
  if (!s)
    return;
  if (r->submitted) {
    vkDeviceWaitIdle(r->device);
    r->submitted = 0;
  }
  if (s->descriptor)
    vkFreeDescriptorSets(r->device, r->descriptors, 1, &s->descriptor);
  buffer_destroy(r, &s->uniform);
  free(s);
}
BkTexture *bk_texture_create(BkRenderer *r, const BkImage *im,
                             char error[256]) {
  return bk_texture_create_sampled(r, im, BK_WRAP_CLAMP, error);
}
BkTexture *bk_texture_create_sampled(BkRenderer *r, const BkImage *im,
                                     BkTextureWrap wrap, char error[256]) {
  if (!r || r->active || !im || !im->rgba || !im->width || !im->height ||
      im->width > 16384 || im->height > 16384 ||
      (wrap != BK_WRAP_CLAMP && wrap != BK_WRAP_REPEAT) ||
      r->texture_serial == UINT32_MAX) {
    snprintf(error, 256, "invalid texture upload state/image/wrap");
    return NULL;
  }
  if (!wait_frame(r, error))
    return NULL;
  BkTexture *t = calloc(1, sizeof(*t));
  Buffer upload = {0};
  if (!t) {
    snprintf(error, 256, "texture allocation failed");
    return NULL;
  }
  t->owner = r;
  t->width = im->width;
  t->height = im->height;
  if (!image_create(
          r, &t->image, im->width, im->height, VK_FORMAT_R8G8B8A8_UNORM,
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_ASPECT_COLOR_BIT, error))
    goto fail;
  size_t bytes = (size_t)im->width * im->height * 4;
  if (!buffer_create(r, &upload, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     error))
    goto fail;
  memcpy(upload.mapped, im->rgba, bytes);
  VK_TRY(buffer_flush(r, &upload, 0));
  VK_TRY(vkResetCommandBuffer(r->command, 0));
  VkCommandBufferBeginInfo cb = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  VK_TRY(vkBeginCommandBuffer(r->command, &cb));
  reset_graphics_bindings(r);
  barrier(r->command, t->image.handle, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy copy = {
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageExtent = {im->width, im->height, 1}};
  vkCmdCopyBufferToImage(r->command, upload.handle, t->image.handle,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  barrier(r->command, t->image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  VK_TRY(vkEndCommandBuffer(r->command));
  VK_TRY(vkResetFences(r->device, 1, &r->fence));
  VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                     .commandBufferCount = 1,
                     .pCommandBuffers = &r->command};
  VK_TRY(vkQueueSubmit(r->queue, 1, &si, r->fence));
  r->submitted = 1;
  if (!wait_frame(r, error))
    goto fail;
  buffer_destroy(r, &upload);
  VkDescriptorSetAllocateInfo da = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = r->descriptors,
      .descriptorSetCount = 1,
      .pSetLayouts = &r->descriptor_layout};
  VK_TRY(vkAllocateDescriptorSets(r->device, &da, &t->descriptor));
  VkDescriptorImageInfo ii = {.sampler = r->samplers[wrap],
                              .imageView = t->image.view,
                              .imageLayout =
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = t->descriptor,
                                .dstBinding = 0,
                                .descriptorCount = 1,
                                .descriptorType =
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .pImageInfo = &ii};
  vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
  t->sort_key = ++r->texture_serial;
  return t;
fail:
  if (r->submitted)
    vkDeviceWaitIdle(r->device);
  buffer_destroy(r, &upload);
  bk_texture_destroy(r, t);
  return NULL;
}
uint32_t bk_texture_sort_key(const BkTexture *texture) {
  return texture ? texture->sort_key : 0;
}
int bk_texture_owned_by(const BkTexture *t, const BkRenderer *r) {
  return t && r && t->owner == r;
}
int bk_texture_update(BkRenderer *r, BkTexture *t, const BkImage *im,
                      char error[256]) {
  if (!r || r->active || !t || t->owner != r || !im || !im->rgba ||
      im->width != t->width || im->height != t->height) {
    snprintf(error, 256, "invalid texture update state/image/dimensions");
    return 0;
  }
  if (!wait_frame(r, error))
    return 0;
  size_t bytes = (size_t)t->width * t->height * 4;
  if (!t->upload.handle &&
      !buffer_create(r, &t->upload, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     error))
    return 0;
  memcpy(t->upload.mapped, im->rgba, bytes);
  VK_TRY(buffer_flush(r, &t->upload, 0));
  if (!t->pending_upload) {
    t->next_upload = r->texture_uploads;
    r->texture_uploads = t;
    t->pending_upload = 1;
  }
  r->stats.uploaded_bytes += bytes;
  return 1;
fail:
  return 0;
}
static void record_texture_uploads(BkRenderer *r) {
  while (r->texture_uploads) {
    BkTexture *t = r->texture_uploads;
    barrier(r->command, t->image.handle,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {t->width, t->height, 1}};
    vkCmdCopyBufferToImage(r->command, t->upload.handle, t->image.handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(r->command, t->image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    r->texture_uploads = t->next_upload;
    t->next_upload = NULL;
    t->pending_upload = 0;
  }
}
void bk_texture_destroy(BkRenderer *r, BkTexture *t) {
  if (!t)
    return;
  if (r->submitted) {
    vkDeviceWaitIdle(r->device);
    r->submitted = 0;
  }
  if (t->descriptor)
    vkFreeDescriptorSets(r->device, r->descriptors, 1, &t->descriptor);
  if (t->pending_upload) {
    BkTexture **link = &r->texture_uploads;
    while (*link && *link != t)
      link = &(*link)->next_upload;
    if (*link)
      *link = t->next_upload;
  }
  buffer_destroy(r, &t->upload);
  image_destroy(r, &t->image);
  free(t);
}
static BkGpuMesh *mesh_create(BkRenderer *r, const void *vertices,
                              unsigned vertex_count, const uint16_t *indices,
                              unsigned index_count, int lit, char error[256]) {
  const size_t stride = lit ? sizeof(BkLitVertex) : sizeof(BkVertex);
  if (!r || r->active || !vertices || !indices || !vertex_count ||
      vertex_count > 65536 || !index_count || index_count % 3 ||
      index_count > 6000000) {
    snprintf(error, 256, "invalid indexed mesh upload state/count");
    return NULL;
  }
  for (unsigned i = 0; i < index_count; i++)
    if (indices[i] >= vertex_count) {
      snprintf(error, 256, "mesh index %u exceeds vertex count", i);
      return NULL;
    }
  _Static_assert(sizeof(BkVertex) == 9 * sizeof(float),
                 "packed float vertices");
  for (unsigned i = 0; i < vertex_count; i++) {
    float components[sizeof(BkLitVertex) / sizeof(float)];
    memcpy(components, (const uint8_t *)vertices + i * stride, stride);
    for (unsigned j = 0; j < stride / sizeof(float); j++)
      if (!isfinite(components[j])) {
        snprintf(error, 256, "nonfinite mesh vertex %u", i);
        return NULL;
      }
  }
  BkGpuMesh *mesh = calloc(1, sizeof(*mesh));
  if (!mesh) {
    snprintf(error, 256, "mesh allocation failed");
    return NULL;
  }
  mesh->owner = r;
  mesh->references = 1;
  mesh->index_count = index_count;
  mesh->vertex_count = vertex_count;
  mesh->lit = lit;
  if (!buffer_create(r, &mesh->vertices, (size_t)vertex_count * stride,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         (lit ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0),
                     error) ||
      !buffer_create(r, &mesh->indices, (size_t)index_count * sizeof(*indices),
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT, error))
    goto fail;
  memcpy(mesh->vertices.mapped, vertices, (size_t)vertex_count * stride);
  memcpy(mesh->indices.mapped, indices, (size_t)index_count * sizeof(*indices));
  VK_TRY(buffer_flush(r, &mesh->vertices, 0));
  VK_TRY(buffer_flush(r, &mesh->indices, 0));
  return mesh;
fail:
  bk_mesh_destroy(r, mesh);
  return NULL;
}
BkGpuMesh *bk_mesh_create(BkRenderer *r, const BkVertex *v, unsigned nv,
                          const uint16_t *ix, unsigned ni, char error[256]) {
  return mesh_create(r, v, nv, ix, ni, 0, error);
}
BkGpuMesh *bk_lit_mesh_create(BkRenderer *r, const BkLitVertex *v, unsigned nv,
                              const uint16_t *ix, unsigned ni,
                              char error[256]) {
  _Static_assert(sizeof(BkLitVertex) == 22 * sizeof(float),
                 "packed lit vertices");
  return mesh_create(r, v, nv, ix, ni, 1, error);
}
static int mesh_update(BkRenderer *r, BkGpuMesh *mesh, const void *vertices,
                       unsigned vertex_count, int lit, char error[256]) {
  if (!r || r->active || !mesh || mesh->owner != r || mesh->lit != lit ||
      !vertices || vertex_count != mesh->vertex_count) {
    snprintf(error, 256, "invalid mesh update state/count/format");
    return 0;
  }
  const size_t stride = lit ? sizeof(BkLitVertex) : sizeof(BkVertex);
  for (unsigned i = 0; i < vertex_count; i++) {
    float components[sizeof(BkLitVertex) / sizeof(float)];
    memcpy(components, (const uint8_t *)vertices + i * stride, stride);
    for (unsigned j = 0; j < stride / sizeof(float); j++)
      if (!isfinite(components[j])) {
        snprintf(error, 256, "nonfinite mesh update vertex %u", i);
        return 0;
      }
  }
  if (!wait_frame(r, error))
    return 0;
  Buffer *target = mesh->palette ? &mesh->skin_source : &mesh->vertices;
  memcpy(target->mapped, vertices, (size_t)vertex_count * stride);
  VK_TRY(buffer_flush(r, target, 0));
  if (mesh->palette)
    skin_queue(r, mesh);
  r->stats.mesh_updates++;
  r->stats.uploaded_bytes += (uint64_t)vertex_count * stride;
  return 1;
fail:
  return 0;
}
int bk_mesh_update(BkRenderer *r, BkGpuMesh *mesh, const BkVertex *vertices,
                   unsigned vertex_count, char error[256]) {
  return mesh_update(r, mesh, vertices, vertex_count, 0, error);
}
int bk_lit_mesh_update(BkRenderer *r, BkGpuMesh *mesh,
                       const BkLitVertex *vertices, unsigned vertex_count,
                       char error[256]) {
  return mesh_update(r, mesh, vertices, vertex_count, 1, error);
}
void bk_mesh_destroy(BkRenderer *r, BkGpuMesh *mesh) {
  if (!mesh || --mesh->references)
    return;
  if (r->submitted) {
    vkDeviceWaitIdle(r->device);
    r->submitted = 0;
  }
  skin_mesh_release(r, mesh);
  buffer_destroy(r, &mesh->vertices);
  buffer_destroy(r, &mesh->indices);
  free(mesh);
}
/* Public transforms/clear values retain the native near=0, far=1 convention.
 * Reverse only the GPU depth row, BEFORE vertex multiplication: subtracting
 * rounded clip Z from W in the shader would already have lost far precision.
 * D32 float then keeps precision near zero for distant, adjacent surfaces. */
static int depth_matrix(float out[16], const float matrix[16], char error[256]) {
  memcpy(out, matrix, 16 * sizeof(float));
  for (unsigned i = 0; i < 4; ++i) {
    out[i * 4 + 2] = (float)((double)matrix[i * 4 + 3] - matrix[i * 4 + 2]);
    if (!isfinite(out[i * 4 + 2])) {
      snprintf(error, 256, "GPU depth transform overflow");
      return 0;
    }
  }
  return 1;
}
int bk_renderer_depth_transform(BkDepthTransform *out, const float view[16],
                                 const float projection[16], char error[256]) {
  if (!out || !view || !projection) {
    snprintf(error, 256, "missing separate depth view/projection");
    return 0;
  }
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(view[i]) || !isfinite(projection[i])) {
      snprintf(error, 256, "nonfinite depth view/projection");
      return 0;
    }
  /* Form projection W-Z before either view or object multiplication. The
   * old composed float MVP has already rounded away those small differences;
   * converting it afterwards cannot recover them when the camera rotates. */
  BkDepthTransform next = {{0}};
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned k = 0; k < 4; ++k)
      next.view_row[i] += (double)view[i * 4 + k] *
          ((double)projection[k * 4 + 3] - projection[k * 4 + 2]);
  *out = next;
  return 1;
}
static int draw_mesh(BkRenderer *r, BkTexture *texture, BkGpuMesh *mesh,
                     BkLightSet *lights, const float matrix[16],
                     const float world[16], const BkDepthTransform *depth,
                     BkDrawState state, char error[256]) {
  int lit = world != NULL;
  if (!r || !r->active || !texture || texture->owner != r || !mesh ||
      mesh->owner != r || mesh->lit != lit ||
      (lit && (!lights || lights->owner != r)) || !matrix ||
      (unsigned)state.blend >= BK_BLEND_COUNT ||
      (state.depth_write != 0 && state.depth_write != 1) ||
      (unsigned)state.cull > BK_CULL_COUNTER_CLOCKWISE) {
    snprintf(error, 256, "invalid indexed mesh draw state");
    return 0;
  }
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(matrix[i])) {
      snprintf(error, 256, "nonfinite mesh transform");
      return 0;
    }
  float gpu_matrix[16];
  if (depth && world) {
    memcpy(gpu_matrix, matrix, sizeof(gpu_matrix));
    for (unsigned i = 0; i < 4; ++i) {
      double value = 0;
      for (unsigned k = 0; k < 4; ++k)
        value += (double)world[i * 4 + k] * depth->view_row[k];
      gpu_matrix[i * 4 + 2] = (float)value;
      if (!isfinite(gpu_matrix[i * 4 + 2])) {
        snprintf(error, 256, "GPU separate depth transform overflow");
        return 0;
      }
    }
  } else if (!depth_matrix(gpu_matrix, matrix, error))
    return 0;
  if (lit) {
    for (unsigned i = 0; i < 16; i++)
      if (!isfinite(world[i])) {
        snprintf(error, 256, "nonfinite world transform");
        return 0;
      }
    double scale = 0;
    for (unsigned row = 0; row < 3; ++row)
      for (unsigned col = 0; col < 3; ++col)
        scale = fmax(scale, fabs(world[row * 4 + col]));
    double det =
        (double)world[0] *
            (world[5] * (double)world[10] - world[6] * (double)world[9]) -
        (double)world[1] *
            (world[4] * (double)world[10] - world[6] * (double)world[8]) +
        (double)world[2] *
            (world[4] * (double)world[9] - world[5] * (double)world[8]);
    /* A snow respawn can land exactly on zero scale. All vertices then have
     * the same clip position: triangle lists cannot produce a fragment.
     * Cull only that exact case, before the undefined normal inverse. Require
     * both valid constant W and the matching collapsed clip transform; a flat
     * but still drawable mesh or inconsistent caller transform remains an
     * error.
     */
    if (scale == 0 && world[3] == 0 && world[7] == 0 && world[11] == 0 &&
        world[15] > 0) {
      int collapsed = 1;
      for (unsigned i = 0; i < 12; ++i)
        collapsed &= matrix[i] == 0;
      if (collapsed)
        return 1;
    }
    /* Snow respawn keys legitimately shrink all axes to ~3e-7. Test the
     * relative determinant, not absolute volume. The shader likewise removes
     * uniform scale before computing its normalized inverse-transpose. */
    if (scale > 0)
      det = det / scale / scale / scale;
    /* Some authored rigid accessory frames have constant W != 1. Retain it
     * in clip coordinates and account for W in the shader's viewer offset. */
    if (world[3] != 0 || world[7] != 0 || world[11] != 0 || world[15] <= 0 ||
        !isfinite(det) || fabs(det) < 1e-12) {
      snprintf(error, 256,
               "lit world transform must be nonsingular affine (det=%g, "
               "scale=%g, W=%g; "
               "perspective=%g,%g,%g)",
               det, scale, world[15], world[3], world[7], world[11]);
      return 0;
    }
    bind_graphics_descriptor(r, 1, lights->descriptor);
    vkCmdPushConstants(r->command, r->layout, VK_SHADER_STAGE_VERTEX_BIT, 64,
                       64, world);
  }
  bind_graphics_pipeline(
      r,
      r->pipelines[lit][state.blend][state.depth_write][state.cull]);
  bind_graphics_vertices(r, mesh->vertices.handle);
  bind_graphics_indices(r, mesh->indices.handle);
  bind_graphics_descriptor(r, 0, texture->descriptor);
  vkCmdPushConstants(r->command, r->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64,
                     gpu_matrix);
  vkCmdDrawIndexed(r->command, mesh->index_count, 1, 0, 0, 0);
  r->stats.draws++;
  return 1;
}
int bk_renderer_draw_mesh(BkRenderer *r, BkTexture *t, BkGpuMesh *m,
                          const float matrix[16], BkDrawState state,
                          char error[256]) {
  return draw_mesh(r, t, m, NULL, matrix, NULL, NULL, state, error);
}
int bk_renderer_draw_lit_mesh(BkRenderer *r, BkTexture *t, BkGpuMesh *m,
                              BkLightSet *lights, const float matrix[16],
                              const float world[16], BkDrawState state,
                              char error[256]) {
  if (!world) {
    snprintf(error, 256, "missing lit world transform");
    return 0;
  }
  return draw_mesh(r, t, m, lights, matrix, world, NULL, state, error);
}
int bk_renderer_draw_lit_mesh_projected(
    BkRenderer *r, BkTexture *t, BkGpuMesh *m, BkLightSet *lights,
    const float matrix[16], const float world[16], const BkDepthTransform *depth,
    BkDrawState state, char error[256]) {
  if (!world || !depth) {
    snprintf(error, 256, "missing world or separate depth transform");
    return 0;
  }
  return draw_mesh(r, t, m, lights, matrix, world, depth, state, error);
}
void bk_renderer_extent(const BkRenderer *r, unsigned *width,
                        unsigned *height) {
  *width = r->width;
  *height = r->height;
}
int bk_renderer_viewport(BkRenderer *r, const BkViewport *viewport,
                         char error[256]) {
  BkViewport full = {0, 0, r->width, r->height};
  const BkViewport *v = viewport ? viewport : &full;
  if (!r->active || !v->width || !v->height || v->x > r->width ||
      v->y > r->height || v->width > r->width - v->x ||
      v->height > r->height - v->y) {
    snprintf(error, 256, "invalid or inactive viewport");
    return 0;
  }
  VkViewport vp = {(float)v->x,      (float)v->y, (float)v->width,
                   (float)v->height, 0,           1};
  VkRect2D sc = {{(int32_t)v->x, (int32_t)v->y}, {v->width, v->height}};
  vkCmdSetViewport(r->command, 0, 1, &vp);
  vkCmdSetScissor(r->command, 0, 1, &sc);
  r->viewport = *v;
  return 1;
}
int bk_renderer_clear_depth(BkRenderer *r, float depth, char error[256]) {
  if (!r || !r->active || !isfinite(depth) || depth < 0 || depth > 1) {
    snprintf(error, 256, "invalid or inactive depth clear");
    return 0;
  }
  VkClearAttachment attachment = {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .clearValue = {.depthStencil = {1.0f - depth, 0}}};
  VkClearRect rect = {
      .rect = {{(int32_t)r->viewport.x, (int32_t)r->viewport.y},
               {r->viewport.width, r->viewport.height}},
      .baseArrayLayer = 0,
      .layerCount = 1};
  vkCmdClearAttachments(r->command, 1, &attachment, 1, &rect);
  return 1;
}
int bk_renderer_begin(BkRenderer *r, char error[256]) {
  if (r->active) {
    snprintf(error, 256, "frame already active");
    return 0;
  }
  if (!wait_frame(r, error))
    return 0;
  if (r->swapchain) {
    double start = performance_seconds();
    VkResult vr = vkAcquireNextImageKHR(r->device, r->swapchain, UINT64_MAX,
                                        r->acquired, VK_NULL_HANDLE, &r->index);
    r->stats.acquire_seconds += performance_seconds() - start;
    if (vr != VK_SUCCESS && vr != VK_SUBOPTIMAL_KHR) {
      snprintf(error, 256, "acquire swapchain: VkResult %d", vr);
      return 0;
    }
  }
  VK_TRY(vkResetCommandBuffer(r->command, 0));
  VkCommandBufferBeginInfo cb = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  VK_TRY(vkBeginCommandBuffer(r->command, &cb));
  reset_graphics_bindings(r);
  VkClearValue clear[2] = {{.color = {{0, 0, 0, 1}}}, {.depthStencil = {0, 0}}};
  record_texture_uploads(r);
  record_skin_updates(r);
  VkRenderPassBeginInfo rp = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                              .renderPass = r->pass,
                              .framebuffer = r->frames[r->index],
                              .renderArea = {{0, 0}, {r->width, r->height}},
                              .clearValueCount = 2,
                              .pClearValues = clear};
  vkCmdBeginRenderPass(r->command, &rp, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport vp = {0, 0, (float)r->width, (float)r->height, 0, 1};
  VkRect2D sc = {{0, 0}, {r->width, r->height}};
  vkCmdSetViewport(r->command, 0, 1, &vp);
  vkCmdSetScissor(r->command, 0, 1, &sc);
  r->vertex_count = 0;
  r->viewport = (BkViewport){0, 0, r->width, r->height};
  r->acquire_pending = r->swapchain != VK_NULL_HANDLE;
  r->active = 1;
  return 1;
fail:
  return 0;
}
int bk_renderer_draw(BkRenderer *r, BkTexture *t, const BkVertex *v, unsigned n,
                     const float matrix[16], char error[256]) {
  return bk_renderer_draw_vertices(r, t, v, n, matrix,
      (BkDrawState){BK_BLEND_UI_ALPHA, 1, BK_CULL_NONE}, error);
}
int bk_renderer_draw_vertices(BkRenderer *r, BkTexture *t, const BkVertex *v,
    unsigned n, const float matrix[16], BkDrawState state, char error[256]) {
  if (!r || !r->active || !t || t->owner != r || !v || !matrix ||
      n > VERTEX_CAPACITY - r->vertex_count || n % 3 ||
      (unsigned)state.blend >= BK_BLEND_COUNT ||
      (state.depth_write != 0 && state.depth_write != 1) ||
      (unsigned)state.cull > BK_CULL_COUNTER_CLOCKWISE) {
    snprintf(error, 256, "invalid draw state/count");
    return 0;
  }
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(matrix[i])) {
      snprintf(error, 256, "nonfinite transient transform"); return 0;
    }
  float gpu_matrix[16];
  if (!depth_matrix(gpu_matrix, matrix, error))
    return 0;
  for (unsigned i = 0; i < n; ++i) {
    float values[9]; memcpy(values, &v[i], sizeof values);
    for (unsigned j = 0; j < 9; ++j)
      if (!isfinite(values[j])) {
        snprintf(error, 256, "nonfinite transient vertex"); return 0;
      }
  }
  memcpy((BkVertex *)r->vertices.mapped + r->vertex_count, v,
         (size_t)n * sizeof(*v));
  bind_graphics_pipeline(
      r, r->pipelines[0][state.blend][state.depth_write][state.cull]);
  bind_graphics_vertices(r, r->vertices.handle);
  bind_graphics_descriptor(r, 0, t->descriptor);
  vkCmdPushConstants(r->command, r->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64,
                     gpu_matrix);
  vkCmdDraw(r->command, n, 1, r->vertex_count, 0);
  r->stats.draws++;
  r->stats.uploaded_bytes += (uint64_t)n * sizeof(*v);
  r->vertex_count += n;
  return 1;
}
int bk_renderer_capture(BkRenderer *r, uint8_t *rgba, size_t size,
                        char error[256]) {
  if (!r || !r->active || !rgba || size != (size_t)r->width * r->height * 4) {
    snprintf(error, 256, "invalid active capture state/buffer");
    return 0;
  }
  vkCmdEndRenderPass(r->command);
  r->active = 0;
  barrier(r->command, r->images[r->index],
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy copy = {
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageExtent = {r->width, r->height, 1}};
  vkCmdCopyImageToBuffer(r->command, r->images[r->index],
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         r->readback.handle, 1, &copy);
  VkMemoryBarrier host = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                          .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
  vkCmdPipelineBarrier(r->command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, NULL, 0,
                       NULL);
  barrier(r->command, r->images[r->index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  VK_TRY(vkEndCommandBuffer(r->command));
  VK_TRY(buffer_flush(r, &r->vertices, 0));
  VK_TRY(vkResetFences(r->device, 1, &r->fence));
  VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .waitSemaphoreCount = r->acquire_pending ? 1 : 0,
                         .pWaitSemaphores = &r->acquired,
                         .pWaitDstStageMask = &wait,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &r->command};
  VK_TRY(vkQueueSubmit(r->queue, 1, &submit, r->fence));
  r->acquire_pending = 0;
  r->submitted = 1;
  if (!wait_frame(r, error))
    goto fail;
  VK_TRY(buffer_flush(r, &r->readback, 1));
  const uint8_t *src = r->readback.mapped;
  if (r->format == VK_FORMAT_B8G8R8A8_UNORM) {
    for (size_t i = 0; i < size; i += 4) {
      rgba[i] = src[i + 2];
      rgba[i + 1] = src[i + 1];
      rgba[i + 2] = src[i];
      rgba[i + 3] = src[i + 3];
    }
  } else
    memcpy(rgba, src, size);
  VK_TRY(vkResetCommandBuffer(r->command, 0));
  VkCommandBufferBeginInfo begin = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  VK_TRY(vkBeginCommandBuffer(r->command, &begin));
  reset_graphics_bindings(r);
  VkRenderPassBeginInfo pass = {.sType =
                                    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = r->resume_pass,
                                .framebuffer = r->frames[r->index],
                                .renderArea = {{0, 0}, {r->width, r->height}}};
  vkCmdBeginRenderPass(r->command, &pass, VK_SUBPASS_CONTENTS_INLINE);
  r->active = 1;
  return bk_renderer_viewport(r, &r->viewport, error);
fail:
  return 0;
}
int bk_renderer_end(BkRenderer *r, char error[256]) {
  if (!r->active) {
    snprintf(error, 256, "no active frame");
    return 0;
  }
  vkCmdEndRenderPass(r->command);
  r->active = 0;
  if (r->swapchain)
    barrier(r->command, r->images[r->index],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  else {
    barrier(r->command, r->images[0], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {r->width, r->height, 1}};
    vkCmdCopyImageToBuffer(r->command, r->images[0],
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           r->readback.handle, 1, &copy);
    VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                          .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(r->command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
                         NULL);
  }
  VK_TRY(vkEndCommandBuffer(r->command));
  VK_TRY(buffer_flush(r, &r->vertices, 0));
  VK_TRY(vkResetFences(r->device, 1, &r->fence));
  VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                     .waitSemaphoreCount = r->acquire_pending ? 1 : 0,
                     .pWaitSemaphores = &r->acquired,
                     .pWaitDstStageMask = &wait,
                     .commandBufferCount = 1,
                     .pCommandBuffers = &r->command,
                     .signalSemaphoreCount = r->swapchain ? 1 : 0,
                     .pSignalSemaphores = &r->finished[r->index]};
  VK_TRY(vkQueueSubmit(r->queue, 1, &si, r->fence));
  r->acquire_pending = 0;
  r->submitted = 1;
  r->rendered = 1;
  if (r->swapchain) {
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1,
                                .pWaitSemaphores = &r->finished[r->index],
                                .swapchainCount = 1,
                                .pSwapchains = &r->swapchain,
                                .pImageIndices = &r->index};
    double start = performance_seconds();
    VkResult vr = vkQueuePresentKHR(r->queue, &present);
    r->stats.present_seconds += performance_seconds() - start;
    if (vr != VK_SUCCESS && vr != VK_SUBOPTIMAL_KHR) {
      snprintf(error, 256, "present: VkResult %d", vr);
      return 0;
    }
  }
  return 1;
fail:
  return 0;
}
int bk_renderer_readback(BkRenderer *r, uint8_t *rgba, size_t n,
                         char error[256]) {
  if (r->swapchain || !r->rendered || r->active ||
      n != (size_t)r->width * r->height * 4) {
    snprintf(error, 256, "invalid readback state/size");
    return 0;
  }
  if (!wait_frame(r, error))
    return 0;
  VK_TRY(buffer_flush(r, &r->readback, 1));
  memcpy(rgba, r->readback.mapped, n);
  return 1;
fail:
  return 0;
}
void bk_renderer_destroy(BkRenderer *r) {
  if (!r)
    return;
  if (r->device) {
    vkDeviceWaitIdle(r->device);
    buffer_destroy(r, &r->vertices);
    buffer_destroy(r, &r->readback);
    if (r->command_pool)
      vkDestroyCommandPool(r->device, r->command_pool, NULL);
    if (r->fence)
      vkDestroyFence(r->device, r->fence, NULL);
    if (r->acquired)
      vkDestroySemaphore(r->device, r->acquired, NULL);
    for (unsigned i = 0; i < MAX_IMAGES; i++) {
      if (r->finished[i])
        vkDestroySemaphore(r->device, r->finished[i], NULL);
      if (r->frames[i])
        vkDestroyFramebuffer(r->device, r->frames[i], NULL);
      if (r->swapchain && r->views[i])
        vkDestroyImageView(r->device, r->views[i], NULL);
    }
    image_destroy(r, &r->offscreen);
    image_destroy(r, &r->depth);
    for (unsigned lit = 0; lit < 2; lit++)
      for (unsigned mode = 0; mode < BK_BLEND_COUNT; mode++)
        for (unsigned write = 0; write < 2; write++)
          for (unsigned cull = 0; cull < 2; cull++)
            if (r->pipelines[lit][mode][write][cull])
              vkDestroyPipeline(r->device, r->pipelines[lit][mode][write][cull],
                                NULL);
    if (r->transfer_pipeline)
      vkDestroyPipeline(r->device, r->transfer_pipeline, NULL);
    if (r->transfer_layout)
      vkDestroyPipelineLayout(r->device, r->transfer_layout, NULL);
    if (r->transfer_descriptor_layout)
      vkDestroyDescriptorSetLayout(r->device, r->transfer_descriptor_layout, NULL);
    if (r->skin_pipeline)
      vkDestroyPipeline(r->device, r->skin_pipeline, NULL);
    if (r->skin_layout)
      vkDestroyPipelineLayout(r->device, r->skin_layout, NULL);
    if (r->skin_descriptor_layout)
      vkDestroyDescriptorSetLayout(r->device, r->skin_descriptor_layout, NULL);
    if (r->layout)
      vkDestroyPipelineLayout(r->device, r->layout, NULL);
    if (r->descriptors)
      vkDestroyDescriptorPool(r->device, r->descriptors, NULL);
    if (r->lighting_layout)
      vkDestroyDescriptorSetLayout(r->device, r->lighting_layout, NULL);
    if (r->descriptor_layout)
      vkDestroyDescriptorSetLayout(r->device, r->descriptor_layout, NULL);
    for (unsigned i = 0; i < 2; i++)
      if (r->samplers[i])
        vkDestroySampler(r->device, r->samplers[i], NULL);
    if (r->pass)
      vkDestroyRenderPass(r->device, r->pass, NULL);
    if (r->resume_pass)
      vkDestroyRenderPass(r->device, r->resume_pass, NULL);
    if (r->swapchain)
      vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
    vkDestroyDevice(r->device, NULL);
  }
  if (r->surface)
    vkDestroySurfaceKHR(r->instance, r->surface, NULL);
  if (r->instance)
    vkDestroyInstance(r->instance, NULL);
  free(r);
}
