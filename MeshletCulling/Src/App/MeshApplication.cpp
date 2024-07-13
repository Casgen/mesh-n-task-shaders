#include "MeshApplication.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stddef.h>
#include <stdexcept>

#include "Constants.h"
#include "GLFW/glfw3.h"
#include "Log/Log.h"
#include "Model/Camera.h"
#include "Model/MatrixBuffer.h"
#include "Model/Shaders/ShaderData.h"
#include "Model/Shaders/ShaderLoader.h"
#include "Model/Structures/Edge.h"
#include "Model/Structures/OcTree.h"
#include "Vk/Buffers/Buffer.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "Vk/Devices/DeviceManager.h"
#include "Vk/GraphicsPipeline/GraphicsPipelineBuilder.h"
#include "Vk/Swapchain.h"
#include "Vk/SwapchainRenderPass.h"
#include "Vk/Utils.h"
#include "Vk/Vertex/VertexAttributeBuilder.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "Model/Structures/AABB.h"
#include "vulkan/vulkan_structs.hpp"

void MeshApplication::Run(const uint32_t winWidth, const uint32_t winHeight)
{
    Logger::SetSeverityFilter(ESeverity::Verbose);

    std::cout << sizeof(Frustum) << std::endl;

    m_Window = new VkCore::Window("Mesh Application", winWidth, winHeight);
    m_Window->SetEventCallback(std::bind(&MeshApplication::OnEvent, this, std::placeholders::_1));

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

    m_Camera = Camera({0.f, 0.f, -2.f}, {0.f, 0.f, 0.f}, (float)m_Window->GetWidth() / m_Window->GetHeight());

    // Create Uniform Buffers
    for (int i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
        VkCore::Buffer matBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eUniformBuffer);
        matBuffer.InitializeOnCpu(sizeof(MatrixBuffer));

        m_MatBuffers.emplace_back(std::move(matBuffer));
    }

    // Camera Matrix Descriptor Sets
    VkCore::DescriptorBuilder descriptorBuilder(VkCore::DeviceManager::GetDevice());

    for (uint32_t i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
        vk::DescriptorSet tempSet;

        descriptorBuilder.BindBuffer(0, m_MatBuffers[i], vk::DescriptorType::eUniformBuffer,
                                     vk::ShaderStageFlagBits::eMeshNV | vk::ShaderStageFlagBits::eVertex |
                                         vk::ShaderStageFlagBits::eTaskEXT);
        descriptorBuilder.Build(tempSet, m_MatrixDescSetLayout);
        descriptorBuilder.Clear();

        m_MatrixDescriptorSets.emplace_back(tempSet);
    }

    InitializeModelPipeline();
    InitializeAxisPipeline();
    InitializeBoundsPipeline();

    Loop();
    Shutdown();
}

void MeshApplication::InitializeModelPipeline()
{

    const std::vector<VkCore::ShaderData> shaders =
        VkCore::ShaderLoader::LoadMeshShaders("MeshletCulling/Res/Shaders/mesh_shading");

    m_Model = new Model("MeshletCulling/Res/Artwork/OBJs/plane.obj");

    // Pipeline
    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice(), true);

    m_ModelPipeline = pipelineBuilder.BindShaderModules(shaders)
                          .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                          .EnableDepthTest()
                          .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                          .FrontFaceDirection(vk::FrontFace::eClockwise)
                          .SetCullMode(vk::CullModeFlagBits::eNone)
                          .AddDisabledBlendAttachment()
                          .AddDescriptorLayout(m_MatrixDescSetLayout)
                          .AddDescriptorLayout(m_Model->GetMeshes()[0].GetDescriptorSetLayout())
                          .AddPushConstantRange<FragmentPC>(vk::ShaderStageFlagBits::eFragment)
                          .AddPushConstantRange<MeshPC>(
                              vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eTaskEXT, sizeof(FragmentPC))
                          .SetPrimitiveAssembly(vk::PrimitiveTopology::eTriangleList)
                          .AddDynamicState(vk::DynamicState::eScissor)
                          .AddDynamicState(vk::DynamicState::eViewport)
                          .Build(m_ModelPipelineLayout);
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
        VkCore::ShaderLoader::LoadClassicShaders("MeshletCulling/Res/Shaders/axis");
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

