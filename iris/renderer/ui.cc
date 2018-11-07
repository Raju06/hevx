#include "renderer/ui.h"
#include "absl/container/fixed_array.h"
#include "glm/gtc/type_ptr.hpp"
#include "renderer/buffer.h"
#include "renderer/image.h"
#include "renderer/impl.h"
#include "renderer/shader.h"
#include "error.h"
#include "logging.h"
#include "wsi/input.h"
#include "imgui.h"
#include <chrono>

namespace iris::Renderer::ui {

static std::string const sUIVertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(push_constant) uniform uPushConstant {
  vec2 uScale;
  vec2 uTranslate;
};
layout(location = 0) out vec4 Color;
layout(location = 1) out vec2 UV;
out gl_PerVertex {
  vec4 gl_Position;
};
void main() {
  Color = aColor;
  UV = aUV;
  gl_Position = vec4(aPos * uScale + uTranslate, 0.f, 1.f);
})";

static std::string const sUIFragmentShaderSource = R"(
#version 450 core
layout(set = 0, binding = 0) uniform sampler2D sTexture;
layout(location = 0) in vec4 Color;
layout(location = 1) in vec2 UV;
layout(location = 0) out vec4 fColor;
void main() {
  fColor = Color * texture(sTexture, UV.st);
})";

static std::vector<VkCommandBuffer> sCommandBuffers{2};
static std::uint32_t sCommandBufferIndex{0};
static VkImage sFontImage{VK_NULL_HANDLE};
static VmaAllocation sFontImageAllocation{VK_NULL_HANDLE};
static VkImageView sFontImageView{VK_NULL_HANDLE};
static VkSampler sFontImageSampler{VK_NULL_HANDLE};
static VkDeviceSize sVertexBufferSize{0};
static VkBuffer sVertexBuffer{VK_NULL_HANDLE};
static VmaAllocation sVertexBufferAllocation{VK_NULL_HANDLE};
static VkDeviceSize sIndexBufferSize{0};
static VkBuffer sIndexBuffer{VK_NULL_HANDLE};
static VmaAllocation sIndexBufferAllocation{VK_NULL_HANDLE};
static VkDescriptorSetLayout sDescriptorSetLayout{VK_NULL_HANDLE};
static std::vector<VkDescriptorSet> sDescriptorSets;
static VkPipelineLayout sPipelineLayout{VK_NULL_HANDLE};
static VkPipeline sPipeline{VK_NULL_HANDLE};

using Duration = std::chrono::duration<float, std::ratio<1, 1>>;
using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Duration>;
static TimePoint sPreviousTime;

} // namespace iris::ui::Renderer

