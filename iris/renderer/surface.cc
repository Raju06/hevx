#include "renderer/surface.h"
#include "absl/container/fixed_array.h"
#include "glm/common.hpp"
#include "logging.h"
#include "renderer/image.h"
#include "renderer/impl.h"
#if defined(VK_USE_PLATFORM_XCB_KHR)
#include "wsi/window_x11.h"
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#include "wsi/window_win32.h"
#endif
#include <algorithm>

namespace iris::Renderer {

tl::expected<VkSurfaceKHR, std::system_error>
static CreateSurface(wsi::Window& window) noexcept {
  IRIS_LOG_ENTER();
  VkSurfaceKHR surface;
  auto native = window.NativeHandle();

#if defined(VK_USE_PLATFORM_XCB_KHR)

  VkXcbSurfaceCreateInfoKHR sci = {};
  sci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  sci.connection = native.connection;
  sci.window = native.window;

  GetLogger()->debug("fp: {}", reinterpret_cast<void*>(vkCreateXcbSurfaceKHR));

  VkResult result = vkCreateXcbSurfaceKHR(sInstance, &sci, nullptr, &surface);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result), "Cannot create surface"));
  }

#elif defined(VK_USE_PLATFORM_WIN32_KHR)

  VkWin32SurfaceCreateInfoKHR sci = {};
  sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  sci.hinstance = native.hInstance;
  sci.hwnd = native.hWnd;

  VkResult result = vkCreateWin32SurfaceKHR(sInstance, &sci, nullptr, &surface);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result), "Cannot create surface"));
  }

#endif

  IRIS_LOG_LEAVE();
  return surface;
} // CreateSurface

tl::expected<bool, std::system_error>
static CheckSurfaceSupport(VkSurfaceKHR surface) noexcept {
  IRIS_LOG_ENTER();
  VkBool32 support;
  VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
    sPhysicalDevice, sGraphicsQueueFamilyIndex, surface, &support);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result),
                        "Cannot check for physical device surface support"));
  }

  IRIS_LOG_LEAVE();
  return (support == VK_TRUE);
} // CheckSurfaceSupport

tl::expected<bool, std::system_error> static CheckSurfaceFormat(
  VkSurfaceKHR surface, VkSurfaceFormatKHR desired) noexcept {
  IRIS_LOG_ENTER();
  VkResult result;

  std::uint32_t numSurfaceFormats;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(sPhysicalDevice, surface,
                                                &numSurfaceFormats, nullptr);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(std::system_error(
      make_error_code(result), "Cannot get physical device surface formats"));
  }

  absl::FixedArray<VkSurfaceFormatKHR> surfaceFormats(numSurfaceFormats);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
    sPhysicalDevice, surface, &numSurfaceFormats, surfaceFormats.data());
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(std::system_error(
      make_error_code(result), "Cannot get physical device surface formats"));
  }

  if (numSurfaceFormats == 1 &&
      surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
    IRIS_LOG_LEAVE();
    return true;
  }

  for (auto&& supported : surfaceFormats) {
    if (supported.format == desired.format &&
        supported.colorSpace == desired.colorSpace) {
      IRIS_LOG_LEAVE();
      return true;
    }
  }

  IRIS_LOG_LEAVE();
  return false;
} // CheckSurfaceFormat

} // namespace iris::Renderer

tl::expected<iris::Renderer::Surface, std::system_error>
iris::Renderer::Surface::Create(wsi::Window& window,
                                glm::vec4 const& clearColor) noexcept {
  IRIS_LOG_ENTER();

  Surface surface;
  surface.clearColor.float32[0] = clearColor[0];
  surface.clearColor.float32[1] = clearColor[1];
  surface.clearColor.float32[2] = clearColor[2];
  surface.clearColor.float32[3] = clearColor[3];

  if (auto sfc = CreateSurface(window)) {
    surface.handle = *sfc;
  } else {
    IRIS_LOG_LEAVE();
    return tl::unexpected(sfc.error());
  }

  if (auto chk = CheckSurfaceSupport(surface.handle)) {
    if (!*chk) {
      IRIS_LOG_LEAVE();
      return tl::unexpected(
        std::system_error(std::make_error_code(std::errc::invalid_argument),
                          "Surface is not supported by the physical device."));
    }
  } else {
    IRIS_LOG_LEAVE();
    return tl::unexpected(chk.error());
  }

  if (auto chk = CheckSurfaceFormat(surface.handle, sSurfaceColorFormat)) {
    if (!*chk) {
      IRIS_LOG_LEAVE();
      return tl::unexpected(std::system_error(
        std::make_error_code(std::errc::invalid_argument),
        "Surface format is not supported by the physical device."));
    }
  } else {
    IRIS_LOG_LEAVE();
    return tl::unexpected(chk.error());
  }

  VkSemaphoreCreateInfo sci = {};
  sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  if (auto result =
        vkCreateSemaphore(sDevice, &sci, nullptr, &surface.imageAvailable);
      result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result), "Cannot create semaphore"));
  }

  auto extent = window.Extent();
  if (auto noret = surface.Resize({extent.width, extent.height}); !noret) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(noret.error());
  }

  IRIS_LOG_LEAVE();
  return std::move(surface);
} // iris::Renderer::Surface::Create