void MeshApplication::InitializeBoundsPipeline()
{

    m_Sphere = SphereModel(Vec3f(0.f, 0.f, 0.f));

    VkCore::VertexAttributeBuilder attributeBuilder;

    attributeBuilder.PushAttribute<float>(3).PushAttribute<float>(3).PushAttribute<float>(3);

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("MeshletCulling/Res/Shaders/bounds");

    m_BoundsPipeline = pipelineBuilder.BindShaderModules(shaderData)
                           .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                           .EnableDepthTest()
                           .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                           .FrontFaceDirection(vk::FrontFace::eClockwise)
                           .SetCullMode(vk::CullModeFlagBits::eNone)
                           .BindVertexAttributes(attributeBuilder)
                           .AddDisabledBlendAttachment()
                           .AddDescriptorLayout(m_MatrixDescSetLayout)
                           .AddDescriptorLayout(m_Model->GetMeshes()[0].GetDescriptorSetLayout())
                           .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                           .AddDynamicState(vk::DynamicState::eScissor)
                           .AddDynamicState(vk::DynamicState::eViewport)
                           .Build(m_BoundsPipelineLayout);
}

void MeshApplication::DrawFrame()
{

    uint32_t imageIndex = m_Renderer.AcquireNextImage();

    if (imageIndex == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }

    m_Camera.Update();
    Frustum frustum = m_Camera.CalculateFrustumNormals();

    // printf("Left: {%.3f,%.3f,%.3f}\n", frustum.left.x, frustum.left.y, frustum.left.z);
    // printf("Right: {%.3f,%.3f,%.3f}\n", frustum.right.x, frustum.right.y, frustum.right.z);
    // printf("Top: {%.3f,%.3f,%.3f}\n", frustum.top.x, frustum.top.y, frustum.top.z);
    // printf("bottom: {%.3f,%.3f,%.3f}\n", frustum.bottom.x, frustum.bottom.y, frustum.bottom.z);
    // printf("position: {%.3f,%.3f,%.3f}\n\n", frustum.pointSides.x, frustum.pointSides.y, frustum.pointSides.z);

    MatrixBuffer ubo{};
    ubo.m_Proj = m_Camera.GetProjMatrix();
    ubo.m_View = m_Camera.GetViewMatrix();
    ubo.frustum = m_Camera.CalculateFrustumNormals();

    fragment_pc.cam_pos = m_Camera.GetPosition();
    fragment_pc.cam_view_dir = m_Camera.GetViewDirection();

    m_Renderer.BeginDraw({0.3f, 0.f, 0.2f, 1.f}, m_Window->GetWidth(), m_Window->GetHeight());

    m_MatBuffers[imageIndex].UpdateData(&ubo);

    vk::CommandBuffer commandBuffer = m_Renderer.GetCurrentCmdBuffer();

    {

        vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
        commandBuffer.setViewport(0, 1, &viewport);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_ModelPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.pushConstants(m_ModelPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPC),
                                    &fragment_pc);

        commandBuffer.pushConstants(m_ModelPipelineLayout,
                                    vk::ShaderStageFlagBits::eMeshNV | vk::ShaderStageFlagBits ::eTaskEXT,
                                    sizeof(FragmentPC), sizeof(MeshPC), &mesh_pc);

        for (const Mesh& mesh : m_Model->GetMeshes())
        {
            const vk::DescriptorSetLayout layout = mesh.GetDescriptorSetLayout();
            const vk::DescriptorSet set = mesh.GetDescriptorSet();

            mesh_pc.meshlet_count = mesh.GetMeshletCount();

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 1, 1, &set, 0,
                                             nullptr);

#ifndef VK_MESH_EXT
            vkCmdDrawMeshTasksNv(&*commandBuffer, mesh.GetMeshletCount(), 0);
#else
            vkCmdDrawMeshTasksEXT(&*commandBuffer, (mesh.GetMeshletCount() / 32) + 1, 1, 1);
#endif
        }
    }

    {
        vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
        commandBuffer.setViewport(0, 1, &viewport);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_BoundsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_BoundsPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindVertexBuffers(0, m_Sphere.m_Vertexbuffer.GetVkBuffer(), {0});
        commandBuffer.bindIndexBuffer(m_Sphere.m_IndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);

		for (const Mesh& mesh : m_Model->GetMeshes()) {
			commandBuffer.drawIndexed(m_Sphere.indices.size(), mesh.GetMeshletCount(), 0, 0, 0);
		}
    }

    {
        vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
        commandBuffer.setViewport(0, 1, &viewport);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_AxisPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_AxisPipelineLayout, 0, 1,
                                         &m_MatrixDescriptorSets[imageIndex], 0, nullptr);

        commandBuffer.bindVertexBuffers(0, m_AxisBuffer.GetVkBuffer(), {0});

        commandBuffer.bindIndexBuffer(m_AxisIndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(6, 1, 0, 0, 0);
    }

    // {
    //     m_Renderer.ImGuiNewFrame(m_Window->GetWidth(), m_Window->GetHeight());
    //
    //     static bool open = true;
    //
    //     const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    //     ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
    //                             ImGuiCond_FirstUseEver);
    //     ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
    //
    //     if (ImGui::Begin("OcTree", &open))
    //     {
    //         CreateImGuiOcTreeNode(m_OcTree, 0);
    //
    //         ImGui::End();
    //     }
    //
    //     m_Renderer.ImGuiRender(commandBuffer);
    // }

    uint32_t endDrawResult = m_Renderer.EndDraw();

    if (endDrawResult == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }
}

