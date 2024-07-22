#include "LODApplication.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stddef.h>
#include <stdexcept>

#include "GLFW/glfw3.h"
#include "Log/Log.h"
#include "Mesh/LODMesh.h"
#include "Model/Camera.h"
#include "Model/MatrixBuffer.h"
#include "Model/Shaders/ShaderData.h"
#include "Model/Shaders/ShaderLoader.h"
#include "Vk/Buffers/Buffer.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "Vk/Devices/DeviceManager.h"
#include "Vk/GraphicsPipeline/GraphicsPipelineBuilder.h"
#include "Vk/Swapchain.h"
#include "Vk/SwapchainRenderPass.h"
#include "Vk/Utils.h"
#include "Vk/Vertex/VertexAttributeBuilder.h"
#include "backends/imgui_impl_glfw.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "glm/gtc/type_ptr.hpp"

void LODApplication::Run(const uint32_t winWidth, const uint32_t winHeight)
{
    Logger::SetSeverityFilter(ESeverity::Verbose);

    std::cout << sizeof(Frustum) << std::endl;

    m_Window = new VkCore::Window("Mesh Application", winWidth, winHeight);
    m_Window->SetEventCallback(std::bind(&LODApplication::OnEvent, this, std::placeholders::_1));

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

    m_Camera =
        Camera({0.f, 0.f, -1.f}, {0.f, 0.f, 0.f}, (float)m_Window->GetWidth() / m_Window->GetHeight(), 45.f, 50.f);
    m_CurrentCamera = &m_Camera;
    m_FrustumCamera =
        Camera({0.f, 0.f, -1.f}, {0.f, 0.f, 0.f}, (float)m_Window->GetWidth() / m_Window->GetHeight(), 45.f, 25.f);

    m_ZenithAngle = m_FrustumCamera.GetZenith();
    m_AzimuthAngle = m_FrustumCamera.GetAzimuth();
    m_Position = m_FrustumCamera.GetPosition();

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

    InitializeInstancing();

    InitializeModelPipeline();
    InitializeAxisPipeline();
    InitializeBoundsPipeline();
    InitializeFrustumPipeline();

    Loop();
    Shutdown();
}

void LODApplication::InitializeModelPipeline()
{


    m_Model = new LODModel("MeshLOD/Res/Artwork/OBJs/kitten_lod0.obj");

    const std::vector<VkCore::ShaderData> shaders =
        VkCore::ShaderLoader::LoadMeshShaders("MeshLOD/Res/Shaders/lod", false);

    // Pipeline
    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice(), true);

    m_ModelPipeline = pipelineBuilder.BindShaderModules(shaders)
                          .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                          .EnableDepthTest()
                          .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                          .FrontFaceDirection(vk::FrontFace::eClockwise)
                          .SetCullMode(vk::CullModeFlagBits::eBack)
                          .AddDisabledBlendAttachment()
                          .AddDescriptorLayout(m_MatrixDescSetLayout)
                          .AddDescriptorLayout(m_Model->GetMeshSetLayout(0))
                          .AddDescriptorLayout(m_InstancesDescSetLayout)
                          .AddPushConstantRange<FragmentPC>(vk::ShaderStageFlagBits::eFragment)
                          .AddPushConstantRange<LodPC>(
                              vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eTaskEXT, sizeof(FragmentPC))
                          .SetPrimitiveAssembly(vk::PrimitiveTopology::eTriangleList)
                          .AddDynamicState(vk::DynamicState::eScissor)
                          .AddDynamicState(vk::DynamicState::eViewport)
                          .Build(m_ModelPipelineLayout);
}

void LODApplication::InitializeAxisPipeline()
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
        VkCore::ShaderLoader::LoadClassicShaders("MeshLOD/Res/Shaders/axis");
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

