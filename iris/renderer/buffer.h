#ifndef HEV_IRIS_RENDERER_BUFFER_H_
#define HEV_IRIS_RENDERER_BUFFER_H_

#include "iris/renderer/impl.h"
#include <string>
#include <system_error>

namespace iris::Renderer {

struct Buffer {
  static tl::expected<Buffer, std::system_error>
  Create(VkDeviceSize size, VkBufferUsageFlags bufferUsage,
         VmaMemoryUsage memoryUsage, std::string name = {}) noexcept;

  static tl::expected<Buffer, std::system_error>
  CreateFromMemory(VkDeviceSize size, VkBufferUsageFlags bufferUsage,
                   VmaMemoryUsage memoryUsage, gsl::not_null<void const*> data,
                   std::string name = {},
                   VkCommandPool commandPool = VK_NULL_HANDLE) noexcept;

  VkDeviceSize size{0};
  VkBuffer handle{VK_NULL_HANDLE};
  VmaAllocation allocation{VK_NULL_HANDLE};

  operator VkBuffer() const noexcept { return handle; }
  VkBuffer* get() noexcept { return &handle; }

  template <class T>
  tl::expected<T, std::system_error> Map() noexcept {
    void* ptr;
    if (auto result = vmaMapMemory(sAllocator, allocation, &ptr);
        result != VK_SUCCESS) {
      return tl::unexpected(
        std::system_error(make_error_code(result), "Cannot map memory"));
    }
    return static_cast<T>(ptr);
  }

  void Unmap(VkDeviceSize flushOffset = 0,
             VkDeviceSize flushSize = VK_WHOLE_SIZE) {
    if (flushSize > 0) {
      vmaFlushAllocation(sAllocator, allocation, flushOffset, flushSize);
    }
    vmaUnmapMemory(sAllocator, allocation);
  }

  Buffer() = default;
  Buffer(Buffer const&) = delete;
  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& rhs) noexcept;
  ~Buffer() noexcept;

private:
  std::string name;
}; // struct Buffer

} // namespace iris::Renderer

#endif // HEV_IRIS_RENDERER_BUFFER_H_
