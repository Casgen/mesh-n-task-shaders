#include "TessApplication.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stddef.h>
#include <stdexcept>

#include "GLFW/glfw3.h"
#include "Log/Log.h"
#include "Model/Camera.h"
#include "Model/MatrixBuffer.h"
#include "Model/Shaders/ShaderData.h"
#include "Model/Shaders/ShaderLoader.h"
#include "../../Common/Query.h"
#include "Vk/Buffers/Buffer.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "Vk/Devices/DeviceManager.h"
#include "Vk/GraphicsPipeline/GraphicsPipelineBuilder.h"
#include "Vk/Swapchain.h"
#include "Vk/SwapchainRenderPass.h"
#include "Vk/Utils.h"
#include "Vk/Vertex/VertexAttributeBuilder.h"
#include "backends/imgui_impl_glfw.h"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "glm/gtc/type_ptr.hpp"

void TessApplication::Run(const uint32_t winWidth, const uint32_t winHeight)
{
    VkCore::DeviceManager::AddDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

    Logger::SetSeverityFilter(ESeverity::Verbose);

    std::cout << sizeof(Frustum) << std::endl;

    m_Window = new VkCore::Window("Mesh Application", winWidth, winHeight);
    m_Window->SetEventCallback(std::bind(&TessApplication::OnEvent, this, std::placeholders::_1));

    m_Renderer = VulkanRenderer("Mesh Application", m_Window,
                                {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,

#ifndef VK_MESH_EXT
                                 VK_NV_MESH_SHADER_EXTENSION_NAME,
#else
                                 VK_EXT_MESH_SHADER_EXTENSION_NAME,
#endif
                                 VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME},
                                {});
    m_Renderer.InitImGui(m_Window, winWidth, winHeight);

#ifndef VK_MESH_EXT
    vkCmdDrawMeshTasksNv =
        (PFN_vkCmdDrawMeshTasksNV)vkGetDeviceProcAddr(*VkCore::DeviceManager::GetDevice(), "vkCmdDrawMeshTasksNV");
#else
    vkCmdDrawMeshTasksEXT =
        (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(*VkCore::DeviceManager::GetDevice(), "vkCmdDrawMeshTasksEXT");
#endif

    vkCmdSetPolygonModeEXT =
        (PFN_vkCmdSetPolygonModeEXT)vkGetDeviceProcAddr(*VkCore::DeviceManager::GetDevice(), "vkCmdSetPolygonModeEXT");

    m_Camera =
        Camera({0.f, 1.f, -1.f}, {0.f, 0.f, 0.f}, (float)m_Window->GetWidth() / m_Window->GetHeight(), 45.f, 50.f);

    // Create Uniform Buffers
    for (int i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
        VkCore::Buffer matBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eUniformBuffer);
        matBuffer.InitializeOnCpu(sizeof(MatrixBuffer));

        m_MatBuffers.emplace_back(std::move(matBuffer));
    }

    // Camera Matrix Descriptor Sets
    m_DescriptorBuilder = VkCore::DescriptorBuilder(VkCore::DeviceManager::GetDevice());

    for (uint32_t i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
        vk::DescriptorSet tempSet;

        m_DescriptorBuilder.BindBuffer(0, m_MatBuffers[i], vk::DescriptorType::eUniformBuffer,
                                       vk::ShaderStageFlagBits::eMeshNV | vk::ShaderStageFlagBits::eVertex |
                                           vk::ShaderStageFlagBits::eTaskEXT);
        m_DescriptorBuilder.Build(tempSet, m_MatrixDescSetLayout);
        m_DescriptorBuilder.Clear();

        m_MatrixDescriptorSets.emplace_back(tempSet);
    }

    InitializeNoisePipeline();
    InitializeTessPipeline();
    InitializeAxisPipeline();

    Loop();
    Shutdown();
}

void TessApplication::InitializeTessPipeline()
{

    const std::vector<VkCore::ShaderData> shaders =
        VkCore::ShaderLoader::LoadMeshShaders("Tesselation/Res/Shaders/tesselation", false);

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice(), true);

    vk::PipelineColorBlendAttachmentState blendAttachment;

    blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                     vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blendAttachment.blendEnable = true;
    blendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    m_WaterPipeline = pipelineBuilder.BindShaderModules(shaders)
                          .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                          .EnableDepthTest()
                          .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                          .FrontFaceDirection(vk::FrontFace::eClockwise)
                          .SetCullMode(vk::CullModeFlagBits::eNone)
                          .AddBlendAttachment(blendAttachment)
                          .AddDescriptorLayout(m_MatrixDescSetLayout)
                          .AddDescriptorLayout(m_MeshNoiseSetLayout)
                          .AddPushConstantRange<FragmentPC>(vk::ShaderStageFlagBits::eFragment)
                          .AddPushConstantRange<TessPC>(vk::ShaderStageFlagBits::eTaskEXT)
                          .SetPrimitiveAssembly(vk::PrimitiveTopology::eTriangleList)
                          .AddDynamicState(vk::DynamicState::eScissor)
                          .AddDynamicState(vk::DynamicState::eViewport)
                          .AddDynamicState(vk::DynamicState::ePolygonModeEXT)
                          .Build(m_WaterPipelineLayout);
}

