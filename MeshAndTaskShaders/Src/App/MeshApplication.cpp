#include "MeshApplication.h"
#include "Model/MatrixBuffer.h"
#include "Model/Shaders/ShaderData.h"
#include "Model/Shaders/ShaderLoader.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "Vk/GraphicsPipeline/GraphicsPipelineBuilder.h"
#include "Log/Log.h"
#include "Vk/Swapchain.h"
#include "Vk/Utils.h"
#include "Vk/Vertex/VertexAttributeBuilder.h"
#include "glm/ext/vector_float3.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "Mesh/MeshletGeneration.h"
#include "vulkan/vulkan_enums.hpp"
#include <cstdint>
#include <stddef.h>

void MeshApplication::PostInitVulkan()
{

    vkCmdDrawMeshTasksNv = (PFN_vkCmdDrawMeshTasksNV)vkGetDeviceProcAddr(*m_Device, "vkCmdDrawMeshTasksNV");

    m_Camera = Camera({0.f, 0.f, -3.f}, {0.f, 0.f, 0.f}, (float)m_WinWidth / m_WinHeight);

    InitializeMeshPipeline();
    InitializeAxisPipeline();

    // Frame buffers
    std::vector<vk::ImageView> imageViews = m_Device.GetSwapchain()->GetImageViews();

    TRY_CATCH_BEGIN()

    for (const vk::ImageView& view : imageViews)
    {

        vk::FramebufferCreateInfo createInfo{};
        createInfo.setWidth(m_WinWidth)
            .setHeight(m_WinHeight)
            .setLayers(1)
            .setRenderPass(m_RenderPass.GetVkRenderPass())
            .setAttachments(view);

        m_SwapchainFramebuffers.emplace_back(m_Device.CreateFrameBuffer(createInfo));
    }

    TRY_CATCH_END()

    vk::CommandPoolCreateInfo createInfo{vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                         m_PhysicalDevice.GetQueueFamilyIndices().m_GraphicsFamily.value()};

    // Command Buffers
    m_CommandPool = m_Device.CreateCommandPool(createInfo);

    vk::CommandBufferAllocateInfo allocateInfo{};

    allocateInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_CommandPool)
        .setCommandBufferCount(m_Device.GetSwapchain()->GetImageViews().size());

    TRY_CATCH_BEGIN()

    m_CommandBuffers = m_Device.AllocateCommandBuffers(allocateInfo);

    TRY_CATCH_END()

    // Sync Objects

    vk::FenceCreateInfo fenceCreateInfo{vk::FenceCreateFlagBits::eSignaled};

    vk::SemaphoreCreateInfo imageAvailableCreateInfo{};
    vk::SemaphoreCreateInfo renderFinishedCreateInfo{};

    TRY_CATCH_BEGIN()

    for (int i = 0; i < m_Device.GetSwapchain()->GetNumberOfSwapBuffers(); i++)
    {
        m_ImageAvailableSemaphores.emplace_back(m_Device.CreateSemaphore(imageAvailableCreateInfo));
        m_RenderFinishedSemaphores.emplace_back(m_Device.CreateSemaphore(renderFinishedCreateInfo));

        m_InFlightFences.emplace_back(m_Device.CreateFence(fenceCreateInfo));
    }

    TRY_CATCH_END()
}

void MeshApplication::InitializeMeshPipeline()
{

    m_Meshlets = MeshletGeneration::MeshletizeUnoptimized(4, 6, m_CubeIndices);

    const std::vector<VkCore::ShaderData> shaders =
        VkCore::ShaderLoader::LoadMeshShaders("MeshAndTaskShaders/Res/Shaders/mesh_shading");

    m_VertexBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eStorageBuffer);
    m_VertexBuffer.InitializeOnGpu(m_CubeVerticesFloats.data(), m_CubeVerticesFloats.size() * sizeof(float));

    m_MeshletBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eStorageBuffer);
    m_MeshletBuffer.InitializeOnGpu(m_Meshlets.data(), m_Meshlets.size() * sizeof(Meshlet));

    // Create Buffers
    for (int i = 0; i < m_Device.GetSwapchain()->GetImageCount(); i++)
    {
        VkCore::Buffer matBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eUniformBuffer);
        matBuffer.InitializeOnCpu(sizeof(MatrixBuffer));

        m_MatBuffers.emplace_back(std::move(matBuffer));
    }

    // Mesh Descriptor Sets
    VkCore::DescriptorBuilder descriptorBuilder(m_Device);

    vk::DescriptorSet tempSet;

    for (uint32_t i = 0; i < m_Device.GetSwapchain()->GetImageCount(); i++)
    {
        descriptorBuilder.BindBuffer(0, m_MatBuffers[i], vk::DescriptorType::eUniformBuffer,
                                     vk::ShaderStageFlagBits::eMeshNV | vk::ShaderStageFlagBits::eVertex);
        descriptorBuilder.Build(tempSet, m_MatrixDescSetLayout);
        descriptorBuilder.Clear();

        m_MatrixDescriptorSets.emplace_back(tempSet);
    }

    descriptorBuilder.Clear();

    descriptorBuilder.BindBuffer(0, m_VertexBuffer, vk::DescriptorType::eStorageBuffer,
                                 vk::ShaderStageFlagBits::eMeshNV);
    descriptorBuilder.BindBuffer(1, m_MeshletBuffer, vk::DescriptorType::eStorageBuffer,
                                 vk::ShaderStageFlagBits::eMeshNV);

    descriptorBuilder.Build(m_MeshDescSet, m_MeshDescSetLayout);

    // Pipeline
    VkCore::GraphicsPipelineBuilder pipelineBuilder(m_Device, true);

    m_MeshPipeline = pipelineBuilder.BindShaderModules(shaders)
                         .BindRenderPass(m_RenderPass.GetVkRenderPass())
                         .AddViewport(glm::uvec4(0, 0, m_WinWidth, m_WinHeight))
                         .FrontFaceDirection(vk::FrontFace::eCounterClockwise)
                         .SetCullMode(vk::CullModeFlagBits::eBack)
                         .AddDisabledBlendAttachment()
                         .AddDescriptorLayout(m_MatrixDescSetLayout)
                         .AddDescriptorLayout(m_MeshDescSetLayout)
                         .SetPrimitiveAssembly(vk::PrimitiveTopology::eTriangleList)
                         .Build(m_MeshPipelineLayout);
}