namespace iris::Renderer {

static tl::expected<VkSwapchainKHR, std::system_error>
CreateSwapchain(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR caps,
                VkExtent2D extent, VkSwapchainKHR oldSwapchain) {
  IRIS_LOG_ENTER();
  VkResult result;

  VkSwapchainCreateInfoKHR sci = {};
  sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  sci.surface = surface;
  sci.minImageCount = caps.minImageCount;
  sci.imageFormat = sSurfaceColorFormat.format;
  sci.imageColorSpace = sSurfaceColorFormat.colorSpace;
  sci.imageExtent = extent;
  sci.imageArrayLayers = 1;
  sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  sci.queueFamilyIndexCount = 0;
  sci.pQueueFamilyIndices = nullptr;
  sci.preTransform = caps.currentTransform;
  sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  sci.presentMode = sSurfacePresentMode;
  sci.clipped = VK_TRUE;
  sci.oldSwapchain = oldSwapchain;

  VkSwapchainKHR newSwapchain;
  result = vkCreateSwapchainKHR(sDevice, &sci, nullptr, &newSwapchain);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result), "Cannot create swapchain"));
  }

  IRIS_LOG_LEAVE();
  return newSwapchain;
} // CreateSwapchain

static tl::expected<VkFramebuffer, std::system_error>
CreateFramebuffer(gsl::span<VkImageView> attachments,
                  VkExtent2D extent) noexcept {
  IRIS_LOG_ENTER();
  VkResult result;

  VkFramebufferCreateInfo ci = {};
  ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  ci.renderPass = sRenderPass;
  ci.attachmentCount = gsl::narrow_cast<std::uint32_t>(attachments.size());
  ci.pAttachments = attachments.data();
  ci.width = extent.width;
  ci.height = extent.height;
  ci.layers = 1;

  VkFramebuffer framebuffer;
  result = vkCreateFramebuffer(sDevice, &ci, nullptr, &framebuffer);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(
      std::system_error(make_error_code(result), "Cannot create framebuffer"));
  }

  IRIS_LOG_LEAVE();
  return framebuffer;
} // CreateFramebuffer

} // namespace iris::Renderer

