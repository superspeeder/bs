#include <iostream>

#include <vulkan/vulkan.hpp>

#include <thread>
#include <vector>
#include <array>

#include <unistd.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define IMAGE_SIZE 2048

#include <functional>



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

vk::Device createDevice(vk::PhysicalDevice gpu) {
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
    
    std::array<float, 1> qp = {1.0f};
    std::vector<vk::DeviceQueueCreateInfo> dqcis{};
    dqcis.push_back(vk::DeviceQueueCreateInfo({}, 0, qp)); // we do a bit of assumptions


    return gpu.createDevice(vk::DeviceCreateInfo({}, dqcis, {}, extensions, nullptr, &features));
}

VmaAllocator createAllocator(vk::Instance instance, vk::PhysicalDevice gpu, vk::Device device) {
    VmaAllocatorCreateInfo ci{};
    ci.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.physicalDevice = gpu;
    ci.device = device;
    ci.instance = instance;
    ci.pVulkanFunctions = nullptr;
    VmaAllocator alloc;
    vmaCreateAllocator(&ci, &alloc);
    return alloc;
}

struct ImageAlloc {
    vk::Image image;
    VmaAllocation alloc;
    VmaAllocationInfo info;
};

struct BufferAlloc {
    vk::Buffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo info;
};

ImageAlloc createImageHost(VmaAllocator allocator, uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling) {
    vk::ImageCreateInfo ici{};
    ici.format = format;
    ici.extent = vk::Extent3D(width, height, 1);
    ici.arrayLayers = 1;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = initialLayout;
    ici.mipLevels = 1;
    ici.usage = usage;
    ici.tiling = tiling;
    ImageAlloc alloc{};

    VkImageCreateInfo ici_ = ici;
    VkImage img;

    VmaAllocationCreateInfo aci{};
    aci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    vmaCreateImage(allocator, &ici_, &aci, &img, &alloc.alloc, &alloc.info);

    alloc.image = img;
    return alloc;
}

ImageAlloc createImageDevice(VmaAllocator allocator, uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling) {
    vk::ImageCreateInfo ici{};
    ici.format = format;
    ici.extent = vk::Extent3D(width, height, 1);
    ici.arrayLayers = 1;
    ici.imageType = vk::ImageType::e2D;
    ici.initialLayout = initialLayout;
    ici.mipLevels = 1;
    ici.usage = usage;
    ici.tiling = tiling;
    ImageAlloc alloc{};

    VkImageCreateInfo ici_ = ici;
    VkImage img;

    VmaAllocationCreateInfo aci{};
    aci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaCreateImage(allocator, &ici_, &aci, &img, &alloc.alloc, &alloc.info);

    alloc.image = img;
    return alloc;
}

void createRenderPass(vk::Device device) {
}