void LODApplication::InitializeBoundsPipeline()
{

    m_Sphere = SphereModel(Vec3f(0.f, 0.f, 0.f));

    VkCore::VertexAttributeBuilder attributeBuilder;

    attributeBuilder.PushAttribute<float>(3).PushAttribute<float>(3).PushAttribute<float>(3);

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("MeshLOD/Res/Shaders/bounds");

    m_BoundsPipeline = pipelineBuilder.BindShaderModules(shaderData)
                           .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                           .EnableDepthTest()
                           .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                           .FrontFaceDirection(vk::FrontFace::eClockwise)
                           .SetCullMode(vk::CullModeFlagBits::eNone)
                           .BindVertexAttributes(attributeBuilder)
                           .AddDisabledBlendAttachment()
                           .AddDescriptorLayout(m_MatrixDescSetLayout)
                           .AddDescriptorLayout(m_Model->GetMeshSetLayout(0))
                           .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                           .AddDynamicState(vk::DynamicState::eScissor)
                           .AddDynamicState(vk::DynamicState::eViewport)
                           .Build(m_BoundsPipelineLayout);
}

void LODApplication::InitializeFrustumPipeline()
{
    std::tie(m_FrustumBuffer, m_FrustumIndexBuffer) = m_FrustumCamera.ConstructFrustumModel();

    VkCore::VertexAttributeBuilder attributeBuilder{};

    attributeBuilder.PushAttribute<float>(3).PushAttribute<float>(3).PushAttribute<float>(3);
    attributeBuilder.SetBinding(0);

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("MeshLOD/Res/Shaders/frustum");

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    m_FrustumPipeline = pipelineBuilder.BindShaderModules(shaderData)
                            .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                            .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                            .FrontFaceDirection(vk::FrontFace::eCounterClockwise)
                            .SetCullMode(vk::CullModeFlagBits::eNone)
                            .BindVertexAttributes(attributeBuilder)
                            .AddDisabledBlendAttachment()
                            .SetLineWidth(2.f)
                            .AddDescriptorLayout(m_MatrixDescSetLayout)
                            .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                            .AddDynamicState(vk::DynamicState::eScissor)
                            .AddDynamicState(vk::DynamicState::eViewport)
                            .Build(m_FrustumPipelineLayout);
}

void LODApplication::InitializeInstancing()
{

    const glm::vec3 span = glm::vec3(1.f);

    std::vector<glm::mat4> instances;
    instances.resize(m_InstanceCountMax);

    for (uint32_t z = 0; z < m_InstanceSize.z; z++)
    {
        for (uint32_t y = 0; y < m_InstanceSize.y; y++)
        {
            for (uint32_t x = 0; x < m_InstanceSize.x; x++)
            {
                glm::vec3 instancePos = {x * span.x, y * span.y, z * span.z};

                const uint32_t index = x + m_InstanceSize.y * y + m_InstanceSize.x * m_InstanceSize.y * z;

                instances[index] = (glm::translate(glm::identity<glm::mat4>(), instancePos));
            }
        }
    }

    m_InstancesBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eStorageBuffer);
    m_InstancesBuffer.InitializeOnGpu(instances.data(), instances.size() * sizeof(glm::mat4));

    m_DescriptorBuilder
        .BindBuffer(0, m_InstancesBuffer, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eTaskEXT)
        .Build(m_InstancesDescSet, m_InstancesDescSetLayout);
}

