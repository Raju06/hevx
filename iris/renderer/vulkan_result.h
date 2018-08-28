#ifndef HEV_IRIS_VK_RESULT_H_
#define HEV_IRIS_VK_RESULT_H_
/*! \file
 * \brief \ref iris::Renderer::VulkanResult definition.
 */

#include "flextVk.h"
#include <string>
#include <system_error>

namespace iris::Renderer {

//! \brief Vulkan result codes.
enum class VulkanResult {
  kSuccess = VK_SUCCESS,
  kNotReady = VK_NOT_READY,
  kTimeout = VK_TIMEOUT,
  kEventSet = VK_EVENT_SET,
  kEventReset = VK_EVENT_RESET,
  kIncomplete = VK_INCOMPLETE,
  kErrorOutOfHostMemory = VK_ERROR_OUT_OF_HOST_MEMORY,
  kErrorOutOfDeviceMemory = VK_ERROR_OUT_OF_DEVICE_MEMORY,
  kErrorInitializationFailed = VK_ERROR_INITIALIZATION_FAILED,
  kErrorDeviceLost = VK_ERROR_DEVICE_LOST,
  kErrorMemoryMapFailed = VK_ERROR_MEMORY_MAP_FAILED,
  kErrorLayerNotPresent = VK_ERROR_LAYER_NOT_PRESENT,
  kErrorExtensionNotPresent = VK_ERROR_EXTENSION_NOT_PRESENT,
  kErrorFeatureNotPresent = VK_ERROR_FEATURE_NOT_PRESENT,
  kErrorIncompatibleDriver = VK_ERROR_INCOMPATIBLE_DRIVER,
  kErrorTooManyObjects = VK_ERROR_TOO_MANY_OBJECTS,
  kErrorFormatNotSupported = VK_ERROR_FORMAT_NOT_SUPPORTED,
  kErrorFragmentedPool = VK_ERROR_FRAGMENTED_POOL,
  kErrorOutOfPoolMemory = VK_ERROR_OUT_OF_POOL_MEMORY,
  kErrorInvalidExternalHandle = VK_ERROR_INVALID_EXTERNAL_HANDLE,
  kErrorSurfaceLostKHR = VK_ERROR_SURFACE_LOST_KHR,
  kErrorNativeWindowInUseKHR = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
  kSuboptimalKHR = VK_SUBOPTIMAL_KHR,
  kErrorOutOfDataKHR = VK_ERROR_OUT_OF_DATE_KHR,
  kErrorValidationFailedEXT = VK_ERROR_VALIDATION_FAILED_EXT,
}; // enum class VulkanResult
static_assert(VK_SUCCESS == 0);

//! \brief Convert a VulkanResult to a std::string.
inline std::string to_string(VulkanResult result) noexcept {
  using namespace std::string_literals;
  switch (result) {
  case VulkanResult::kSuccess: return "success"s;
  case VulkanResult::kNotReady: return "not ready"s;
  case VulkanResult::kTimeout: return "timeout"s;
  case VulkanResult::kEventSet: return "event set"s;
  case VulkanResult::kEventReset: return "event reset"s;
  case VulkanResult::kIncomplete: return "incomplete"s;
  case VulkanResult::kErrorOutOfHostMemory: return "error: out of host memory"s;
  case VulkanResult::kErrorOutOfDeviceMemory:
    return "error: out of device memory"s;
  case VulkanResult::kErrorInitializationFailed:
    return "error: initialization failed"s;
  case VulkanResult::kErrorDeviceLost: return "error: device lost"s;
  case VulkanResult::kErrorMemoryMapFailed: return "error: memory map failed"s;
  case VulkanResult::kErrorLayerNotPresent: return "error: layer not present"s;
  case VulkanResult::kErrorExtensionNotPresent:
    return "error: extension not present"s;
  case VulkanResult::kErrorFeatureNotPresent:
    return "error: feature not present"s;
  case VulkanResult::kErrorIncompatibleDriver:
    return "error: incompatible driver"s;
  case VulkanResult::kErrorTooManyObjects: return "error: too many objects"s;
  case VulkanResult::kErrorFormatNotSupported:
    return "error: format not supported"s;
  case VulkanResult::kErrorFragmentedPool: return "error: fragmented pool"s;
  case VulkanResult::kErrorOutOfPoolMemory: return "error: out of pool memory"s;
  case VulkanResult::kErrorInvalidExternalHandle:
    return "error: invalid external handle"s;
  case VulkanResult::kErrorSurfaceLostKHR: return "error: surface lost"s;
  case VulkanResult::kErrorNativeWindowInUseKHR:
    return "error: native window in use"s;
  case VulkanResult::kSuboptimalKHR: return "suboptimal"s;
  case VulkanResult::kErrorOutOfDataKHR: return "error: out of date"s;
  case VulkanResult::kErrorValidationFailedEXT:
    return "error: validation failed"s;
  }
  return "unknown"s;
} // to_string

//! \brief Convert a VkResult to a std::string.
inline std::string to_string(VkResult result) noexcept {
  return to_string(static_cast<VulkanResult>(result));
}

//! \brief Implements std::error_category for \ref VulkanResult
class VulkanResultCategory : public std::error_category {
public:
  virtual ~VulkanResultCategory() noexcept {}

  //! \brief Get the name of this category.
  virtual const char* name() const noexcept override {
    return "iris::VulkanResult";
  }

  //! \brief Convert an int representing an Error into a std::string.
  virtual std::string message(int ev) const override {
    return to_string(static_cast<VulkanResult>(ev));
  }
};

//! The global instance of the VulkanResultCategory.
inline VulkanResultCategory const gVulkanResultCategory;

/*! \brief Get the global instance of the VulkanResultCategory.
 * \return \ref gVulkanResultCategory
 */
inline std::error_category const& GetVulkanResultCategory() {
  return gVulkanResultCategory;
}

/*! \brief Make a std::error_code from a \ref VulkanResult.
 * \return std::error_code
 */
inline std::error_code make_error_code(VulkanResult r) noexcept {
  return std::error_code(static_cast<int>(r), GetVulkanResultCategory());
}

/*! \brief Make a std::error_code from a VkResult.
 * \return std::error_code
 */
inline std::error_code make_error_code(VkResult r) noexcept {
  return std::error_code(static_cast<int>(r), GetVulkanResultCategory());
}

} // namespace iris::Renderer

namespace std {

template <>
struct is_error_code_enum<iris::Renderer::VulkanResult> : public true_type {};

} // namespace std

#endif // HEV_IRIS_VK_RESULT_H_