void doGpuThings(int i, vk::Instance instance, vk::PhysicalDevice gpu) {
    // create a logical device
    auto props = gpu.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDevicePCIBusInfoPropertiesEXT>();
    auto pr2 = props.get<vk::PhysicalDeviceProperties2>();
    auto pciInfo = props.get<vk::PhysicalDevicePCIBusInfoPropertiesEXT>();

    vk::Device device = createDevice(gpu);
    vk::Queue queue = device.getQueue(0, 0);
    vk::CommandPool pool = device.createCommandPool(vk::CommandPoolCreateInfo({}, 0));
    vk::CommandBuffer cmd = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1))[0];

    std::cout << "Created device on " << pr2.properties.deviceName.data() << " in PCI bus " << pciInfo.pciBus << '\n';

    VmaAllocator allocator = createAllocator(instance, gpu, device);

    ImageAlloc hostImage = createImageHost(allocator, IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageTiling::eLinear);
    ImageAlloc deviceImage = createImageDevice(allocator, IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst, vk::ImageTiling::eOptimal);
    // BufferAlloc hostBuffer = createBufferHost(allocator, 4096 * 4096 * 4);

    vk::Fence fence = device.createFence(vk::FenceCreateInfo());

    cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    vk::ImageMemoryBarrier imb1{};
    imb1.oldLayout = vk::ImageLayout::eUndefined;
    imb1.newLayout = vk::ImageLayout::eTransferDstOptimal;
    imb1.srcAccessMask = vk::AccessFlagBits::eNone;
    imb1.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    imb1.image = deviceImage.image;
    imb1.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, imb1);

    vk::ImageSubresourceRange range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::ClearColorValue clearColor;
    switch (i) {
        case 0:
            clearColor = vk::ClearColorValue(1.0f, 0.0f, 0.0f, 1.0f);
            break;
        case 1:
            clearColor = vk::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case 2:
            clearColor = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 3:
            clearColor = vk::ClearColorValue(1.0f, 0.0f, 1.0f, 1.0f);
            break;
        default:
            clearColor = vk::ClearColorValue(1.0f, 1.0f, 0.0f, 1.0f);
            break;
    }

    cmd.clearColorImage(deviceImage.image, vk::ImageLayout::eTransferDstOptimal, clearColor, range);

    vk::ImageMemoryBarrier imb2{};
    imb2.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imb2.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    imb2.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    imb2.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    imb2.image = deviceImage.image;
    imb2.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    vk::ImageMemoryBarrier imb3{};
    imb3.oldLayout = vk::ImageLayout::eUndefined;
    imb3.newLayout = vk::ImageLayout::eTransferDstOptimal;
    imb3.srcAccessMask = vk::AccessFlagBits::eNone;
    imb3.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    imb3.image = hostImage.image;
    imb3.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);


    std::vector<vk::ImageMemoryBarrier> imbs = {
        imb2, imb3        
    };
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, imbs);

    vk::ImageCopy region;
    region.srcOffset = vk::Offset3D{0, 0, 0};
    region.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    region.extent = vk::Extent3D(IMAGE_SIZE, IMAGE_SIZE, 1);
    region.dstOffset = vk::Offset3D{0, 0, 0};
    region.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

    std::cout << "hostImage.image " << (size_t)((VkImage)hostImage.image) << std::endl;

    cmd.copyImage(deviceImage.image, vk::ImageLayout::eTransferSrcOptimal, hostImage.image, vk::ImageLayout::eTransferDstOptimal, region);

    cmd.end();

    std::cout << "pre Submit\n";
    vk::SubmitInfo si{};
    si.setCommandBuffers(cmd);
    queue.submit(si, fence);
    std::cout << "Submit\n";

    device.waitForFences(fence, true, UINT64_MAX);
    std::cout << "done render\n";

    void* map = nullptr;
    vmaMapMemory(allocator, hostImage.alloc, &map);
    std::cout << "Mapped\n";

    std::cout << "map: " << (size_t)map << std::endl;

    void* data = malloc(IMAGE_SIZE * IMAGE_SIZE * 4);
    memcpy(data, map, IMAGE_SIZE * IMAGE_SIZE * 4);
    vmaUnmapMemory(allocator, hostImage.alloc);

    std::string s = "test" + std::to_string(i) + ".png";
    stbi_write_png(s.c_str(), IMAGE_SIZE, IMAGE_SIZE, 4, data, 4);
    std::cout << "Wrote\n";
    
    free(data);

    std::cout << "Done\n";

    // sleep(15);

    device.destroy(pool);
    device.destroy(deviceImage.image);
    device.destroy(hostImage.image);
    device.destroy(fence);
    vmaFreeMemory(allocator, deviceImage.alloc);
    vmaFreeMemory(allocator, hostImage.alloc);
    vmaDestroyAllocator(allocator);

    device.destroy();
}

int main() {
    std::cout << "Hello!" << std::endl;

    auto instance = createInstance();

    std::vector<std::thread> threads;

    size_t i = 0;
    auto physicalDevices = instance.enumeratePhysicalDevices();
    for (auto gpu : physicalDevices) {
        int i_ = i;
        printGpuInfo(i, gpu);
        vk::PhysicalDeviceProperties p = gpu.getProperties();
        if (p.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
            std::cout << p.deviceName.data() << " is not a real gpu :(\n";
            continue;
        }
        threads.push_back(std::thread(doGpuThings, i_, instance, gpu));
    }
    
    // wait for threads to end
    for (auto& t : threads) {
        t.join();
    }

    instance.destroy();

    return 0;
}