void MeshApplication::InitializeAxisPipeline()
{

    const float m_AxisVertexData[12] = {
        0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
    };

    const uint32_t m_AxisIndexData[6] = {
        0, 1, 0, 2, 0, 3,
    };

    m_AxisBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eVertexBuffer);
    m_AxisBuffer.InitializeOnGpu(&m_AxisVertexData, sizeof(float) * 12);

    m_AxisIndexBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eIndexBuffer);
    m_AxisIndexBuffer.InitializeOnGpu(&m_AxisIndexData, sizeof(uint32_t) * 6);

    VkCore::VertexAttributeBuilder attributeBuilder{};

    attributeBuilder.PushAttribute<float>(3);
    attributeBuilder.SetBinding(0);

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("MeshAndTaskShaders/Res/Shaders/axis");
    VkCore::GraphicsPipelineBuilder pipelineBuilder(m_Device);

    m_AxisPipeline = pipelineBuilder.BindShaderModules(shaderData)
                        .SetLineWidth(3.f)
                         .BindRenderPass(m_RenderPass.GetVkRenderPass())
                         .AddViewport(glm::uvec4(0, 0, m_WinWidth, m_WinHeight))
                         .FrontFaceDirection(vk::FrontFace::eCounterClockwise)
                         .SetCullMode(vk::CullModeFlagBits::eBack)
                         .BindVertexAttributes(attributeBuilder)
                         .AddDisabledBlendAttachment()
                         .AddDescriptorLayout(m_MatrixDescSetLayout)
                         .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                         .Build(m_AxisPipelineLayout);
}

void MeshApplication::DrawFrame()
{

    m_Device.WaitForFences(m_InFlightFences[m_CurrentFrame], false);

    uint32_t imageIndex;
    vk::ResultValue<uint32_t> result = m_Device.AcquireNextImageKHR(m_ImageAvailableSemaphores[m_CurrentFrame]);

    VkCore::Utils::CheckVkResult(result.result);

    imageIndex = result.value;

    m_Device.ResetFences(m_InFlightFences[m_CurrentFrame]);
    m_CommandBuffers[m_CurrentFrame].reset();

    RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(m_CommandBuffers[m_CurrentFrame])
        .setWaitSemaphores(m_ImageAvailableSemaphores[m_CurrentFrame])
        .setWaitDstStageMask(dstStageMask)
        .setSignalSemaphores(m_RenderFinishedSemaphores[m_CurrentFrame]);

    TRY_CATCH_BEGIN()

    m_Device.GetGraphicsQueue().submit(submitInfo, m_InFlightFences[m_CurrentFrame]);

    TRY_CATCH_END()

    vk::SwapchainKHR swapchain = m_Device.GetSwapchain()->GetVkSwapchain();

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(m_RenderFinishedSemaphores[m_CurrentFrame])
        .setSwapchains(swapchain)
        .setImageIndices(imageIndex)
        .setPResults(nullptr);

    VkCore::Utils::CheckVkResult(m_Device.GetPresentQueue().presentKHR(presentInfo));

    m_CurrentFrame = (m_CurrentFrame + 1) % m_Device.GetSwapchain()->GetNumberOfSwapBuffers();
}