void TessApplication::InitializeNoisePipeline()
{
    const VkCore::ShaderData shader =
        VkCore::ShaderLoader::LoadComputeShader("Tesselation/Res/Shaders/noise/noise.comp", false);

    VkCore::ComputePipelineBuilder pipelineBuilder{};

    m_NoiseHeight.InitializeOnTheGpu(m_NoiseResolution, m_NoiseResolution, vk::Format::eR32Sfloat);
    m_NoiseNormals.InitializeOnTheGpu(m_NoiseResolution, m_NoiseResolution, vk::Format::eR32G32B32A32Sfloat);

    vk::DescriptorImageInfo descHeight = m_NoiseHeight.CreateDescriptorImageInfo(vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo descNormals = m_NoiseNormals.CreateDescriptorImageInfo(vk::ImageLayout::eGeneral);

    // Have to create 2 types of descriptor sets. One for the compute storage image and other for sampling (reading as a
    // texture).
    m_DescriptorBuilder.BindImage(0, descHeight, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
        .BindImage(1, descNormals, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
        .Build(m_ComputeNoiseSet, m_ComputeNoiseSetLayout);

    m_DescriptorBuilder
        .BindImage(0, descHeight, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eFragment)
        .BindImage(1, descNormals, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eFragment)
        .Build(m_MeshNoiseSet, m_MeshNoiseSetLayout);

    m_ComputePipeline = pipelineBuilder.BindShaderModule(shader)
                            .AddDescriptorLayout(m_ComputeNoiseSetLayout)
                            .AddPushConstantRange<NoisePC>(vk::ShaderStageFlagBits::eCompute)
                            .Build(m_ComputePipelineLayout);
}

void TessApplication::InitializeAxisPipeline()
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
        VkCore::ShaderLoader::LoadClassicShaders("Tesselation/Res/Shaders/axis");
    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    m_AxisPipeline = pipelineBuilder.BindShaderModules(shaderData)
                         .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                         .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                         .FrontFaceDirection(vk::FrontFace::eCounterClockwise)
                         .SetCullMode(vk::CullModeFlagBits::eBack)
                         .BindVertexAttributes(attributeBuilder)
                         .AddDisabledBlendAttachment()
                         .AddDescriptorLayout(m_MatrixDescSetLayout)
                         .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                         .AddDynamicState(vk::DynamicState::eScissor)
                         .AddDynamicState(vk::DynamicState::eViewport)
                         .Build(m_AxisPipelineLayout);
}

void TessApplication::DrawFrame()
{

    uint32_t imageIndex = m_Renderer.AcquireNextImage();

    if (imageIndex == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }

    noise_pc.time = glfwGetTime();

    m_Camera.Update();

    MatrixBuffer ubo{};
    ubo.m_Proj = m_Camera.GetProjMatrix();
    ubo.m_View = m_Camera.GetViewMatrix();

    ubo.frustum = m_Camera.CalculateFrustum();

    fragment_pc.cam_pos = m_Camera.GetPosition();
    fragment_pc.cam_view_dir = m_Camera.GetViewDirection();

    m_MatBuffers[imageIndex].UpdateData(&ubo);

    m_Renderer.BeginCmdBuffer();

    DurationQuery durationQuery;

    vk::CommandBuffer commandBuffer = m_Renderer.GetCurrentCmdBuffer();
    durationQuery.Reset(commandBuffer);

    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_ComputePipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_ComputePipelineLayout, 0, 1,
                                         &m_ComputeNoiseSet, 0, nullptr);
        commandBuffer.pushConstants(m_ComputePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(NoisePC),
                                    &noise_pc);

        commandBuffer.dispatch(m_NoiseResolution / 8, m_NoiseResolution / 8, 1);

        m_NoiseNormals.TransitionToShaderRead(commandBuffer, vk::PipelineStageFlagBits::eComputeShader,
                                              vk::PipelineStageFlagBits::eTaskShaderEXT);
        m_NoiseHeight.TransitionToShaderRead(commandBuffer, vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eTaskShaderEXT);
    }

    vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
    commandBuffer.setScissor(0, 1, &scissor);

    vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
    commandBuffer.setViewport(0, 1, &viewport);

    m_Renderer.BeginRenderPass({0.3f, 0.f, 0.2f, 1.f}, m_Window->GetWidth(), m_Window->GetHeight());

    {

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_WaterPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_WaterPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_WaterPipelineLayout, 1, 1, &m_MeshNoiseSet,
                                         0, nullptr);

        commandBuffer.pushConstants(m_WaterPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPC),
                                    &fragment_pc);

        commandBuffer.pushConstants(m_WaterPipelineLayout, vk::ShaderStageFlagBits::eTaskEXT, sizeof(FragmentPC), sizeof(TessPC),
                                    &tess_pc);

        durationQuery.StartTimestamp(commandBuffer, vk::PipelineStageFlagBits::eTaskShaderEXT);

        vkCmdSetPolygonModeEXT(&*commandBuffer, m_PolygonMode);

