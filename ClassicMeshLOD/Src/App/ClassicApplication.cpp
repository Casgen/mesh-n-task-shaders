#include "ClassicApplication.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stddef.h>
#include <stdexcept>

#include "Constants.h"
#include "GLFW/glfw3.h"
#include "Log/Log.h"
#include "Mesh/ClassicLODMesh.h"
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
#include "../../Common/Query.h"

void ClassicApplication::Run(const uint32_t winWidth, const uint32_t winHeight)
{
    Logger::SetSeverityFilter(ESeverity::Verbose);

    m_Window = new VkCore::Window("Mesh Application", winWidth, winHeight);
    m_Window->SetEventCallback(std::bind(&ClassicApplication::OnEvent, this, std::placeholders::_1));

    m_Renderer = VulkanRenderer("Mesh Application", m_Window,
                                {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                 VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME},
                                {});
    m_Renderer.InitImGui(m_Window, winWidth, winHeight);

    m_Camera =
        Camera({-1.f, 3.f, -1.f}, {1.f, 0.5f, 1.f}, (float)m_Window->GetWidth() / m_Window->GetHeight(), 45.f, 50.f);
    m_CurrentCamera = &m_Camera;
    m_FrustumCamera =
        Camera({-1.f, 3.f, -1.f}, {1.f, 0.5f, 1.f}, (float)m_Window->GetWidth() / m_Window->GetHeight(), 45.f, 40.f);

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
                                       vk::ShaderStageFlagBits::eVertex);
        m_DescriptorBuilder.Build(tempSet, m_MatrixDescSetLayout);
        m_DescriptorBuilder.Clear();

        m_MatrixDescriptorSets.emplace_back(tempSet);
    }

    InitializeInstancing();
    InitializeModelPipeline();

    InitializeLODCompute();
    InitializeAxisPipeline();
    InitializeFrustumPipeline();

    Loop();
    Shutdown();
}

void ClassicApplication::InitializeModelPipeline()
{

    m_Model = new ClassicLODModel("ClassicMeshLOD/Res/Artwork/OBJs/lucy_lod0.obj");

    const std::vector<VkCore::ShaderData> shaders =
        VkCore::ShaderLoader::LoadClassicShaders("ClassicMeshLOD/Res/Shaders/lod", false);

    ClassicLODMeshInfo meshInfo = m_Model->GetMesh(0).GetMeshInfo();

    m_LODMeshInfo = VkCore::Buffer(vk::BufferUsageFlagBits::eStorageBuffer);
    m_LODMeshInfo.InitializeOnGpu(&meshInfo, sizeof(ClassicLODMeshInfo));

    m_DescriptorBuilder
        .BindBuffer(0, m_LODMeshInfo, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
        .Build(m_LODMeshInfoSet, m_LODMeshInfoSetLayout);

    m_DescriptorBuilder.Clear();

    VkCore::VertexAttributeBuilder attributeBuilder = Vertex::CreateAttributeBuilder();

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice(), false);

    m_ModelPipeline = pipelineBuilder.BindShaderModules(shaders)
                          .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                          .EnableDepthTest()
                          .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                          .FrontFaceDirection(vk::FrontFace::eClockwise)
                          .SetCullMode(vk::CullModeFlagBits::eBack)
                          .AddDisabledBlendAttachment()
                          .BindVertexAttributes(attributeBuilder)
                          .AddDescriptorLayout(m_MatrixDescSetLayout)
                          .AddDescriptorLayout(m_InstancesDescSetLayout)
                          .AddDescriptorLayout(m_ScratchSetLayout)
                          .AddPushConstantRange<FragmentPC>(vk::ShaderStageFlagBits::eFragment)
                          .SetPrimitiveAssembly(vk::PrimitiveTopology::eTriangleList)
                          .AddDynamicState(vk::DynamicState::eScissor)
                          .AddDynamicState(vk::DynamicState::eViewport)
                          .Build(m_ModelPipelineLayout);
}