std::error_code iris::Renderer::ui::Initialize() noexcept {
  IRIS_LOG_ENTER();
  VkResult result;

  if (auto cbs = AllocateCommandBuffers(2, VK_COMMAND_BUFFER_LEVEL_SECONDARY)) {
    sCommandBuffers = std::move(*cbs);
  } else {
    IRIS_LOG_LEAVE();
    return cbs.error();
  }

  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO& io = ImGui::GetIO();

  io.KeyMap[ImGuiKey_Tab] = static_cast<int>(wsi::Keys::kTab);
  io.KeyMap[ImGuiKey_LeftArrow] = static_cast<int>(wsi::Keys::kLeft);
  io.KeyMap[ImGuiKey_RightArrow] = static_cast<int>(wsi::Keys::kRight);
  io.KeyMap[ImGuiKey_UpArrow] = static_cast<int>(wsi::Keys::kUp);
  io.KeyMap[ImGuiKey_DownArrow] = static_cast<int>(wsi::Keys::kDown);
  io.KeyMap[ImGuiKey_PageUp] = static_cast<int>(wsi::Keys::kPageUp);
  io.KeyMap[ImGuiKey_PageDown] = static_cast<int>(wsi::Keys::kPageDown);
  io.KeyMap[ImGuiKey_Home] = static_cast<int>(wsi::Keys::kHome);
  io.KeyMap[ImGuiKey_End] = static_cast<int>(wsi::Keys::kEnd);
  io.KeyMap[ImGuiKey_Insert] = static_cast<int>(wsi::Keys::kInsert);
  io.KeyMap[ImGuiKey_Delete] = static_cast<int>(wsi::Keys::kDelete);
  io.KeyMap[ImGuiKey_Backspace] = static_cast<int>(wsi::Keys::kBackspace);
  io.KeyMap[ImGuiKey_Space] = static_cast<int>(wsi::Keys::kSpace);
  io.KeyMap[ImGuiKey_Enter] = static_cast<int>(wsi::Keys::kEnter);
  io.KeyMap[ImGuiKey_Escape] = static_cast<int>(wsi::Keys::kEscape);
  io.KeyMap[ImGuiKey_A] = static_cast<int>(wsi::Keys::kA);
  io.KeyMap[ImGuiKey_C] = static_cast<int>(wsi::Keys::kC);
  io.KeyMap[ImGuiKey_V] = static_cast<int>(wsi::Keys::kV);
  io.KeyMap[ImGuiKey_X] = static_cast<int>(wsi::Keys::kX);
  io.KeyMap[ImGuiKey_Y] = static_cast<int>(wsi::Keys::kY);
  io.KeyMap[ImGuiKey_Z] = static_cast<int>(wsi::Keys::kZ);

  if (!io.Fonts->AddFontFromFileTTF((std::string(kIRISContentDirectory) +
                                     "/assets/fonts/SourceSansPro-Regular.ttf")
                                      .c_str(),
                                    16.f)) {
    GetLogger()->error("Cannot load UI font file");
    IRIS_LOG_LEAVE();
    return Error::kInitializationFailed;
  }

  unsigned char* pixels;
  int width, height, bytes_per_pixel;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

  if (auto ti = CreateImageFromMemory(
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
        {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
         1},
        VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, pixels,
        bytes_per_pixel)) {
    std::tie(sFontImage, sFontImageAllocation) = *ti;
  } else {
    IRIS_LOG_LEAVE();
    return ti.error();
  }

  if (auto tv = CreateImageView(sFontImage, VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_VIEW_TYPE_2D,
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})) {
    sFontImageView = std::move(*tv);
  } else {
    IRIS_LOG_LEAVE();
    return tv.error();
  }

  VkSamplerCreateInfo samplerCI = {};
  samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCI.magFilter = VK_FILTER_LINEAR;
  samplerCI.minFilter = VK_FILTER_LINEAR;
  samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCI.mipLodBias = 0.f;
  samplerCI.anisotropyEnable = VK_FALSE;
  samplerCI.maxAnisotropy = 1;
  samplerCI.compareEnable = VK_FALSE;
  samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerCI.minLod = -1000.f;
  samplerCI.maxLod = 1000.f;
  samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerCI.unnormalizedCoordinates = VK_FALSE;

  result = vkCreateSampler(sDevice, &samplerCI, nullptr, &sFontImageSampler);
  if (result != VK_SUCCESS) {
    GetLogger()->error("Cannot create sampler for UI font texture: {}",
                       to_string(result));
    IRIS_LOG_LEAVE();
    return make_error_code(result);
  }

  sVertexBufferSize = 1024 * sizeof(ImDrawVert);

  if (auto vb =
        CreateBuffer(sVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VMA_MEMORY_USAGE_CPU_TO_GPU)) {
    std::tie(sVertexBuffer, sVertexBufferAllocation) = *vb;
  } else {
    IRIS_LOG_LEAVE();
    return vb.error();
  }

  sIndexBufferSize = 1024 * sizeof(ImDrawIdx);

  if (auto ib = CreateBuffer(sIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU)) {
    std::tie(sIndexBuffer, sIndexBufferAllocation) = *ib;
  } else {
    IRIS_LOG_LEAVE();
    return ib.error();
  }

  VkShaderModule vertexShader;
  if (auto vs = CreateShaderFromSource(sUIVertexShaderSource,
                                       VK_SHADER_STAGE_VERTEX_BIT)) {
    vertexShader = *vs;
  } else {
    IRIS_LOG_LEAVE();
    return vs.error();
  }

  VkShaderModule fragmentShader;
  if (auto fs = CreateShaderFromSource(sUIFragmentShaderSource,
                                       VK_SHADER_STAGE_FRAGMENT_BIT)) {
    fragmentShader = *fs;
  } else {
    IRIS_LOG_LEAVE();
    return fs.error();
  }

  absl::FixedArray<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding(1);
  descriptorSetLayoutBinding[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                   &sFontImageSampler};

  if (auto d = CreateDescriptors(descriptorSetLayoutBinding)) {
    std::tie(sDescriptorSetLayout, sDescriptorSets) = *d;
  } else {
    IRIS_LOG_LEAVE();
    return d.error();
  }

  VkDescriptorImageInfo descriptorImageI = {};
  descriptorImageI.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageI.imageView = sFontImageView;
  descriptorImageI.sampler = sFontImageSampler;

  absl::FixedArray<VkWriteDescriptorSet> writeDescriptorSets(
    sDescriptorSets.size());

  for (std::size_t i = 0; i < writeDescriptorSets.size(); ++i) {
    VkWriteDescriptorSet& writeDS = writeDescriptorSets[i];

    writeDS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDS.pNext = nullptr;
    writeDS.dstSet = sDescriptorSets[i];
    writeDS.dstBinding = 0;
    writeDS.dstArrayElement = 0;
    writeDS.descriptorCount = 1;
    writeDS.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDS.pImageInfo = &descriptorImageI;
    writeDS.pBufferInfo = nullptr;
    writeDS.pTexelBufferView = nullptr;
  }

  UpdateDescriptorSets(writeDescriptorSets);

  VkPushConstantRange pushConstantRange = {};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(glm::vec2) * 2;

  absl::FixedArray<VkPipelineShaderStageCreateInfo> shaderStageCIs(2);
  shaderStageCIs[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                       nullptr,
                       0,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       vertexShader,
                       "main",
                       nullptr};
  shaderStageCIs[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                       nullptr,
                       0,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       fragmentShader,
                       "main",
                       nullptr};

  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding = 0;
  vertexInputBindingDescription.stride = sizeof(ImDrawVert);
  vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription>
    vertexInputAttributeDescriptions(3);
  vertexInputAttributeDescriptions[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT,
                                         offsetof(ImDrawVert, pos)};
  vertexInputAttributeDescriptions[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT,
                                         offsetof(ImDrawVert, uv)};
  vertexInputAttributeDescriptions[2] = {2, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                         offsetof(ImDrawVert, col)};

  VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {};
  vertexInputStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputStateCI.vertexBindingDescriptionCount = 1;
  vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBindingDescription;
  vertexInputStateCI.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
  vertexInputStateCI.pVertexAttributeDescriptions =
    vertexInputAttributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = {};
  inputAssemblyStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportStateCI = {};
  viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateCI.viewportCount = 1;
  viewportStateCI.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizationStateCI = {};
  rasterizationStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
  rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationStateCI.lineWidth = 1.f;

  VkPipelineMultisampleStateCreateInfo multisampleStateCI = {};
  multisampleStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleStateCI.rasterizationSamples = sSurfaceSampleCount;
  multisampleStateCI.minSampleShading = 1.f;

  VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = {};
  depthStencilStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

  VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
  colorBlendAttachmentState.blendEnable = VK_TRUE;
  colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachmentState.dstColorBlendFactor =
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachmentState.srcAlphaBlendFactor =
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachmentState.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlendStateCI = {};
  colorBlendStateCI.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendStateCI.attachmentCount = 1;
  colorBlendStateCI.pAttachments = &colorBlendAttachmentState;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicStateCI = {};
  dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicStateCI.dynamicStateCount =
    static_cast<uint32_t>(dynamicStates.size());
  dynamicStateCI.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
  pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCI.setLayoutCount = 1;
  pipelineLayoutCI.pSetLayouts = &sDescriptorSetLayout;
  pipelineLayoutCI.pushConstantRangeCount = 1;
  pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

  result = vkCreatePipelineLayout(sDevice, &pipelineLayoutCI, nullptr,
                                           &sPipelineLayout);
  if (result != VK_SUCCESS) {
    GetLogger()->error("Cannot create pipeline layout: {}", to_string(result));
    IRIS_LOG_LEAVE();
    return make_error_code(result);
  }

  VkGraphicsPipelineCreateInfo graphicsPipelineCI = {};
  graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCI.stageCount = static_cast<uint32_t>(shaderStageCIs.size());
  graphicsPipelineCI.pStages = shaderStageCIs.data();
  graphicsPipelineCI.pVertexInputState = &vertexInputStateCI;
  graphicsPipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
  graphicsPipelineCI.pViewportState = &viewportStateCI;
  graphicsPipelineCI.pRasterizationState = &rasterizationStateCI;
  graphicsPipelineCI.pMultisampleState = &multisampleStateCI;
  graphicsPipelineCI.pDepthStencilState = &depthStencilStateCI;
  graphicsPipelineCI.pColorBlendState = &colorBlendStateCI;
  graphicsPipelineCI.pDynamicState = &dynamicStateCI;
  graphicsPipelineCI.layout = sPipelineLayout;
  graphicsPipelineCI.renderPass = sRenderPass;
  graphicsPipelineCI.subpass = 0;

  result = vkCreateGraphicsPipelines(sDevice, nullptr, 1, &graphicsPipelineCI,
                                     nullptr, &sPipeline);
  if (result != VK_SUCCESS) {
    vkDestroyPipelineLayout(sDevice, sPipelineLayout, nullptr);
    GetLogger()->error("Cannot create pipeline: {}", to_string(result));
    IRIS_LOG_LEAVE();
    return make_error_code(result);
  }

  vkDestroyShaderModule(sDevice, fragmentShader, nullptr);
  vkDestroyShaderModule(sDevice, vertexShader, nullptr);

  NameObject(VK_OBJECT_TYPE_IMAGE, sFontImage, "ui::sFontImage");
  NameObject(VK_OBJECT_TYPE_SAMPLER, sFontImageSampler,
             "ui::sFontImageSampler");
  NameObject(VK_OBJECT_TYPE_BUFFER, sVertexBuffer, "ui::sVertexBuffer");
  NameObject(VK_OBJECT_TYPE_BUFFER, sIndexBuffer, "ui::sIndexBuffer");
  NameObject(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, sDescriptorSetLayout,
             "ui::sDescriptorSetLayout");
  for (auto&& set : sDescriptorSets) {
    NameObject(VK_OBJECT_TYPE_DESCRIPTOR_SET, set, "ui::sDescriptorSets");
  }
  NameObject(VK_OBJECT_TYPE_PIPELINE_LAYOUT, sPipelineLayout,
             "ui::sPipelineLayout");
  NameObject(VK_OBJECT_TYPE_PIPELINE, sPipeline, "ui::sPipeline");

  return Error::kNone;
} // iris::Renderer::ui::Initialize