void LODApplication::DrawFrame()
{

    uint32_t imageIndex = m_Renderer.AcquireNextImage();

    if (imageIndex == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }

    const double time = glfwGetTime();

    m_CurrentCamera->Update();

    if (m_ZenithSweepEnabled)
    {
        m_FrustumCamera.Pitch(sinf(time));
    }

    if (m_AzimuthSweepEnabled)
    {
        m_FrustumCamera.Yaw(cosf(time));
    }

    MatrixBuffer ubo{};
    ubo.m_Proj = m_CurrentCamera->GetProjMatrix();
    ubo.m_View = m_CurrentCamera->GetViewMatrix();

    ubo.frustum = m_FrustumCamera.CalculateFrustum();

    fragment_pc.cam_pos = m_CurrentCamera->GetPosition();
    fragment_pc.cam_view_dir = m_CurrentCamera->GetViewDirection();

    m_Renderer.BeginDraw({0.3f, 0.f, 0.2f, 1.f}, m_Window->GetWidth(), m_Window->GetHeight());

    m_MatBuffers[imageIndex].UpdateData(&ubo);

    vk::CommandBuffer commandBuffer = m_Renderer.GetCurrentCmdBuffer();

    vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
    commandBuffer.setScissor(0, 1, &scissor);

    vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
    commandBuffer.setViewport(0, 1, &viewport);

    {

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_ModelPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 2, 1,
                                         &m_InstancesDescSet, 0, nullptr);

        commandBuffer.pushConstants(m_ModelPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPC),
                                    &fragment_pc);

        commandBuffer.pushConstants(m_ModelPipelineLayout,
                                    vk::ShaderStageFlagBits::eMeshNV | vk::ShaderStageFlagBits ::eTaskEXT,
                                    sizeof(FragmentPC), sizeof(LodPC), &lod_pc);

        for (uint32_t i = 0; i < m_Model->GetMeshCount(); i++)
        {
			LODMesh& mesh = m_Model->GetMesh(i);

            const vk::DescriptorSetLayout layout = mesh.GetDescriptorSetLayout();
            const vk::DescriptorSet set = mesh.GetDescriptorSet();

            lod_pc.meshlet_count = mesh.GetMeshInfo().lodMeshletCount[0];

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 1, 1, &set, 0,
                                             nullptr);

#ifndef VK_MESH_EXT
            vkCmdDrawMeshTasksNv(&*commandBuffer, mesh.GetMeshletCount(), 0);
#else
            vkCmdDrawMeshTasksEXT(&*commandBuffer, (mesh.GetMeshInfo().lodMeshletCount[0] / 32) + 1,
                                  m_InstanceCount, 1);
