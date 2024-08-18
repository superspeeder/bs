#include "setup.hpp"

#include <iostream>

#include <vulkan/vulkan.hpp>

#include <thread>
#include <vector>
#include <array>

#if __has_include("unistd.h")
#include <unistd.h>
#else
#include <chrono>
void sleep(int seconds) {
    using clock = std::chrono::system_clock;
    using time_point = std::chrono::time_point<clock>;
    using duration = std::chrono::duration<double>;

    time_point now = clock::now();

    duration dur = duration(seconds);

    while ((clock::now() - now) < dur) {
        std::this_thread::yield();
    }
}

#endif

#define IMAGE_SIZE 2048

void doGpuThings(int i, vk::Instance instance, vk::PhysicalDevice gpu) {
    // create a logical device

    auto* gc = new GraphicsContext(instance, gpu);

    Image hostImage = gc->createImageHost(IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageTiling::eLinear);
    Image deviceImage = gc->createImageDevice(IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst, vk::ImageTiling::eOptimal);
    Buffer hostBuffer = gc->createBufferHost(IMAGE_SIZE * IMAGE_SIZE * 4, vk::BufferUsageFlagBits::eTransferDst);

    gc->runCommands([&](const vk::CommandBuffer& cmd) {
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

        cmd.copyImage(deviceImage.image, vk::ImageLayout::eTransferSrcOptimal, hostImage.image, vk::ImageLayout::eTransferDstOptimal, region);

        vk::ImageMemoryBarrier imb4{};
        imb4.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        imb4.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        imb4.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        imb4.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        imb4.image = hostImage.image;
        imb4.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, imb4);

        vk::BufferImageCopy region2;
        region2.imageOffset = vk::Offset3D{0, 0, 0};
        region2.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        region2.imageExtent = vk::Extent3D(IMAGE_SIZE, IMAGE_SIZE, 1);
        region2.bufferOffset = 0;
        region2.bufferRowLength = 0;
        region2.bufferImageHeight = 0;

        cmd.copyImageToBuffer(hostImage.image, vk::ImageLayout::eTransferSrcOptimal, hostBuffer.buffer, region2);
    });

    std::stringstream ss;
    ss << "test" << i << ".png";
    std::string path = ss.str();

    gc->saveBufferImage(path, hostBuffer, IMAGE_SIZE, IMAGE_SIZE, 4, 4);

    std::cout << "Done\n";

    // sleep(15);

    gc->destroy(deviceImage);
    gc->destroy(hostImage);
    gc->destroy(hostBuffer);

    delete gc;
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