std::error_code
iris::Renderer::ui::BeginFrame(glm::vec2 const& windowSize,
                               glm::vec2 const& mousePos) noexcept {
  ImGuiIO& io = ImGui::GetIO();

#if 0
  io.KeyCtrl = io.KeysDown[wsi::Keys::kLeftControl] ||
               io.KeysDown[wsi::Keys::kRightControl];
  io.KeyShift =
    io.KeysDown[wsi::Keys::kLeftShift] || io.KeysDown[wsi::Keys::kRightShift];
  io.KeyAlt =
    io.KeysDown[wsi::Keys::kLeftAlt] || io.KeysDown[wsi::Keys::kRightAlt];
#endif
  if (mousePos.x > -FLT_MAX && mousePos.y > -FLT_MAX) {
    io.MousePos = {mousePos.x, mousePos.y};
  }

  TimePoint currentTime = std::chrono::steady_clock::now();
  io.DeltaTime = sPreviousTime.time_since_epoch().count() > 0
                   ? (currentTime - sPreviousTime).count()
                   : 1.f / 60.f;
  sPreviousTime = currentTime;

  io.DisplaySize = {windowSize.x, windowSize.y};
  io.DisplayFramebufferScale = {0.f, 0.f};

  ImGui::NewFrame();

  return Error::kNone;
} // iris::Renderer::ui::Frame