void MeshApplication::CreateImGuiOcTreeNode(const OcTreeTriangles& ocTreeNode, const uint32_t level,
                                            const uint32_t index, int id)
{

    ImGui::PushID(id);
    if (ImGui::TreeNode("OcTreeNode", "Level: %d, Index: %d", level, index))
    {
        uint32_t numOfTriangles = 0;
        if (ocTreeNode.isDivided)
        {
            for (int i = 0; i < 8; i++)
            {
                CreateImGuiOcTreeNode(*ocTreeNode.nodes[i], level + 1, i, id++);
                numOfTriangles += ocTreeNode.nodes[i]->triangles.size();
            }
        }

        ImGui::Text("Num Of Triangles: %d", numOfTriangles);
        ImGui::Text("Boundary:\n\tMinPoint: {%.4f, %.4f, %.4f},\n\tMaxPoint: {%.4f, %.4f, %.4f}",
                    ocTreeNode.boundary.minPoint.x, ocTreeNode.boundary.minPoint.y, ocTreeNode.boundary.minPoint.z,
                    ocTreeNode.boundary.maxPoint.x, ocTreeNode.boundary.maxPoint.y, ocTreeNode.boundary.maxPoint.z);

        ImGui::PushID(id++);
        if (ImGui::TreeNode("Triangles"))
        {

            for (uint32_t i = 0; i < ocTreeNode.triangles.size(); i++)
            {
                ImGui::PushID(id++);
                if (ImGui::TreeNode("Triangle", "[%d]", i))
                {
                    ImGui::Text("A: {%.4f, %.4f, %.4f}\n\tB: {%.4f, %.4f, %.4f}\n\tC: {%.4f, %.4f, %.4f}\n\t",
                                ocTreeNode.triangles[i].a.x, ocTreeNode.triangles[i].a.y, ocTreeNode.triangles[i].a.z,
                                ocTreeNode.triangles[i].b.x, ocTreeNode.triangles[i].b.y, ocTreeNode.triangles[i].b.z,
                                ocTreeNode.triangles[i].c.x, ocTreeNode.triangles[i].c.y, ocTreeNode.triangles[i].c.z);

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void MeshApplication::Loop()
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

void MeshApplication::Shutdown()
{

    VkCore::Device& device = VkCore::DeviceManager::GetDevice();

    m_Renderer.m_Swapchain.Destroy();

    device.DestroyPipeline(m_AxisPipeline);
    device.DestroyPipelineLayout(m_AxisPipelineLayout);

    device.DestroyPipeline(m_ModelPipeline);
    device.DestroyPipelineLayout(m_ModelPipelineLayout);

    device.DestroyPipeline(m_AabbPipeline);
    device.DestroyPipelineLayout(m_AabbPipelineLayout);

    m_Model->Destroy();

    m_AxisBuffer.Destroy();
    m_AxisIndexBuffer.Destroy();

    m_DescriptorBuilder.Clear();
    m_DescriptorBuilder.Cleanup();

    m_AabbBuffer.Destroy();

    for (VkCore::Buffer& buffer : m_MatBuffers)
    {
        buffer.Destroy();
    }

    device.DestroyDescriptorSetLayout(m_MeshDescSetLayout);
    device.DestroyDescriptorSetLayout(m_MatrixDescSetLayout);

    delete m_Window;
}

void MeshApplication::OnEvent(Event& event)
{
    EventDispatcher dispatcher = EventDispatcher(event);

    switch (event.GetEventType())
    {
    case EventType::MouseBtnPressed:
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(MeshApplication::OnMousePress));
        break;
    case EventType::MouseMoved:
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(MeshApplication::OnMouseMoved));
        break;
    case EventType::MouseBtnReleased:
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(MeshApplication::OnMouseRelease));
        break;
    case EventType::KeyPressed:
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(MeshApplication::OnKeyPressed));
        break;
    case EventType::KeyReleased:
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(MeshApplication::OnKeyReleased));
        break;
    case EventType::WindowResized:
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(MeshApplication::OnWindowResize));
        break;
    case EventType::KeyRepeated:
    case EventType::MouseBtnRepeated:
    case EventType::WindowClosed:
    case EventType::MouseScrolled:
        break;
    }
}