#ifndef VK_MESH_EXT
        vkCmdDrawMeshTasksNv(&*commandBuffer, mesh.GetMeshletCount(), 0);
#else
        vkCmdDrawMeshTasksEXT(&*commandBuffer, m_PatchCounts.x, m_PatchCounts.y, m_GridSize * m_GridSize);
#endif

        durationQuery.EndTimestamp(commandBuffer, vk::PipelineStageFlagBits::eFragmentShader);
    }

    {
        m_Renderer.ImGuiNewFrame(m_Window->GetWidth(), m_Window->GetHeight());

        static bool open = true;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Tesselation", &open))
        {

            ImGui::Text("Task/Mesh Shader execution in ms: %.4f", m_Duration / 1000000.f);
            ImGui::Text("Patch Count");
            ImGui::DragInt2("##Patch Count", glm::value_ptr(m_PatchCounts), 1.f, 1, 100, "%d",
                            ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Grid Scale");
            ImGui::DragFloat("##Grid Scale", &tess_pc.scale, 0.01f, 0.f, 30.f, "x%.3f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Grid Size");
            ImGui::DragInt("##Grid Size", &m_GridSize, 0.1f, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Speed");
            ImGui::DragFloat("##Speed", &noise_pc.speed, 0.01f, 0.f, 8.f, "x%.3f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Octaves");
            ImGui::DragInt("##Octaves", &noise_pc.octaves, 0.1f, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Height");
            ImGui::DragFloat("##Height", &noise_pc.height, 0.01f, 0.f, 4.f, "x%.3f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Enable Line Mode");
            ImGui::SameLine();
            if (ImGui::Checkbox("##Enable Line Mode", &m_EnableLineMode))
            {
                if (m_EnableLineMode)
                    m_PolygonMode = VK_POLYGON_MODE_LINE;
                else
                    m_PolygonMode = VK_POLYGON_MODE_FILL;
            }

            ImGui::End();
        }

        m_Renderer.ImGuiRender(commandBuffer);
    }

    m_Renderer.EndRenderPass();

    m_NoiseNormals.TransitionToGeneral(commandBuffer, vk::PipelineStageFlagBits::eFragmentShader,
                                       vk::PipelineStageFlagBits::eFragmentShader);
    m_NoiseHeight.TransitionToGeneral(commandBuffer, vk::PipelineStageFlagBits::eFragmentShader,
                                      vk::PipelineStageFlagBits::eFragmentShader);

    uint32_t endDrawResult = m_Renderer.EndCmdBuffer();

    m_Duration = durationQuery.GetResults();

    if (endDrawResult == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }
}

void TessApplication::Loop()
{
    if (m_Window == nullptr)
    {
        throw std::runtime_error("Failed to run the App! The window is NULL!");
    }

    while (!m_Window->ShouldClose())
    {
        glfwPollEvents();
        DrawFrame();
    }
}

void TessApplication::Shutdown()
{

    VkCore::Device& device = VkCore::DeviceManager::GetDevice();

    m_Renderer.m_Swapchain.Destroy();

    device.DestroyPipeline(m_AxisPipeline);
    device.DestroyPipelineLayout(m_AxisPipelineLayout);

    device.DestroyPipeline(m_WaterPipeline);
    device.DestroyPipelineLayout(m_WaterPipelineLayout);

    m_NoiseHeight.Destroy();
    m_NoiseNormals.Destroy();

    m_AxisBuffer.Destroy();
    m_AxisIndexBuffer.Destroy();

    for (VkCore::Buffer& buffer : m_MatBuffers)
    {
        buffer.Destroy();
    }

    device.DestroyDescriptorSetLayout(m_MatrixDescSetLayout);
    device.DestroyDescriptorSetLayout(m_MeshNoiseSetLayout);
    device.DestroyDescriptorSetLayout(m_ComputeNoiseSetLayout);

    m_DescriptorBuilder.Clear();
    m_DescriptorBuilder.Cleanup();

    delete m_Window;
}

void TessApplication::OnEvent(Event& event)
{
    EventDispatcher dispatcher = EventDispatcher(event);

    switch (event.GetEventType())
    {
    case EventType::MouseBtnPressed:
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(TessApplication::OnMousePress));
        break;
    case EventType::MouseMoved:
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(TessApplication::OnMouseMoved));
        break;
    case EventType::MouseBtnReleased:
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(TessApplication::OnMouseRelease));
        break;
    case EventType::MouseScrolled:
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(TessApplication::OnMouseScrolled));
    case EventType::KeyPressed:
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(TessApplication::OnKeyPressed));
        break;
    case EventType::KeyReleased:
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(TessApplication::OnKeyReleased));
        break;
    case EventType::WindowResized:
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(TessApplication::OnWindowResize));
        break;
    case EventType::KeyRepeated:
    case EventType::MouseBtnRepeated:
    case EventType::WindowClosed:
        break;
    }
}