tl::expected<VkCommandBuffer, std::error_code>
iris::Renderer::ui::EndFrame(VkFramebuffer framebuffer) noexcept {
  VkResult result;

  ImGui::EndFrame();
  ImGui::Render();

  ImDrawData* drawData = ImGui::GetDrawData();
  if (drawData->TotalVtxCount == 0) return VkCommandBuffer{VK_NULL_HANDLE};

  VkDeviceSize newBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
  if (newBufferSize > sVertexBufferSize) {
    if (auto newVB =
          CreateBuffer(newBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VMA_MEMORY_USAGE_CPU_TO_GPU)) {
      vmaDestroyBuffer(sAllocator, sVertexBuffer, sVertexBufferAllocation);
      std::tie(sVertexBuffer, sVertexBufferAllocation) = *newVB;
      sVertexBufferSize = newBufferSize;
      NameObject(VK_OBJECT_TYPE_BUFFER, sVertexBuffer, "ui::sVertexBuffer");
    } else {
      GetLogger()->error("Cannot resize vertex buffer: {}",
                         newVB.error().message());
      return tl::unexpected(newVB.error());
    }
  }

  newBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
  if (newBufferSize > sIndexBufferSize) {
    if (auto newIB =
          CreateBuffer(newBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VMA_MEMORY_USAGE_CPU_TO_GPU)) {
      vmaDestroyBuffer(sAllocator, sIndexBuffer, sIndexBufferAllocation);
      std::tie(sIndexBuffer, sIndexBufferAllocation) = *newIB;
      sIndexBufferSize = newBufferSize;
      NameObject(VK_OBJECT_TYPE_BUFFER, sIndexBuffer, "ui::sIndexBuffer");
    } else {
      GetLogger()->error("Cannot resize index buffer: {}",
                         newIB.error().message());
      return tl::unexpected(newIB.error());
    }
  }

  ImDrawVert* pVerts;
  if (auto p = MapMemory(sVertexBufferAllocation)) {
    pVerts = reinterpret_cast<ImDrawVert*>(*p);
  } else {
    GetLogger()->error("Cannot map staging buffer: {}", p.error().message());
    return tl::unexpected(p.error());
  }

  ImDrawIdx* pIndxs;
  if (auto p = MapMemory(sIndexBufferAllocation)) {
    pIndxs = reinterpret_cast<ImDrawIdx*>(*p);
  } else {
    GetLogger()->error("Cannot map staging buffer: {}", p.error().message());
    return tl::unexpected(p.error());
  }

  for (int i = 0; i < drawData->CmdListsCount; ++i) {
    ImDrawList const* cmdList = drawData->CmdLists[i];
    std::memcpy(pVerts, cmdList->VtxBuffer.Data,
                cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
    std::memcpy(pIndxs, cmdList->IdxBuffer.Data,
                cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
    pVerts += cmdList->VtxBuffer.Size;
    pIndxs += cmdList->IdxBuffer.Size;
  }

  UnmapMemory(sVertexBufferAllocation, 0, VK_WHOLE_SIZE);
  UnmapMemory(sIndexBufferAllocation, 0, VK_WHOLE_SIZE);

  absl::FixedArray<VkClearValue> clearValues(4);
  clearValues[sColorTargetAttachmentIndex].color = {0, 0, 0, 0};
  clearValues[sDepthStencilTargetAttachmentIndex].depthStencil = {1.f, 0};

  sCommandBufferIndex = (sCommandBufferIndex + 1) % sCommandBuffers.size();
  auto&& cb = sCommandBuffers[sCommandBufferIndex];

  VkCommandBufferInheritanceInfo inheritanceInfo = {};
  inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritanceInfo.renderPass = sRenderPass;
  inheritanceInfo.framebuffer = framebuffer;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT |
                    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  beginInfo.pInheritanceInfo = &inheritanceInfo;

  result = vkBeginCommandBuffer(cb, &beginInfo);
  if (result != VK_SUCCESS) {
    GetLogger()->error("Cannot begin UI command buffer: {}", to_string(result));
    return tl::unexpected(make_error_code(result));
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, sPipeline);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, sPipelineLayout,
                          0, 1, &sDescriptorSets[0], 0, nullptr);

  VkDeviceSize bindingOffset = 0;
  vkCmdBindVertexBuffers(cb, 0, 1, &sVertexBuffer, &bindingOffset);
  vkCmdBindIndexBuffer(cb, sIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

  glm::vec2 const displaySize{drawData->DisplaySize.x, drawData->DisplaySize.y};
  glm::vec2 const displayPos{drawData->DisplayPos.x, drawData->DisplayPos.y};

  VkViewport viewport = {0, 0, displaySize.x, displaySize.y, 0.f, 1.f};
  vkCmdSetViewport(cb, 0, 1, &viewport);

  glm::vec2 const scale = glm::vec2{2.f, 2.f} / displaySize;
  glm::vec2 const translate = glm::vec2{-1.f, -1.f} - displayPos * scale;

  vkCmdPushConstants(cb, sPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(glm::vec2), glm::value_ptr(scale));
  vkCmdPushConstants(cb, sPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                     sizeof(glm::vec2), sizeof(glm::vec2),
                     glm::value_ptr(translate));

  for (int i = 0, idxOff = 0, vtxOff = 0; i < drawData->CmdListsCount; ++i) {
    ImDrawList* cmdList = drawData->CmdLists[i];

    for (int j = 0; j < cmdList->CmdBuffer.size(); ++j) {
      ImDrawCmd const* drawCmd = &cmdList->CmdBuffer[j];

      if (drawCmd->UserCallback) {
        drawCmd->UserCallback(cmdList, drawCmd);
      } else {
        VkRect2D scissor;
        scissor.offset.x = (int32_t)(drawCmd->ClipRect.x - displayPos.x) > 0
                             ? (int32_t)(drawCmd->ClipRect.x - displayPos.x)
                             : 0;
        scissor.offset.y = (int32_t)(drawCmd->ClipRect.y - displayPos.y) > 0
                             ? (int32_t)(drawCmd->ClipRect.y - displayPos.y)
                             : 0;
        scissor.extent.width =
          (uint32_t)(drawCmd->ClipRect.z - drawCmd->ClipRect.x);
        scissor.extent.height = (uint32_t)(
          drawCmd->ClipRect.w - drawCmd->ClipRect.y + 1); // FIXME: Why +1 here?

        vkCmdSetScissor(cb, 0, 1, &scissor);
        vkCmdDrawIndexed(cb, drawCmd->ElemCount, 1, idxOff, vtxOff, 0);
      }

      idxOff += drawCmd->ElemCount;
    }

    vtxOff += cmdList->VtxBuffer.Size;
  }

  result = vkEndCommandBuffer(cb);
  if (result != VK_SUCCESS) {
    GetLogger()->error("Cannot end UI command buffer: {}", to_string(result));
    return tl::unexpected(make_error_code(result));
  }

  return cb;
} // iris::Renderer::ui::EndFrame

void iris::Renderer::ui::Shutdown() noexcept {
  ImGui::DestroyContext();

  if (sPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(sDevice, sPipeline, nullptr);
  }

  if (sPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(sDevice, sPipelineLayout, nullptr);
  }

  if (sDescriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(sDevice, sDescriptorSetLayout, nullptr);
  }

  if (sIndexBuffer != VK_NULL_HANDLE &&
      sIndexBufferAllocation != VK_NULL_HANDLE) {
    vmaDestroyBuffer(sAllocator, sIndexBuffer, sIndexBufferAllocation);
  }

  if (sVertexBuffer != VK_NULL_HANDLE &&
      sVertexBufferAllocation != VK_NULL_HANDLE) {
    vmaDestroyBuffer(sAllocator, sVertexBuffer, sVertexBufferAllocation);
  }

  if (sFontImageSampler != VK_NULL_HANDLE) {
    vkDestroySampler(sDevice, sFontImageSampler, nullptr);
  }

  if (sFontImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(sDevice, sFontImageView, nullptr);
  }

  if (sFontImage != VK_NULL_HANDLE && sFontImageAllocation != VK_NULL_HANDLE) {
    vmaDestroyImage(sAllocator, sFontImage, sFontImageAllocation);
  }
} // iris::Renderer::ui::Shutdown