#endif
        }
    }

    // {
    //     commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_BoundsPipeline);
    //     commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_BoundsPipelineLayout, 0, 1,
    //                                      &m_MatrixDescriptorSets[imageIndex], 0, nullptr);
    //
    //     commandBuffer.bindVertexBuffers(0, m_Sphere.m_Vertexbuffer.GetVkBuffer(), {0});
    //     commandBuffer.bindIndexBuffer(m_Sphere.m_IndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
    //
    //     for (const Mesh& mesh : m_Model->GetMeshes())
    //     {
    //         const vk::DescriptorSet& set = mesh.GetDescriptorSet();
    //         commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_BoundsPipelineLayout, 1, 1, &set, 0,
    //                                          nullptr);
    //         commandBuffer.drawIndexed(m_Sphere.indices.size(), mesh.GetMeshletCount(), 0, 0, 0);
    //     }
    // }

    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_AxisPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_AxisPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindVertexBuffers(0, m_AxisBuffer.GetVkBuffer(), {0});

        commandBuffer.bindIndexBuffer(m_AxisIndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(6, 1, 0, 0, 0);
    }

    {

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_FrustumPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_FrustumPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindVertexBuffers(0, m_FrustumBuffer.GetVkBuffer(), {0});

        commandBuffer.bindIndexBuffer(m_FrustumIndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(32, 1, 0, 0, 0);
    }

    {
        m_Renderer.ImGuiNewFrame(m_Window->GetWidth(), m_Window->GetHeight());

        static bool open = true;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Instancing", &open))
        {
			ImGui::Text("Instance Count");
			ImGui::SliderInt("##Instance Count", &m_InstanceCount, 0, (int)m_InstanceCountMax, "%d", ImGuiSliderFlags_AlwaysClamp);

			ImGui::Text("LOD Exponent");
			ImGui::SliderFloat("##LOD Exponent", &lod_pc.lod_pow, 0, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	
            ImGui::Text("Posses Preview Camera");
            ImGui::SameLine();
            if (ImGui::Checkbox("##Posses Preview Camera", &m_PossesCamera))
            {
                m_AzimuthSweepEnabled = false;
                m_ZenithSweepEnabled = false;

                if (m_PossesCamera)
                {
                    m_CurrentCamera = &m_FrustumCamera;
                }
                else
                {
                    m_AzimuthAngle = m_FrustumCamera.GetAzimuth();
                    m_ZenithAngle = m_FrustumCamera.GetZenith();
                    m_Position = m_FrustumCamera.GetPosition();
                    m_CurrentCamera = &m_Camera;
                }
            };

            if (!m_PossesCamera)
            {
                ImGui::Text("Zenith");
                if (ImGui::SliderAngle("##Zenith", &m_ZenithAngle, 90, -90))
                {
                    m_FrustumCamera.SetZenith(m_ZenithAngle);
                };

                ImGui::Text("Sweep Zenith");
                ImGui::SameLine();
                ImGui::Checkbox("##Sweep Zenith", &m_ZenithSweepEnabled);

                ImGui::Text("Azimuth");
                if (ImGui::SliderAngle("##Azimuth", &m_AzimuthAngle, -180, 180))
                {
                    m_FrustumCamera.SetAzimuth(m_AzimuthAngle);
                }

                ImGui::Text("Sweep Azimuth");
                ImGui::SameLine();
                ImGui::Checkbox("##Sweep Azimuth", &m_AzimuthSweepEnabled);

                ImGui::Text("Position");
                if (ImGui::DragFloat3("##Position", glm::value_ptr(m_Position), 0.01f, -20.f, 20.f))
                {
                    m_FrustumCamera.SetPosition(m_Position);
                }
            }

            ImGui::End();
        }

        m_Renderer.ImGuiRender(commandBuffer);
    }

    uint32_t endDrawResult = m_Renderer.EndDraw();

    if (endDrawResult == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }
}

void LODApplication::Loop()
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

void LODApplication::Shutdown()
{

    VkCore::Device& device = VkCore::DeviceManager::GetDevice();

    m_Renderer.m_Swapchain.Destroy();

    device.DestroyPipeline(m_AxisPipeline);
    device.DestroyPipelineLayout(m_AxisPipelineLayout);

    device.DestroyPipeline(m_ModelPipeline);
    device.DestroyPipelineLayout(m_ModelPipelineLayout);

    device.DestroyPipeline(m_BoundsPipeline);
    device.DestroyPipelineLayout(m_BoundsPipelineLayout);

    device.DestroyPipeline(m_FrustumPipeline);
    device.DestroyPipelineLayout(m_FrustumPipelineLayout);

    m_Model->Destroy();

    m_AxisBuffer.Destroy();
    m_AxisIndexBuffer.Destroy();

    m_FrustumBuffer.Destroy();
    m_FrustumIndexBuffer.Destroy();

    for (VkCore::Buffer& buffer : m_MatBuffers)
    {
        buffer.Destroy();
    }

    device.DestroyDescriptorSetLayout(m_MatrixDescSetLayout);

    m_DescriptorBuilder.Clear();
    m_DescriptorBuilder.Cleanup();

    delete m_Window;
}

void LODApplication::OnEvent(Event& event)
{
    EventDispatcher dispatcher = EventDispatcher(event);

    switch (event.GetEventType())
    {
    case EventType::MouseBtnPressed:
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(LODApplication::OnMousePress));
        break;
    case EventType::MouseMoved:
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(LODApplication::OnMouseMoved));
        break;
    case EventType::MouseBtnReleased:
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(LODApplication::OnMouseRelease));
        break;
    case EventType::MouseScrolled:
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(LODApplication::OnMouseScrolled));
    case EventType::KeyPressed:
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(LODApplication::OnKeyPressed));
        break;
    case EventType::KeyReleased:
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(LODApplication::OnKeyReleased));
        break;
    case EventType::WindowResized:
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(LODApplication::OnWindowResize));
        break;
    case EventType::KeyRepeated:
    case EventType::MouseBtnRepeated:
    case EventType::WindowClosed:
        break;
    }
}

