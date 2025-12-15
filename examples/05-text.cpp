#define DEBUG_DRAW_IMPLEMENTATION
#include <debug_draw.hpp>

#include <cstring>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>


#pragma comment(lib, "vulkan-1.lib")

// Helper macros for error checking
#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      std::cout << "Detected Vulkan error: " << err << std::endl;              \
      abort();                                                                 \
    }                                                                          \
  } while (0)

class VulkanRenderInterface : public dd::RenderInterface {
public:
  VkDevice device;
  VkCommandBuffer cmdBuffer;

  // Pipelines (created elsewhere in real app)
  VkPipeline pointPipeline = VK_NULL_HANDLE;
  VkPipeline linePipeline = VK_NULL_HANDLE;
  VkPipeline textPipeline = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  // Buffers
  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
  size_t vertexBufferSize = 1024 * 1024;
  size_t currentOffset = 0;
  void *mappedPtr = nullptr;

  // Glyph Texture
  VkImage fontImage = VK_NULL_HANDLE;
  VkImageView fontView = VK_NULL_HANDLE;
  VkSampler fontSampler = VK_NULL_HANDLE;
  VkDescriptorSet fontDescriptorSet = VK_NULL_HANDLE;

  VulkanRenderInterface(VkDevice dev, VkCommandBuffer cmd)
      : device(dev), cmdBuffer(cmd) {
    // Allocation logic would go here
    // For example purposes we assume resources are valid
    createBuffers();
  }

  ~VulkanRenderInterface() {
    if (vertexBuffer)
      vkDestroyBuffer(device, vertexBuffer, nullptr);
    if (vertexMemory)
      vkFreeMemory(device, vertexMemory, nullptr);
    // Clean up others..
  }

  void createBuffers() {
    // Create a host visible buffer for dynamic vertices
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = vertexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);

    // Allocate memory (Stub memory type index)
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = vertexBufferSize;
    allocInfo.memoryTypeIndex =
        0; // FindMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    vkAllocateMemory(device, &allocInfo, nullptr, &vertexMemory);
    vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0);

    vkMapMemory(device, vertexMemory, 0, vertexBufferSize, 0, &mappedPtr);
  }

  void drawPointList(const dd::DrawVertex *points, int count,
                     bool depthEnabled) override {
    uploadAndDraw(points, count, pointPipeline);
  }

  void drawLineList(const dd::DrawVertex *lines, int count,
                    bool depthEnabled) override {
    uploadAndDraw(lines, count, linePipeline);
  }

  void uploadAndDraw(const dd::DrawVertex *verts, int count,
                     VkPipeline pipeline) {
    size_t dataSize = count * sizeof(dd::DrawVertex);
    if (currentOffset + dataSize > vertexBufferSize)
      currentOffset = 0;

    memcpy((uint8_t *)mappedPtr + currentOffset, verts, dataSize);

    // Flush range if non-coherent memory
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = vertexMemory;
    range.offset = currentOffset;
    range.size = dataSize;
    vkFlushMappedMemoryRanges(device, 1, &range);

    if (pipeline)
      vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = currentOffset;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);

    // Push Constants for camera ViewProj could go here
    // vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
    // 0, sizeof(mat4), &viewProj);

    vkCmdDraw(cmdBuffer, count, 1, 0, 0);

    currentOffset += dataSize;
  }

  // Text
  dd::GlyphTextureHandle createGlyphTexture(int width, int height,
                                            const void *pixels) override {
    std::cout << "[Vulkan] Creating Glyph Texture: " << width << "x" << height
              << "\n";
    // Real logic: Create VkImage, upload pixels via staging buffer, transition
    // layout
    return new int(1); // Dummy
  }

  void destroyGlyphTexture(dd::GlyphTextureHandle glyphTex) override {
    delete (int *)glyphTex;
  }

  void drawGlyphList(const dd::DrawVertex *glyphs, int count,
                     dd::GlyphTextureHandle glyphTex) override {
    if (textPipeline)
      vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        textPipeline);
    if (fontDescriptorSet)
      vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout, 0, 1, &fontDescriptorSet, 0,
                              nullptr);

    // Use same upload logic as lines/points
    size_t dataSize = count * sizeof(dd::DrawVertex);
    if (currentOffset + dataSize > vertexBufferSize)
      currentOffset = 0;
    memcpy((uint8_t *)mappedPtr + currentOffset, glyphs, dataSize);

    VkDeviceSize offset = currentOffset;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);

    vkCmdDraw(cmdBuffer, count, 1, 0, 0);
    currentOffset += dataSize;
  }
};

int main() {
  // Stub Vulkan Instance creation
  VkInstance instance;
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "DebugDraw Vulkan";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  vkCreateInstance(&createInfo, nullptr, &instance);

  // Mock Device/CmdBuffer (In real app, create these)
  VkDevice device = VK_NULL_HANDLE;
  VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;

  // We can't really run this without a real device, so just show compileability
  // VulkanRenderInterface renderer(device, cmdBuffer);
  // dd::initialize(&renderer);

  // ... drawing calls ...

  // dd::flush(0);
  // dd::shutdown();

  vkDestroyInstance(instance, nullptr);

  return 0;
}