bool MeshApplication::OnMousePress(MouseButtonEvent& event)
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
        break;
    }

    LOG(Application, Verbose, "Mouse Pressed")
    return true;
}

bool MeshApplication::OnMouseMoved(MouseMovedEvent& event)
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
        m_Camera.Yaw(-diff.x);
        m_Camera.Pitch(-diff.y);
    }

    if (m_MouseState.m_IsLMBPressed)
    {
        const glm::ivec2 diff = m_MouseState.m_LastPosition - event.GetPos();

        angles += static_cast<glm::vec2>(diff) * 0.01f;

        mesh_pc.rotation_mat = glm::rotate(glm::identity<glm::mat4>(), angles.x, {0.f, 1.f, 0.f});
        mesh_pc.rotation_mat = glm::rotate(mesh_pc.rotation_mat, angles.y, {-1.f, 0.f, 0.f});
    }

    m_MouseState.UpdatePosition(event.GetPos());

    return true;
}

bool MeshApplication::OnMouseRelease(MouseButtonEvent& event)
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
    case GLFW_KEY_M:
        fragment_pc.is_meshlet_view_on = !fragment_pc.is_meshlet_view_on;
        LOGF(Application, Verbose, "Meshlet view is %d", fragment_pc.is_meshlet_view_on)
    default:
        return false;
    }
}

void MeshApplication::RecreateSwapchain()
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

bool MeshApplication::OnWindowResize(WindowResizedEvent& event)
{
    LOG(Window, Verbose, "Window Resized");
    m_FramebufferResized = true;

    return true;
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