tl::expected<void, std::system_error>
iris::Renderer::Surface::Resize(VkExtent2D newExtent) noexcept {
  IRIS_LOG_ENTER();
  VkResult result;

  GetLogger()->debug("Surface resizing to ({}x{})", newExtent.width,
                     newExtent.height);

  VkSurfaceCapabilities2KHR surfaceCapabilities = {};
  surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

  VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
  surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  surfaceInfo.surface = handle;

  result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    sPhysicalDevice, &surfaceInfo, &surfaceCapabilities);
  if (result != VK_SUCCESS) {
    IRIS_LOG_LEAVE();
    return tl::unexpected(std::system_error(
      make_error_code(result), "Cannot query for surface capabilities"));
  }

  VkSurfaceCapabilitiesKHR caps = surfaceCapabilities.surfaceCapabilities;

  newExtent.width = caps.currentExtent.width == UINT32_MAX
                      ? glm::clamp(newExtent.width, caps.minImageExtent.width,
                                   caps.maxImageExtent.width)
                      : caps.currentExtent.width;

  newExtent.height =
    caps.currentExtent.height == UINT32_MAX
      ? glm::clamp(newExtent.height, caps.minImageExtent.height,
                   caps.maxImageExtent.height)
      : caps.currentExtent.height;

  VkExtent3D imageExtent{newExtent.width, newExtent.height, 1};

  VkViewport newViewport{0.f,
                         0.f,
                         static_cast<float>(newExtent.width),
                         static_cast<float>(newExtent.height),
                         0.f,
                         1.f};

  VkRect2D newScissor{{0, 0}, newExtent};

  VkSwapchainKHR newSwapchain{VK_NULL_HANDLE};
  if (auto swpc = CreateSwapchain(handle, caps, newExtent, swapchain)) {
    newSwapchain = *swpc;
  } else {
    IRIS_LOG_LEAVE();
    return tl::unexpected(swpc.error());
  }

  uint32_t numSwapchainImages;
  result = vkGetSwapchainImagesKHR(sDevice, newSwapchain, &numSwapchainImages,
                                   nullptr);
  if (result != VK_SUCCESS) {
    vkDestroySwapchainKHR(sDevice, newSwapchain, nullptr);
    IRIS_LOG_LEAVE();
    return tl::unexpected(std::system_error(make_error_code(result),
                                            "Cannot get swapchain images"));
  }

  std::vector<VkImage> newColorImages(numSwapchainImages);
  result = vkGetSwapchainImagesKHR(sDevice, newSwapchain, &numSwapchainImages,
                                   newColorImages.data());
  if (result != VK_SUCCESS) {
    vkDestroySwapchainKHR(sDevice, newSwapchain, nullptr);
    IRIS_LOG_LEAVE();
    return tl::unexpected(std::system_error(make_error_code(result),
                                            "Cannot get swapchain images"));
  }

  std::vector<VkImageView> newColorImageViews(numSwapchainImages);
  for (std::uint32_t i = 0; i < numSwapchainImages; ++i) {
    if (auto view = CreateImageView(
          newColorImages[i], sSurfaceColorFormat.format, VK_IMAGE_VIEW_TYPE_2D,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})) {
      newColorImageViews[i] = *view;
    } else {
      IRIS_LOG_LEAVE();
      return tl::unexpected(view.error());
    }
  }

  Image newDepthStencilImage;
  ImageView newDepthStencilImageView{};

  Image newColorTarget;
  ImageView newColorTargetView{};

  Image newDepthStencilTarget;
  ImageView newDepthStencilTargetView{};

  absl::FixedArray<VkImageView> attachments(sNumRenderPassAttachments);
  std::vector<VkFramebuffer> newFramebuffers(numSwapchainImages);

  std::system_error error(Error::kNone);

  if (auto ds = Image::Create(VK_IMAGE_TYPE_2D, sSurfaceDepthStencilFormat,
                              imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              VMA_MEMORY_USAGE_GPU_ONLY, "depthStencilImage")) {
    newDepthStencilImage = std::move(*ds);
  } else {
    error = ds.error();
    goto fail;
  }

  if (auto view = newDepthStencilImage.CreateImageView(
        VK_IMAGE_VIEW_TYPE_2D, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1})) {
    newDepthStencilImageView = std::move(*view);
  } else {
    error = view.error();
    goto fail;
  }

  if (auto ct =
        Image::Create(VK_IMAGE_TYPE_2D, sSurfaceColorFormat.format,
                           imageExtent, 1, 1, sSurfaceSampleCount,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                           VMA_MEMORY_USAGE_GPU_ONLY, "colorTarget")) {
    newColorTarget = std::move(*ct);
  } else {
    error = ct.error();
    goto fail;
  }

  if (auto view = newColorTarget.CreateImageView(
        VK_IMAGE_VIEW_TYPE_2D, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})) {
    newColorTargetView = std::move(*view);
  } else {
    error = view.error();
    goto fail;
  }

  GetLogger()->debug("Transitioning new color target");
  if (auto noret = newColorTarget.Transition(
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      !noret) {
    error = noret.error();
    goto fail;
  }

  if (auto ds = Image::Create(
        VK_IMAGE_TYPE_2D, sSurfaceDepthStencilFormat, imageExtent, 1, 1,
        sSurfaceSampleCount, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "depthStencilTarget")) {
    newDepthStencilTarget = std::move(*ds);
  } else {
    error = ds.error();
    goto fail;
  }

  if (auto view = newDepthStencilTarget.CreateImageView(
        VK_IMAGE_VIEW_TYPE_2D, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1})) {
    newDepthStencilTargetView = std::move(*view);
  } else {
    error = view.error();
    goto fail;
  }

  GetLogger()->debug("Transitioning new depth target");
  if (auto noret = newDepthStencilTarget.Transition(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      !noret) {
    error = noret.error();
    goto fail;
  }

  attachments[sColorTargetAttachmentIndex] = newColorTargetView;
  attachments[sDepthStencilTargetAttachmentIndex] = newDepthStencilTargetView;
  attachments[sDepthStencilResolveAttachmentIndex] = newDepthStencilImageView;

  for (std::uint32_t i = 0; i < numSwapchainImages; ++i) {
    attachments[sColorResolveAttachmentIndex] = newColorImageViews[i];
    if (auto fb = CreateFramebuffer(attachments, newExtent)) {
      newFramebuffers[i] = *fb;
    } else {
      error = fb.error();
      goto fail;
    }
  }

  if (swapchain != VK_NULL_HANDLE) Release();

  extent = newExtent;
  viewport = newViewport;
  scissor = newScissor;
  swapchain = newSwapchain;

  colorImages = std::move(newColorImages);
  colorImageViews = std::move(newColorImageViews);

  // swap to ensure old objects are properly released
  std::swap(depthStencilImage, newDepthStencilImage);
  std::swap(depthStencilImageView, newDepthStencilImageView);
  std::swap(colorTarget, newColorTarget);
  std::swap(colorTargetView, newColorTargetView);
  std::swap(depthStencilTarget, newDepthStencilTarget);
  std::swap(depthStencilTargetView, newDepthStencilTargetView);

  framebuffers = std::move(newFramebuffers);

  IRIS_LOG_LEAVE();
  return {};

fail:
  GetLogger()->debug("Surface::Resize cleaning up on on failure");
  for (auto&& framebuffer : newFramebuffers) {
    vkDestroyFramebuffer(sDevice, framebuffer, nullptr);
  }

  for (auto&& imageView : newColorImageViews) {
    vkDestroyImageView(sDevice, imageView, nullptr);
  }

  vkDestroySwapchainKHR(sDevice, newSwapchain, nullptr);

  IRIS_LOG_LEAVE();
  return tl::unexpected(error);
} // iris::Renderer::Surface::Resize

