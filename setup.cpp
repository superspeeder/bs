#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"


#include "setup.hpp"

#include <iostream>

#include <fstream>

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::ate);
    auto pos = f.tellg();
    f.seekg(0);
    char* buf = new char[(size_t)pos + 1];
    memset(buf, 0, (size_t)pos + 1);
    f.read(buf, pos);
    f.close();
    std::string s = buf;
    delete[] buf;

    return s;
}

vk::Instance createInstance() {
    vk::ApplicationInfo appInfo{};
    appInfo.apiVersion = vk::ApiVersion13;

    auto ieprops = vk::enumerateInstanceExtensionProperties();
    for (const auto& pr : ieprops) {
        std::cout << "- " << pr.extensionName.data() << std::endl;
    }

    std::cout << "Layers:" << std::endl;
    auto lprs = vk::enumerateInstanceLayerProperties();
    for (const auto& pr : lprs) {
        std::cout << "- " << pr.layerName.data() << std::endl;
    }

    auto instv = vk::enumerateInstanceVersion();
    std::cout << "Instance Version: " << vk::apiVersionMajor(instv) << '.' << vk::apiVersionMinor(instv) << '.' << vk::apiVersionPatch(instv) << std::endl;

    std::vector<const char*> iextensions = {
        "VK_KHR_device_group_creation",
        "VK_KHR_get_physical_device_properties2",
        "VK_EXT_debug_utils",
    };

    std::vector<const char*> layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    vk::InstanceCreateInfo ici{};
    ici.setPEnabledExtensionNames(iextensions);
    ici.setPEnabledLayerNames(layers);
    ici.setPApplicationInfo(&appInfo);

    return vk::createInstance(ici);
}

void printGpuInfo(size_t& i, vk::PhysicalDevice gpu) {
    std::cout << "==================================================\n";
    auto props = gpu.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    auto pr2 = props.get<vk::PhysicalDeviceProperties2>();
    auto pciInfo = props.get<vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    std::cout << "Gpu #" << i++ << ": " << pr2.properties.deviceName.data() << '\n';
    std::cout << "Vendor: " << vk::to_string((vk::VendorId)pr2.properties.vendorID) << '\n';
    std::cout << "PCI bus: " << pciInfo.pciBus << '\n';
    std::cout << "Device Id: " << pr2.properties.deviceID << '\n';
}

GraphicsContext::GraphicsContext(vk::Instance instance, vk::PhysicalDevice gpu) : m_Instance(instance), m_Gpu(gpu) {
    auto props = gpu.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    m_GpuProperties = props.get<vk::PhysicalDeviceProperties2>();
    m_GpuPciInfo = props.get<vk::PhysicalDevicePCIBusInfoPropertiesEXT>();

    createDevice();
    std::cout << "Created device on " << m_GpuProperties.properties.deviceName.data() << " in PCI bus " << m_GpuPciInfo.pciBus << '\n';

    createAllocator();

    m_Pool = m_Device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 0));
}

GraphicsContext::~GraphicsContext() {
    m_Device.waitIdle();

    m_Device.destroy(m_Pool);
    vmaDestroyAllocator(m_Allocator);
    m_Device.destroy();
}

void GraphicsContext::createDevice() {
    std::vector<const char*> extensions = {
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
    };

    vk::PhysicalDeviceFeatures2 features{};

    vk::PhysicalDeviceBufferDeviceAddressFeatures bdaf{};
    bdaf.bufferDeviceAddress = true;
    features.pNext = &bdaf;

    std::array<float, 1> qp = {1.0f};
    std::vector<vk::DeviceQueueCreateInfo> dqcis{};
    dqcis.push_back(vk::DeviceQueueCreateInfo({}, 0, qp)); // we do a bit of assumptions

    m_Device = m_Gpu.createDevice(vk::DeviceCreateInfo({}, dqcis, {}, extensions, nullptr, &features));
    m_Queue = m_Device.getQueue(0, 0);
}

void GraphicsContext::createAllocator() {
    VmaAllocatorCreateInfo ci{};
    ci.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.physicalDevice = m_Gpu;
    ci.device = m_Device;
    ci.instance = m_Instance;
    ci.pVulkanFunctions = nullptr;
    vmaCreateAllocator(&ci, &m_Allocator);
}