void ClassicApplication::InitializeLODCompute()
{

    VkCore::ShaderData computeShader =
        VkCore::ShaderLoader::LoadComputeShader("ClassicMeshLOD/Res/Shaders/lod_compute.comp", false);

    VkCore::ComputePipelineBuilder pipelineBuilder{};

    m_LODCalculatePipeline = pipelineBuilder.BindShaderModule(computeShader)
                                 .AddPushConstantRange<LodPC>(vk::ShaderStageFlagBits::eCompute)
                                 .AddDescriptorLayout(m_LODMeshInfoSetLayout)
                                 .AddDescriptorLayout(m_InstancesDescSetLayout)
                                 .AddDescriptorLayout(m_ScratchSetLayout)
                                 .AddDescriptorLayout(m_DrawIndirectCmdsLayout)
                                 .Build(m_LODCalculatePipelineLayout);

    computeShader = VkCore::ShaderLoader::LoadComputeShader("ClassicMeshLOD/Res/Shaders/lod_sort.comp", false);

    pipelineBuilder.Reset();

    m_LODPreparePipeline = pipelineBuilder.BindShaderModule(computeShader)
                               .AddPushConstantRange<LodPC>(vk::ShaderStageFlagBits::eCompute)
                               .AddDescriptorLayout(m_LODMeshInfoSetLayout)
                               .AddDescriptorLayout(m_InstancesDescSetLayout)
                               .AddDescriptorLayout(m_ScratchSetLayout)
                               .AddDescriptorLayout(m_DrawIndirectCmdsLayout)
                               .Build(m_LODPreparePipelineLayout);

    m_DescriptorBuilder.Clear();
}

