#include <cstdint>
#include <vector>

#include "../Model/PushConstants.h"
#include "App/BaseApplication.h"
#include "Event/WindowEvent.h"
#include "Mesh/Meshlet.h"
#include "Mesh/Model.h"
#include "Model/Camera.h"
#include "Vk/Devices/DeviceManager.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"

class MeshApplication : public VkCore::BaseApplication
{
  public:
    MeshApplication(const uint32_t winWidth, const uint32_t winHeight)
        : BaseApplication(winWidth, winHeight, "Mesh and Task Shading")
    {
        VkCore::DeviceManager::AddDeviceExtension(VK_NV_MESH_SHADER_EXTENSION_NAME);
        VkCore::DeviceManager::AddDeviceExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        VkCore::DeviceManager::AddDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    }

    void PreInitVulkan() override {};
    void PostInitVulkan() override;
    void DrawFrame() override;
    void Shutdown() override;
    void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, const uint32_t imageIndex) override;

    void InitializeMeshPipeline();
    void InitializeModelPipeline();
    void InitializeAxisPipeline();

    void RecreateSwapchain();
    void CreateFramebuffers();

    bool OnMousePress(MouseButtonEvent& event) override;
    bool OnMouseMoved(MouseMovedEvent& event) override;
    bool OnMouseRelease(MouseButtonEvent& event) override;

    bool OnKeyPressed(KeyPressedEvent& event) override;
    bool OnKeyReleased(KeyReleasedEvent& event) override;

    bool OnWindowResize(WindowResizedEvent& event) override;

    vk::Pipeline m_MeshPipeline;
    vk::PipelineLayout m_MeshPipelineLayout;

    vk::Pipeline m_AxisPipeline;
    vk::PipelineLayout m_AxisPipelineLayout;

    vk::Pipeline m_ModelPipeline;
    vk::PipelineLayout m_ModelPipelineLayout;

    vk::CommandPool m_CommandPool;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    std::vector<vk::Semaphore> m_ImageAvailableSemaphores;
    std::vector<vk::Semaphore> m_RenderFinishedSemaphores;

    std::vector<vk::Fence> m_InFlightFences;
    std::vector<VkCore::Buffer> m_MatBuffers;

    std::vector<vk::DescriptorSet> m_MatrixDescriptorSets;
    vk::DescriptorSet m_MeshDescSet;

    vk::DescriptorSetLayout m_MatrixDescSetLayout;
    vk::DescriptorSetLayout m_MeshDescSetLayout;

    VkCore::Buffer m_MeshletBuffer;
    VkCore::Buffer m_VertexBuffer;

    VkCore::Buffer m_AxisBuffer;
    VkCore::Buffer m_AxisIndexBuffer;

    std::vector<Meshlet> m_Meshlets;

    glm::vec2 angles = {0.f, 0.f};

    MeshPC mesh_pc;

    FragmentPC fragment_pc = {
        .diffusion_color = glm::vec3(1.f),
        .is_meshlet_view_on = false,
        .ambient_color = glm::vec3(1.f),
        .specular_color = glm::vec3(1.f),
        .direction = glm::vec3(1.f),
        .cam_pos = glm::vec3(0.f),
        .cam_view_dir = glm::vec3(0.f),
    };

    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNv;
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;

    Model* m_Model = nullptr;
    Camera m_Camera;
};
