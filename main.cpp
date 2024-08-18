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

vk::RenderPass createRenderPass(GraphicsContext* gc) {

    vk::AttachmentDescription colorAttachment = {{}, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal};

    vk::AttachmentReference colorAttachmentRef = {0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::SubpassDescription subpass = {{}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef, {}, nullptr, {}};

    vk::SubpassDependency dep = {VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite};

    vk::RenderPassCreateInfo rpci{};
    rpci.setAttachments(colorAttachment);
    rpci.setSubpasses(subpass);
    rpci.setDependencies(dep);

    return gc->getDevice().createRenderPass(rpci);
}

vk::PipelineLayout createPipelineLayout(GraphicsContext* gc) {
    vk::PipelineLayoutCreateInfo plci{};
    return gc->getDevice().createPipelineLayout(plci);
}

vk::Pipeline createPipeline(GraphicsContext* gc, vk::PipelineLayout pipelineLayout, vk::RenderPass renderPass, vk::ShaderModule vert, vk::ShaderModule frag) {
    std::vector<vk::PipelineShaderStageCreateInfo> stages = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vert, "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, frag, "main"),
    };

    std::vector<vk::DynamicState> dynamicStates{};

    vk::Viewport viewport{0.0f, 0.0f, (float)IMAGE_SIZE, (float)IMAGE_SIZE, 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {IMAGE_SIZE, IMAGE_SIZE}};

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.primitiveRestartEnable = false;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewports(viewport).setScissors(scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = false;

    vk::PipelineMultisampleStateCreateInfo multisampler{};
    multisampler.sampleShadingEnable = false;
    multisampler.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blendAttachment.blendEnable = true;
    blendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blendAttachment.colorBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo blendState{};
    std::array<float, 4> blend_constants = {0.0f, 0.0f, 0.0f, 0.0f};
    blendState.setBlendConstants(blend_constants);
    blendState.setAttachments(blendAttachment);
    blendState.logicOpEnable = false;
    blendState.logicOp = vk::LogicOp::eCopy;

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    vk::GraphicsPipelineCreateInfo gpci{};
    gpci.setStages(stages);
    gpci.pVertexInputState = &vertexInput;
    gpci.pInputAssemblyState = &inputAssembly;
    gpci.pViewportState = &viewportState;
    gpci.pRasterizationState = &rasterizer;
    gpci.pMultisampleState = &multisampler;
    gpci.pDepthStencilState = nullptr;
    gpci.pColorBlendState = &blendState;
    gpci.pDynamicState = &dynamicState;

    gpci.renderPass = renderPass;
    gpci.layout = pipelineLayout;
    gpci.subpass = 0;

    return gc->getDevice().createGraphicsPipeline(nullptr, gpci).value;
}

void doGpuThings(int i, vk::Instance instance, vk::PhysicalDevice gpu) {
    // create a logical device

    auto* gc = new GraphicsContext(instance, gpu);

    Image hostImage = gc->createImageHost(IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, vk::ImageTiling::eLinear);
    Image deviceImage = gc->createImageDevice(IMAGE_SIZE, IMAGE_SIZE, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst, vk::ImageTiling::eOptimal);
    Buffer hostBuffer = gc->createBufferHost(IMAGE_SIZE * IMAGE_SIZE * 4, vk::BufferUsageFlagBits::eTransferDst);

    vk::ShaderModule vertexShader = gc->buildShaderModule("shaders/main.vert");
    vk::ShaderModule fragmentShader = gc->buildShaderModule("shaders/main.frag");

    vk::RenderPass renderPass = createRenderPass(gc);
    vk::PipelineLayout pipelineLayout = createPipelineLayout(gc);
    vk::Pipeline pipeline = createPipeline(gc, pipelineLayout, renderPass, vertexShader, fragmentShader);

    vk::ImageView imageView = gc->createImageView(deviceImage, vk::Format::eR8G8B8A8Unorm);
    vk::Framebuffer framebuffer = gc->createFramebuffer(renderPass, imageView, {IMAGE_SIZE, IMAGE_SIZE});

    gc->runCommands([&](const vk::CommandBuffer& cmd) {
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

        std::array<vk::ClearValue, 1> clearValues = {clearColor};

        cmd.beginRenderPass(vk::RenderPassBeginInfo(renderPass, framebuffer, vk::Rect2D({0, 0}, {IMAGE_SIZE, IMAGE_SIZE}), clearValues), vk::SubpassContents::eInline);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();
    });

    gc->runCommands([&](const vk::CommandBuffer& cmd) {
        vk::ImageMemoryBarrier imb{};
        imb.oldLayout = vk::ImageLayout::eUndefined;
        imb.newLayout = vk::ImageLayout::eTransferDstOptimal;
        imb.srcAccessMask = vk::AccessFlagBits::eNone;
        imb.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        imb.image = hostImage.image;
        imb.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, imb);

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

    gc->destroy(framebuffer);
    gc->destroy(imageView);
    gc->destroy(pipeline);
    gc->destroy(pipelineLayout);
    gc->destroy(renderPass);
    gc->destroy(vertexShader);
    gc->destroy(fragmentShader);
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