void ClassicApplication::InitializeAxisPipeline()
{

    const float m_AxisVertexData[12] = {
        0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
    };

    uint32_t m_AxisIndexData[6] = {
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
        VkCore::ShaderLoader::LoadClassicShaders("ClassicMeshLOD/Res/Shaders/axis");
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

void ClassicApplication::InitializeBoundsPipeline()
{

    m_Sphere = SphereModel(Vec3f(0.f, 0.f, 0.f));

    VkCore::VertexAttributeBuilder attributeBuilder;

    attributeBuilder.PushAttribute<float>(3).PushAttribute<float>(3).PushAttribute<float>(3);

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("ClassicMeshLOD/Res/Shaders/bounds");

    m_BoundsPipeline = pipelineBuilder.BindShaderModules(shaderData)
                           .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                           .EnableDepthTest()
                           .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                           .FrontFaceDirection(vk::FrontFace::eClockwise)
                           .SetCullMode(vk::CullModeFlagBits::eNone)
                           .BindVertexAttributes(attributeBuilder)
                           .AddDisabledBlendAttachment()
                           .AddDescriptorLayout(m_MatrixDescSetLayout)
                           .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                           .AddDynamicState(vk::DynamicState::eScissor)
                           .AddDynamicState(vk::DynamicState::eViewport)
                           .Build(m_BoundsPipelineLayout);
}

void ClassicApplication::InitializeFrustumPipeline()
{
    std::tie(m_FrustumBuffer, m_FrustumIndexBuffer) = m_FrustumCamera.ConstructFrustumModel();

    VkCore::VertexAttributeBuilder attributeBuilder{};

    attributeBuilder.PushAttribute<float>(3).PushAttribute<float>(3).PushAttribute<float>(3);
    attributeBuilder.SetBinding(0);

    std::vector<VkCore::ShaderData> shaderData =
        VkCore::ShaderLoader::LoadClassicShaders("ClassicMeshLOD/Res/Shaders/frustum");

    VkCore::GraphicsPipelineBuilder pipelineBuilder(VkCore::DeviceManager::GetDevice());

    m_FrustumPipeline = pipelineBuilder.BindShaderModules(shaderData)
                            .BindRenderPass(m_Renderer.m_RenderPass.GetVkRenderPass())
                            .AddViewport(glm::uvec4(0, 0, m_Window->GetWidth(), m_Window->GetHeight()))
                            .FrontFaceDirection(vk::FrontFace::eCounterClockwise)
                            .SetCullMode(vk::CullModeFlagBits::eNone)
                            .BindVertexAttributes(attributeBuilder)
                            .AddDisabledBlendAttachment()
                            .SetLineWidth(2.f)
                            .AddPushConstantRange<Frustum>(vk::ShaderStageFlagBits::eVertex)
                            .AddDescriptorLayout(m_MatrixDescSetLayout)
                            .SetPrimitiveAssembly(vk::PrimitiveTopology::eLineList)
                            .AddDynamicState(vk::DynamicState::eScissor)
                            .AddDynamicState(vk::DynamicState::eViewport)
                            .Build(m_FrustumPipelineLayout);
}

void ClassicApplication::InitializeInstancing()
{

    const glm::vec3 span = glm::vec3(1.f);

    std::vector<glm::mat4> instances;
    instances.resize(m_InstanceCountMax);

    for (uint32_t y = 0; y < m_InstanceSize.y; y++)
    {
        for (uint32_t x = 0; x < m_InstanceSize.x; x++)
        {
            glm::vec3 instancePos = {x * span.x, 0.f, y * span.y};

            const uint32_t index = x + m_InstanceSize.y * y;

            instances[index] = (glm::translate(glm::identity<glm::mat4>(), instancePos));
        }
    }

    m_InstancesBuffer = VkCore::Buffer(vk::BufferUsageFlagBits::eStorageBuffer);
    m_InstancesBuffer.InitializeOnGpu(instances.data(), instances.size() * sizeof(glm::mat4));

    m_DescriptorBuilder
        .BindBuffer(0, m_InstancesBuffer, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute)
        .Build(m_InstancesDescSet, m_InstancesDescSetLayout);

    m_DescriptorBuilder.Clear();

    for (int i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
        m_DrawIndirectCmds.emplace_back(vk::BufferUsageFlagBits::eStorageBuffer |
                                        vk::BufferUsageFlagBits::eIndirectBuffer |
                                        vk::BufferUsageFlagBits::eTransferDst);

        m_DrawIndirectCmds[i].InitializeOnGpu(Constants::MAX_LOD_LEVELS * sizeof(vk::DrawIndexedIndirectCommand));

        m_DescriptorBuilder.BindBuffer(0, m_DrawIndirectCmds[i], vk::DescriptorType::eStorageBuffer,
                                       vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute);

        vk::DescriptorSet set;

        m_DescriptorBuilder.Build(set, m_DrawIndirectCmdsLayout);
        m_DrawIndirectCmdSets.emplace_back(set);

        m_DescriptorBuilder.Clear();
    }


    for (int i = 0; i < m_Renderer.m_Swapchain.GetImageCount(); i++)
    {
		m_ScratchBuffers.emplace_back(vk::BufferUsageFlagBits::eStorageBuffer);
		m_ScratchBuffers[i].InitializeOnGpu(sizeof(uint32_t) * m_InstanceCountMax);

		m_InstanceIndexBuffers.emplace_back(vk::BufferUsageFlagBits::eStorageBuffer);
		m_InstanceIndexBuffers[i].InitializeOnGpu(m_InstanceCountMax * sizeof(uint32_t));

		vk::DescriptorSet set;

		m_DescriptorBuilder
			.BindBuffer(0, m_ScratchBuffers[i], vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
			.BindBuffer(1, m_InstanceIndexBuffers[i], vk::DescriptorType::eStorageBuffer,
						vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.Build(set, m_ScratchSetLayout);

		m_ScratchSets.emplace_back(set);

		m_DescriptorBuilder.Clear();
	}
}

void ClassicApplication::DrawFrame()
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

    FrustumMatrixBuffer ubo{};
    ubo.m_Proj = m_CurrentCamera->GetProjMatrix();
    ubo.m_View = m_CurrentCamera->GetViewMatrix();

    Frustum frustum = m_FrustumCamera.CalculateFrustum();
    lod_pc.frustum = frustum;

    fragment_pc.cam_pos = m_CurrentCamera->GetPosition();
    fragment_pc.cam_view_dir = m_CurrentCamera->GetViewDirection();

    m_MatBuffers[imageIndex].UpdateData(&ubo);

    m_Renderer.BeginCmdBuffer();
    vk::CommandBuffer cmdBuffer = m_Renderer.GetCurrentCmdBuffer();

    DurationQuery durationQuery;

    durationQuery.Reset(cmdBuffer);

    {
        // Compute the LODs
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_LODCalculatePipeline);
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, m_LODCalculatePipelineLayout, 0,
            {m_LODMeshInfoSet, m_InstancesDescSet, m_ScratchSets[imageIndex], m_DrawIndirectCmdSets[imageIndex]}, {});

        cmdBuffer.pushConstants(m_LODCalculatePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(LodPC),
                                &lod_pc);

        cmdBuffer.fillBuffer(m_DrawIndirectCmds[imageIndex].GetVkBuffer(), 0, m_DrawIndirectCmds[imageIndex].GetSize(),
                             0);

        vk::BufferMemoryBarrier drawIndirectBarrier = m_DrawIndirectCmds[imageIndex].CreateBufferMemoryBarrier(
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);

        cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                                  {}, {}, drawIndirectBarrier, {});

        durationQuery.StartTimestamp(cmdBuffer, vk::PipelineStageFlagBits::eComputeShader);

        cmdBuffer.dispatch(((uint32_t)m_InstanceCount / 32) + 1, 1, 1);

        vk::MemoryBarrier memoryBarrier;
        memoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        memoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                                  {}, memoryBarrier, {}, {});
    }
    {
        // Sort and Prepare the LOD Instances data for drawing
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_LODPreparePipeline);

        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, m_LODPreparePipelineLayout, 0,
            {m_LODMeshInfoSet, m_InstancesDescSet, m_ScratchSets[imageIndex], m_DrawIndirectCmdSets[imageIndex]}, {});

        cmdBuffer.pushConstants(m_LODPreparePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(LodPC),
                                &lod_pc);

        cmdBuffer.dispatch(1, 1, 1);

        vk::MemoryBarrier memoryBarrier;
        memoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        memoryBarrier.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;

        cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect,
                                  {}, memoryBarrier, {}, {});
    }

    m_Renderer.BeginRenderPass({0.3f, 0.f, 0.2f, 1.f}, m_Window->GetWidth(), m_Window->GetHeight());

    vk::Rect2D scissor = vk::Rect2D({0, 0}, {m_Window->GetWidth(), m_Window->GetHeight()});
    cmdBuffer.setScissor(0, 1, &scissor);

    vk::Viewport viewport = vk::Viewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight(), 0, 1);
    cmdBuffer.setViewport(0, 1, &viewport);

    {

        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_ModelPipeline);

        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ModelPipelineLayout, 0,
                                     {m_MatrixDescriptorSets[imageIndex], m_InstancesDescSet, m_ScratchSets[imageIndex]}, {});

        cmdBuffer.pushConstants(m_ModelPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPC),
                                &fragment_pc);

        ClassicLODMesh& mesh = m_Model->GetMesh(0);

        cmdBuffer.bindVertexBuffers(0, mesh.GetVertexBuffer().GetVkBuffer(), {0});
        cmdBuffer.bindIndexBuffer(mesh.GetIndexBuffer().GetVkBuffer(), 0, vk::IndexType::eUint32);

        cmdBuffer.drawIndexedIndirect(m_DrawIndirectCmds[imageIndex].GetVkBuffer(), 0, mesh.GetMeshInfo().LodCount,
                                      sizeof(vk::DrawIndexedIndirectCommand));

        durationQuery.EndTimestamp(cmdBuffer, vk::PipelineStageFlagBits::eFragmentShader);
    }

    //
    // {
    //
    //     cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_FrustumPipeline);
    //     cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_FrustumPipelineLayout, 0, 1,
    //                                  &m_MatrixDescriptorSets[imageIndex], 0, nullptr);
    //
    //     cmdBuffer.pushConstants(m_FrustumPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Frustum),
    //                             &frustum);
    //
    //     cmdBuffer.bindVertexBuffers(0, m_FrustumBuffer.GetVkBuffer(), {0});
    //
    //     cmdBuffer.bindIndexBuffer(m_FrustumIndexBuffer.GetVkBuffer(), 0, vk::IndexType::eUint32);
    //     cmdBuffer.drawIndexed(32, 1, 0, 0, 0);
    // }
    //
    {
        m_Renderer.ImGuiNewFrame(m_Window->GetWidth(), m_Window->GetHeight());

        static bool open = true;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Instancing", &open))
        {
            ImGui::Text("Drawing execution in ms: %.4f", m_Duration / 1000000.f);
            ImGui::Text("Avg. Drawing execution in ms: %.4f", m_AvgDuration / 1000000.f);
            ImGui::Text("Instance Count");
            if (ImGui::SliderInt("##Instance Count", &m_InstanceCount, 0, (int)m_InstanceCountMax, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
            {
                lod_pc.instances_count = (uint32_t)m_InstanceCount;
            }

            ImGui::Text("LOD Exponent");
            ImGui::SliderFloat("##LOD Exponent", &lod_pc.lod_pow, 0, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

            ImGui::Text("Show LODs with color");
            ImGui::SameLine();
            ImGui::Checkbox("##Show LODs with color", (bool*)&fragment_pc.lod_color);

            ImGui::Text("Enable culling");
            ImGui::SameLine();

            if (ImGui::Checkbox("##Enable culling", &m_EnableCulling))
            {
                lod_pc.enable_culling = m_EnableCulling;
            };

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

        m_Renderer.ImGuiRender(cmdBuffer);
    }

    uint32_t endDrawResult = m_Renderer.EndDraw();
    m_AccDuration += m_Duration = durationQuery.GetResults();

    m_Counter++;
    m_Counter %= 180;

    if (m_Counter >= 179)
    {
        m_AvgDuration = m_AccDuration / 179;
        m_AccDuration = 0;
    }

    if (endDrawResult == -1)
    {
        m_FramebufferResized = true;
        RecreateSwapchain();
        return;
    }
}

void ClassicApplication::Loop()
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

void ClassicApplication::Shutdown()
{

    VkCore::Device& device = VkCore::DeviceManager::GetDevice();

    device.WaitIdle();

    m_Renderer.m_Swapchain.Destroy();

    device.DestroyPipeline(m_AxisPipeline);
    device.DestroyPipelineLayout(m_AxisPipelineLayout);

    device.DestroyPipeline(m_ModelPipeline);
    device.DestroyPipelineLayout(m_ModelPipelineLayout);

    device.DestroyPipeline(m_BoundsPipeline);
    device.DestroyPipelineLayout(m_BoundsPipelineLayout);

    device.DestroyPipeline(m_FrustumPipeline);
    device.DestroyPipelineLayout(m_FrustumPipelineLayout);

    device.DestroyPipeline(m_LODCalculatePipeline);
    device.DestroyPipelineLayout(m_LODCalculatePipelineLayout);

    m_Model->Destroy();

    m_AxisBuffer.Destroy();
    m_AxisIndexBuffer.Destroy();

    m_FrustumBuffer.Destroy();
    m_FrustumIndexBuffer.Destroy();

    for (VkCore::Buffer& buffer : m_DrawIndirectCmds)
    {
        buffer.Destroy();
    }

    m_InstancesBuffer.Destroy();

    for (VkCore::Buffer& buffer : m_InstanceIndexBuffers)
    {
        buffer.Destroy();
    }

    for (VkCore::Buffer& buffer : m_ScratchBuffers)
    {
        buffer.Destroy();
    }

    for (VkCore::Buffer& buffer : m_MatBuffers)
    {
        buffer.Destroy();
    }

    device.DestroyDescriptorSetLayout(m_MatrixDescSetLayout);
    device.DestroyDescriptorSetLayout(m_ScratchSetLayout);
    device.DestroyDescriptorSetLayout(m_InstancesDescSetLayout);

    m_DescriptorBuilder.Clear();
    m_DescriptorBuilder.Cleanup();

    delete m_Window;
}

void ClassicApplication::OnEvent(Event& event)
{
    EventDispatcher dispatcher = EventDispatcher(event);

    switch (event.GetEventType())
    {
    case EventType::MouseBtnPressed:
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(ClassicApplication::OnMousePress));
        break;
    case EventType::MouseMoved:
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(ClassicApplication::OnMouseMoved));
        break;
    case EventType::MouseBtnReleased:
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(ClassicApplication::OnMouseRelease));
        break;
    case EventType::MouseScrolled:
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(ClassicApplication::OnMouseScrolled));
    case EventType::KeyPressed:
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(ClassicApplication::OnKeyPressed));
        break;
    case EventType::KeyReleased:
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(ClassicApplication::OnKeyReleased));
        break;
    case EventType::WindowResized:
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(ClassicApplication::OnWindowResize));
        break;
    case EventType::KeyRepeated:
    case EventType::MouseBtnRepeated:
    case EventType::WindowClosed:
        break;
    }
}