Image GraphicsContext::createImage(const vk::ImageCreateInfo &ici, VmaAllocationCreateFlags aci_flags, vk::MemoryPropertyFlags requiredFlags, VmaMemoryUsage usage) const {
    VmaAllocationCreateInfo aci{};
    aci.flags = aci_flags;
    aci.requiredFlags = (VkMemoryPropertyFlags)requiredFlags;
    aci.usage = usage;

    VkImageCreateInfo ici_ = ici;

    Image img;
    VkImage img_;
    vmaCreateImage(m_Allocator, &ici_, &aci, &img_, &img.allocation, &img.allocationInfo);
    img.image = img_;
    return img;
}

Buffer GraphicsContext::createBuffer(const vk::BufferCreateInfo &bci, VmaAllocationCreateFlags aci_flags, vk::MemoryPropertyFlags requiredFlags, VmaMemoryUsage usage) const {
    VmaAllocationCreateInfo aci{};
    aci.flags = aci_flags;
    aci.requiredFlags = (VkMemoryPropertyFlags)requiredFlags;
    aci.usage = usage;

    VkBufferCreateInfo bci_ = bci;

    Buffer buf;
    VkBuffer buf_;
    vmaCreateBuffer(m_Allocator, &bci_, &aci, &buf_, &buf.allocation, &buf.allocationInfo);
    buf.buffer = buf_;
    return buf;
}