iris::Renderer::Surface::Surface(Surface&& other) noexcept
  : handle(other.handle)
  , imageAvailable(other.imageAvailable)
  , extent(other.extent)
  , viewport(other.viewport)
  , scissor(other.scissor)
  , clearColor(other.clearColor)
  , swapchain(other.swapchain)
  , colorImages(std::move(other.colorImages))
  , colorImageViews(std::move(other.colorImageViews))
  , depthStencilImage(std::move(other.depthStencilImage))
  , depthStencilImageView(std::move(other.depthStencilImageView))
  , colorTarget(std::move(other.colorTarget))
  , colorTargetView(std::move(other.colorTargetView))
  , depthStencilTarget(std::move(other.depthStencilTarget))
  , depthStencilTargetView(std::move(other.depthStencilTargetView))
  , framebuffers(std::move(other.framebuffers))
  , currentImageIndex(other.currentImageIndex) {
  IRIS_LOG_ENTER();

  other.handle = VK_NULL_HANDLE;
  other.imageAvailable = VK_NULL_HANDLE;
  other.swapchain = VK_NULL_HANDLE;

  IRIS_LOG_LEAVE();
} // iris::Renderer::Surface

iris::Renderer::Surface& iris::Renderer::Surface::
operator=(Surface&& rhs) noexcept {
  if (this == &rhs) return *this;
  IRIS_LOG_ENTER();

  handle = rhs.handle;
  imageAvailable = rhs.imageAvailable;
  extent = rhs.extent;
  viewport = rhs.viewport;
  scissor = rhs.scissor;
  clearColor = rhs.clearColor;
  swapchain = rhs.swapchain;
  colorImages = std::move(rhs.colorImages);
  colorImageViews = std::move(rhs.colorImageViews);
  depthStencilImage = std::move(rhs.depthStencilImage);
  depthStencilImageView = std::move(rhs.depthStencilImageView);
  colorTarget = std::move(rhs.colorTarget);
  colorTargetView = std::move(rhs.colorTargetView);
  depthStencilTarget = std::move(rhs.depthStencilTarget);
  depthStencilTargetView = std::move(rhs.depthStencilTargetView);
  framebuffers = std::move(rhs.framebuffers);
  currentImageIndex = (rhs.currentImageIndex);

  rhs.handle = VK_NULL_HANDLE;
  rhs.imageAvailable = VK_NULL_HANDLE;
  rhs.swapchain = VK_NULL_HANDLE;

  IRIS_LOG_LEAVE();
  return *this;
} // iris::Renderer::Surface::operator=

iris::Renderer::Surface::~Surface() noexcept {
  if (handle == VK_NULL_HANDLE) return;

  IRIS_LOG_ENTER();
  Release();
  vkDestroySemaphore(sDevice, imageAvailable, nullptr);
  vkDestroySurfaceKHR(sInstance, handle, nullptr);
  IRIS_LOG_LEAVE();
} // iris::Renderer::Surface::~Surface

void iris::Renderer::Surface::Release() noexcept {
  IRIS_LOG_ENTER();
  for (auto&& framebuffer : framebuffers) {
    vkDestroyFramebuffer(sDevice, framebuffer, nullptr);
  }

  for (auto&& imageView : colorImageViews) {
    vkDestroyImageView(sDevice, imageView, nullptr);
  }

  //GetLogger()->debug("Swapchain: {}", static_cast<void*>(swapchain));
  vkDestroySwapchainKHR(sDevice, swapchain, nullptr);
  IRIS_LOG_LEAVE();
} // iris::Renderer::Surface::Release