bool LODApplication::OnMousePress(MouseButtonEvent& event)
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

bool LODApplication::OnMouseMoved(MouseMovedEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse)
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
        m_CurrentCamera->Yaw(-diff.x);
        m_CurrentCamera->Pitch(-diff.y);
    }

    if (m_MouseState.m_IsLMBPressed)
    {
        const glm::ivec2 diff = m_MouseState.m_LastPosition - event.GetPos();

        angles += static_cast<glm::vec2>(diff) * 0.01f;

        lod_pc.rotation_mat = glm::rotate(glm::identity<glm::mat4>(), angles.x, {0.f, 1.f, 0.f});
        lod_pc.rotation_mat = glm::rotate(lod_pc.rotation_mat, angles.y, {-1.f, 0.f, 0.f});
    }

    m_MouseState.UpdatePosition(event.GetPos());

    return true;
}

bool LODApplication::OnMouseRelease(MouseButtonEvent& event)
{

    if (ImGui::GetIO().WantCaptureMouse)
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

bool LODApplication::OnMouseScrolled(MouseScrolledEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        ImGui_ImplGlfw_ScrollCallback(m_Window->GetGLFWWindow(), event.GetXOffset(), event.GetYOffset());
        return true;
    }

    m_CurrentCamera->AddMovementSpeed((float)event.GetYOffset() * 0.01f);

    return true;
}

bool LODApplication::OnKeyPressed(KeyPressedEvent& event)
{
    switch (event.GetKeyCode())
    {
    case GLFW_KEY_W:
        m_CurrentCamera->SetIsMovingForward(true);
        return true;
    case GLFW_KEY_A:
        m_CurrentCamera->SetIsMovingLeft(true);
        return true;
    case GLFW_KEY_S:
        m_CurrentCamera->SetIsMovingBackward(true);
        return true;
    case GLFW_KEY_D:
        m_CurrentCamera->SetIsMovingRight(true);
        return true;
    case GLFW_KEY_E:
        m_CurrentCamera->SetIsMovingUp(true);
        return true;
    case GLFW_KEY_Q:
        m_CurrentCamera->SetIsMovingDown(true);
        return true;
    case GLFW_KEY_M:
        fragment_pc.is_meshlet_view_on = !fragment_pc.is_meshlet_view_on ;
        LOGF(Application, Verbose, "Meshlet view is %d", fragment_pc.is_meshlet_view_on)
    default:
        return false;
    }
}

void LODApplication::RecreateSwapchain()
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

bool LODApplication::OnWindowResize(WindowResizedEvent& event)
{
    LOG(Window, Verbose, "Window Resized");
    m_FramebufferResized = true;

    return true;
}

bool LODApplication::OnKeyReleased(KeyReleasedEvent& event)
{
    switch (event.GetKeyCode())
    {
    case GLFW_KEY_W:
        m_CurrentCamera->SetIsMovingForward(false);
        return true;
    case GLFW_KEY_A:
        m_CurrentCamera->SetIsMovingLeft(false);
        return true;
    case GLFW_KEY_S:
        m_CurrentCamera->SetIsMovingBackward(false);
        return true;
    case GLFW_KEY_D:
        m_CurrentCamera->SetIsMovingRight(false);
        return true;
    case GLFW_KEY_E:
        m_CurrentCamera->SetIsMovingUp(false);
        return true;
    case GLFW_KEY_Q:
        m_CurrentCamera->SetIsMovingDown(false);
        return true;
    default:
        return false;
    }
    LOG(Application, Info, "Key released")
    return true;
}