void MeshApplication::RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, const uint32_t imageIndex)
{
    m_Camera.Update();

    MatrixBuffer ubo{};
    ubo.m_Proj = m_Camera.GetProjMatrix();
    ubo.m_View = m_Camera.GetViewMatrix();

    m_MatBuffers[imageIndex].UpdateData(&ubo);

    vk::CommandBufferBeginInfo cmdBufferBeginInfo{};

    vk::ClearValue clearValue{};
    clearValue.setColor({.0f, .0f, 0.f, 1.f});

    vk::RenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.setRenderPass(m_RenderPass.GetVkRenderPass())
        .setRenderArea(vk::Rect2D({0, 0}, {m_WinWidth, m_WinHeight}))
        .setFramebuffer(m_SwapchainFramebuffers[imageIndex])
        .setClearValues(clearValue);

    TRY_CATCH_BEGIN()

    commandBuffer.begin(cmdBufferBeginInfo);

    {
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_MeshPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_MeshPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_MeshPipelineLayout, 1, 1,
                                         &m_MeshDescSet, 0, nullptr);

        vkCmdDrawMeshTasksNv(&*commandBuffer, m_Meshlets.size(), 0);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_AxisPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_AxisPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);
        
        commandBuffer.bindVertexBuffers(0,m_AxisBuffer.GetVkBuffer(),{0});

        commandBuffer.bindIndexBuffer(m_AxisIndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(6, 1, 0, 0, 0);

        commandBuffer.endRenderPass();
    }

    commandBuffer.end();

    TRY_CATCH_END()
}

bool MeshApplication::OnMousePress(MouseButtonEvent& event)
{
    // Later for ImGui
    // if (m_AppWindow->GetProps().m_ImGuiIO->WantCaptureMouse)
    // {
    //     ImGui_ImplGlfw_MouseButtonCallback(m_AppWindow->GetGLFWWindow(), event.GetKeyCode(), GLFW_PRESS, 0);
    //     return true;
    // }

    switch (event.GetKeyCode())
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        m_MouseState.m_IsLMBPressed = true;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        m_MouseState.m_IsMMBPressed = true;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        m_MouseState.m_IsRMBPressed = true;
        break;
    }

    LOG(Application, Verbose, "Mouse Pressed")
    return true;
}

bool MeshApplication::OnMouseMoved(MouseMovedEvent& event)
{
    // ImGui_ImplGlfw_CursorPosCallback(m_AppWindow->GetGLFWWindow(), event.GetPos().x, event.GetPos().y);

    // LOGF(Application, Info, "Mouse last position X: %d, Y: %d", m_MouseState.m_LastPosition.x,
    //      m_MouseState.m_LastPosition.y)

    if (m_MouseState.m_IsRMBPressed)
    {
        const glm::ivec2 diff = m_MouseState.m_LastPosition - event.GetPos();
        m_Camera.Yaw(-diff.x);
        m_Camera.Pitch(-diff.y);
    }

    m_MouseState.UpdatePosition(event.GetPos());

    return true;
}

bool MeshApplication::OnMouseRelease(MouseButtonEvent& event)
{

    // Later for ImGui
    // if (m_AppWindow->GetProps().m_ImGuiIO->WantCaptureMouse)
    // {
    //     ImGui_ImplGlfw_MouseButtonCallback(m_AppWindow->GetGLFWWindow(), event.GetKeyCode(), GLFW_RELEASE, 0);
    //     return true;
    // }

    switch (event.GetKeyCode())
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        m_MouseState.m_IsLMBPressed = false;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        m_MouseState.m_IsMMBPressed = false;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        m_MouseState.m_IsRMBPressed = false;
        break;
    }

    return true;
    LOG(Application, Info, "Mouse released")
    return true;
}

bool MeshApplication::OnKeyPressed(KeyPressedEvent& event)
{
    switch (event.GetKeyCode())
    {
    case GLFW_KEY_W:
        m_Camera.SetIsMovingForward(true);
        return true;
    case GLFW_KEY_A:
        m_Camera.SetIsMovingLeft(true);
        return true;
    case GLFW_KEY_S:
        m_Camera.SetIsMovingBackward(true);
        return true;
    case GLFW_KEY_D:
        m_Camera.SetIsMovingRight(true);
        return true;
    case GLFW_KEY_E:
        m_Camera.SetIsMovingUp(true);
        return true;
    case GLFW_KEY_Q:
        m_Camera.SetIsMovingDown(true);
        return true;
    default:
        return false;
    }
}

bool MeshApplication::OnKeyReleased(KeyReleasedEvent& event)
{
    switch (event.GetKeyCode())
    {
    case GLFW_KEY_W:
        m_Camera.SetIsMovingForward(false);
        return true;
    case GLFW_KEY_A:
        m_Camera.SetIsMovingLeft(false);
        return true;
    case GLFW_KEY_S:
        m_Camera.SetIsMovingBackward(false);
        return true;
    case GLFW_KEY_D:
        m_Camera.SetIsMovingRight(false);
        return true;
    case GLFW_KEY_E:
        m_Camera.SetIsMovingUp(false);
        return true;
    case GLFW_KEY_Q:
        m_Camera.SetIsMovingDown(false);
        return true;
    default:
        return false;
    }
    LOG(Application, Info, "Key released")
    return true;
}