Image GraphicsContext::createImageHost(uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling, bool allowMapping) const {
    vk::ImageCreateInfo ici{};
    ici.format = format;
    ici.extent = vk::Extent3D(width, height, 1);
    ici.arrayLayers = 1;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = initialLayout;
    ici.mipLevels = 1;
    ici.usage = usage;
    ici.tiling = tiling;

    VmaAllocationCreateFlags acif = 0;
    if (allowMapping) acif |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    return createImage(ici, acif, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
}

Image GraphicsContext::createImageDevice(uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling) const {
    vk::ImageCreateInfo ici{};
    ici.format = format;
    ici.extent = vk::Extent3D(width, height, 1);
    ici.arrayLayers = 1;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = initialLayout;
    ici.mipLevels = 1;
    ici.usage = usage;
    ici.tiling = tiling;
    ici.sharingMode = vk::SharingMode::eExclusive;

    return createImage(ici, 0, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

Buffer GraphicsContext::createBufferHost(size_t size, vk::BufferUsageFlags usage) const {
    vk::BufferCreateInfo bci{};
    bci.usage = usage;
    bci.size = size;
    bci.sharingMode = vk::SharingMode::eExclusive;

    return createBuffer(bci, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
}

Buffer GraphicsContext::createBufferDevice(size_t size, vk::BufferUsageFlags usage) const {
    vk::BufferCreateInfo bci{};
    bci.usage = usage;
    bci.size = size;
    bci.sharingMode = vk::SharingMode::eExclusive;

    return createBuffer(bci, 0, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

vk::CommandBuffer GraphicsContext::allocateCommandBuffer() const {
    return m_Device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(m_Pool, vk::CommandBufferLevel::ePrimary, 1))[0];
}

void GraphicsContext::runCommands(const std::function<void(const vk::CommandBuffer &)> &f) {
    auto cmd = allocateCommandBuffer();
    auto fence = createFence();

    cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    f(cmd);
    cmd.end();

    submitCommands(cmd, fence);
    waitForFence(fence);

    destroy(fence);
    m_Device.freeCommandBuffers(m_Pool, cmd);
}

vk::Fence GraphicsContext::createFence() const {
    return m_Device.createFence({});
}

void GraphicsContext::waitForFence(vk::Fence fence) const {
    auto _ = m_Device.waitForFences(fence, true, UINT64_MAX);
}

void GraphicsContext::submitCommands(vk::CommandBuffer cmd, vk::Fence fence) const {
    vk::SubmitInfo si{};
    si.setCommandBuffers(cmd);
    m_Queue.submit(si, fence);
}

void GraphicsContext::destroy(const Image &image) const {
    destroy(image.image);
    freeAllocation(image.allocation);
}

void GraphicsContext::destroy(const Buffer &buffer) const {
    destroy(buffer.buffer);
    freeAllocation(buffer.allocation);
}

void GraphicsContext::freeAllocation(VmaAllocation alloc) const {
    vmaFreeMemory(m_Allocator, alloc);
}

void *GraphicsContext::mapBuffer(const Buffer &buffer) const {
    return mapMemory(buffer.allocation);
}

void *GraphicsContext::mapMemory(const vk::DeviceMemory &memory, size_t offset, size_t size, vk::MemoryMapFlags flags) const {
    return m_Device.mapMemory(memory, offset, size, flags);
}

void *GraphicsContext::mapMemory(VmaAllocation const &allocation) const {
    void* map;
    vmaMapMemory(m_Allocator, allocation, &map);
    return map;
}

void GraphicsContext::unmapBuffer(const Buffer &buffer) const {
    unmapMemory(buffer.allocation);
}

void GraphicsContext::unmapMemory(const vk::DeviceMemory &memory) const {
    m_Device.unmapMemory(memory);
}

void GraphicsContext::unmapMemory(VmaAllocation const &allocation) const {
    vmaUnmapMemory(m_Allocator, allocation);
}

void *GraphicsContext::mapImage(const Image &image) const {
    return mapMemory(image.allocation);
}

void GraphicsContext::unmapImage(const Image &image) const {
    unmapMemory(image.allocation);
}

void GraphicsContext::saveImage(const std::string &path, const Image &image, int width, int height, int channels, int bpp) const {
    void* map = mapImage(image);
    void* data = malloc(width * height * bpp);
    memcpy(data, map, width * height * bpp);
    unmapImage(image);
    saveImage(path, data, width, height, channels, bpp);
    free(data);
}

void GraphicsContext::saveBufferImage(const std::string &path, const Buffer &bufferImage, int width, int height, int channels, int bpp) const {
    void* map = mapBuffer(bufferImage);
    void* data = malloc(width * height * bpp);
    memcpy(data, map, width * height * bpp);
    unmapBuffer(bufferImage);
    saveImage(path, data, width, height, channels, bpp);
    free(data);
}

void GraphicsContext::saveImage(const std::string &path, const void *data, int width, int height, int channels, int bpp) {
    stbi_write_png(path.c_str(), width, height, channels, data, bpp);
}

std::vector<uint32_t> GraphicsContext::compileShader(const std::string &path) const {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    std::string source = readFile(path);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto res = compiler.CompileGlslToSpv(source, shaderc_glsl_infer_from_source, path.c_str(), options);
    auto status = res.GetCompilationStatus();
    if (status != shaderc_compilation_status_success) {
        std::cerr << "Error compiling shader " << path << ": " << res.GetErrorMessage() << std::endl;
        throw std::runtime_error("Error compiling shader");
    }

    return std::vector(res.cbegin(), res.cend()); // NOLINT(*-return-braced-init-list)
}

vk::ShaderModule GraphicsContext::buildShaderModule(const std::string &path) const {
    auto spirv = compileShader(path);
    return m_Device.createShaderModule(vk::ShaderModuleCreateInfo({}, spirv));
}

std::vector<uint32_t> GraphicsContext::compileShader(const std::string &path, const std::string &entry_point) const {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    std::string source = readFile(path);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto res = compiler.CompileGlslToSpv(source, shaderc_glsl_infer_from_source, path.c_str(), entry_point.c_str(), options);
    if (res.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << "Error compiling shader " << path << ": " << res.GetErrorMessage() << std::endl;
        throw std::runtime_error("Error compiling shader");
    }

    return std::vector(res.cbegin(), res.cend());
}

vk::ShaderModule GraphicsContext::buildShaderModule(const std::string &path, const std::string &entry_point) const {
    auto spirv = compileShader(path, entry_point);
    return m_Device.createShaderModule(vk::ShaderModuleCreateInfo({}, spirv));
}

vk::ImageView GraphicsContext::createImageView(const Image &image, vk::Format format) const {
    return m_Device.createImageView(vk::ImageViewCreateInfo({}, image.image, vk::ImageViewType::e2D, format, STANDARD_COMPONENT_MAPPING, STANDARD_ISR));
}

vk::Framebuffer GraphicsContext::createFramebuffer(vk::RenderPass rp, vk::ImageView iv, vk::Extent2D extent) const {
    return m_Device.createFramebuffer(vk::FramebufferCreateInfo({}, rp, iv, extent.width, extent.height, 1));
}