bool TessApplication::OnMousePress(MouseButtonEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        ImGui_ImplGlfw_MouseButtonCallback(m_Window->GetGLFWWindow(), event.GetKeyCode(), GLFW_PRESS, 0);

        LOG(Vulkan, Verbose, "Capturing mouse")
        return true;
    }

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
        m_Window->DisableCursor();
        break;
    }

    LOG(Application, Verbose, "Mouse Pressed")
    return true;
}

bool TessApplication::OnMouseMoved(MouseMovedEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse && !m_MouseState.m_IsRMBPressed)
    {
        ImGui_ImplGlfw_CursorPosCallback(m_Window->GetGLFWWindow(), event.GetPos().x, event.GetPos().y);
        return false;
    }

    // LOGF(Application, Info, "Mouse last position X: %d, Y: %d",
    // m_MouseState.m_LastPosition.x,
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

bool TessApplication::OnMouseRelease(MouseButtonEvent& event)
{

    if (ImGui::GetIO().WantCaptureMouse && !m_MouseState.m_IsRMBPressed)
    {
        ImGui_ImplGlfw_MouseButtonCallback(m_Window->GetGLFWWindow(), event.GetKeyCode(), GLFW_RELEASE, 0);
        return true;
    }

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
        m_Window->EnabledCursor();
        break;
    }

    return true;
    LOG(Application, Info, "Mouse released")
    return true;
}

bool TessApplication::OnMouseScrolled(MouseScrolledEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        ImGui_ImplGlfw_ScrollCallback(m_Window->GetGLFWWindow(), event.GetXOffset(), event.GetYOffset());
        return true;
    }

    m_Camera.AddMovementSpeed((float)event.GetYOffset() * 0.01f);

    return true;
}

bool TessApplication::OnKeyPressed(KeyPressedEvent& event)
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
    case GLFW_KEY_M:
        fragment_pc.is_meshlet_view_on = !fragment_pc.is_meshlet_view_on;
        LOGF(Application, Verbose, "Meshlet view is %d", fragment_pc.is_meshlet_view_on)
    default:
        return false;
    }
}

void TessApplication::RecreateSwapchain()
{

    VkCore::Device& device = VkCore::DeviceManager::GetDevice();

    device.WaitIdle();
    m_Window->WaitEvents();

    m_Renderer.DestroySwapchain();
    m_Renderer.DestroyFrameBuffers();

    m_Window->RefreshResolution();
    m_Renderer.CreateSwapchain(m_Window->GetWidth(), m_Window->GetHeight());
    m_Renderer.CreateFramebuffers(m_Window->GetWidth(), m_Window->GetHeight());

    m_Camera.RecreateProjection(m_Window->GetWidth(), m_Window->GetHeight());

    m_FramebufferResized = false;
}

bool TessApplication::OnWindowResize(WindowResizedEvent& event)
{
    LOG(Window, Verbose, "Window Resized");
    m_FramebufferResized = true;

    return true;
}

bool TessApplication::OnKeyReleased(KeyReleasedEvent& event)
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