bool ClassicApplication::OnMousePress(MouseButtonEvent& event)
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

bool ClassicApplication::OnMouseMoved(MouseMovedEvent& event)
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
        m_CurrentCamera->Yaw(-diff.x);
        m_CurrentCamera->Pitch(-diff.y);
    }

    if (m_MouseState.m_IsLMBPressed)
    {
        const glm::ivec2 diff = m_MouseState.m_LastPosition - event.GetPos();

        angles += static_cast<glm::vec2>(diff) * 0.01f;

        // lod_pc.rotation_mat = glm::rotate(glm::identity<glm::mat4>(), angles.x, {0.f, 1.f, 0.f});
        // lod_pc.rotation_mat = glm::rotate(lod_pc.rotation_mat, angles.y, {-1.f, 0.f, 0.f});
    }

    m_MouseState.UpdatePosition(event.GetPos());

    return true;
}

bool ClassicApplication::OnMouseRelease(MouseButtonEvent& event)
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

bool ClassicApplication::OnMouseScrolled(MouseScrolledEvent& event)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        ImGui_ImplGlfw_ScrollCallback(m_Window->GetGLFWWindow(), event.GetXOffset(), event.GetYOffset());
        return true;
    }

    m_CurrentCamera->AddMovementSpeed((float)event.GetYOffset() * 0.01f);

    return true;
}

bool ClassicApplication::OnKeyPressed(KeyPressedEvent& event)
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
    default:
        return false;
    }
}

void ClassicApplication::RecreateSwapchain()
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

bool ClassicApplication::OnWindowResize(WindowResizedEvent& event)
{
    LOG(Window, Verbose, "Window Resized");
    m_FramebufferResized = true;

    return true;
}

bool ClassicApplication::OnKeyReleased(KeyReleasedEvent& event)
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
