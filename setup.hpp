#pragma once
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

#include <vector>
#include <functional>
#include <array>
#include <string>

vk::Instance createInstance();
void printGpuInfo(size_t& i, vk::PhysicalDevice gpu);

struct Image {
    vk::Image image;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
};

struct Buffer {
    vk::Buffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
};

template<typename T>
concept inst_destruct = requires(const T& v, vk::Instance inst) {
    inst.destroy(v);
};

template<typename T>
concept dev_destruct = requires(const T& v, vk::Device dev) {
    dev.destroy(v);
};

class GraphicsContext {
  public:
    GraphicsContext(vk::Instance instance, vk::PhysicalDevice gpu);
    ~GraphicsContext();

    [[nodiscard]] Image createImage(const vk::ImageCreateInfo &ici, VmaAllocationCreateFlags aci_flags, vk::MemoryPropertyFlags requiredFlags, VmaMemoryUsage usage) const;
    [[nodiscard]] Buffer createBuffer(const vk::BufferCreateInfo &bci, VmaAllocationCreateFlags aci_flags, vk::MemoryPropertyFlags requiredFlags, VmaMemoryUsage usage) const;

    [[nodiscard]] Image createImageHost(uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling, bool allowMapping = false) const;
    [[nodiscard]] Image createImageDevice(uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling) const;

    // host buffers are mappable to read/write
    [[nodiscard]] Buffer createBufferHost(size_t size, vk::BufferUsageFlags usage) const;
    [[nodiscard]] Buffer createBufferDevice(size_t size, vk::BufferUsageFlags usage) const;

    void runCommands(const std::function<void(const vk::CommandBuffer& cmd)>& f);

    vk::CommandBuffer allocateCommandBuffer() const;

    vk::Fence createFence() const;

    void waitForFence(vk::Fence fence) const;

    void submitCommands(vk::CommandBuffer cmd, vk::Fence fence) const;

    template<inst_destruct T>
    void destroy(const T& v) const {
        m_Instance.destroy(v);
    };

    template<dev_destruct T>
    void destroy(const T& v) const {
        m_Device.destroy(v);
    };

    void destroy(const Image&) const;
    void destroy(const Buffer&) const;

    void freeAllocation(VmaAllocation alloc) const;

    [[nodiscard]] void* mapBuffer(const Buffer &buffer) const;
    [[nodiscard]] void* mapImage(const Image &image) const;
    [[nodiscard]] void* mapMemory(const vk::DeviceMemory& memory, size_t offset, size_t size, vk::MemoryMapFlags flags) const;
    [[nodiscard]] void* mapMemory(const VmaAllocation& allocation) const;

    void unmapBuffer(const Buffer &buffer) const;
    void unmapImage(const Image &image) const;
    void unmapMemory(const vk::DeviceMemory &memory) const;
    void unmapMemory(const VmaAllocation& allocation) const;


    // assumes image/buffer is host side and mappable
    void saveImage(const std::string& path, const Image& image, int width, int height, int channels, int bpp) const;
    void saveBufferImage(const std::string& path, const Buffer& bufferImage, int width, int height, int channels, int bpp) const;

    static void saveImage(const std::string& path, const void* data, int width, int height, int channels, int bpp);

  private:
    vk::Instance m_Instance;
    vk::PhysicalDevice m_Gpu;
    vk::Device m_Device;
    vk::Queue m_Queue;
    vk::CommandPool m_Pool;
    VmaAllocator m_Allocator;

    vk::PhysicalDeviceProperties2 m_GpuProperties;
    vk::PhysicalDevicePCIBusInfoPropertiesEXT m_GpuPciInfo;

    void createDevice();
    void createAllocator();
};