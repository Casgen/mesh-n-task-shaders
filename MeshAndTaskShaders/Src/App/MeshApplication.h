#include "App/BaseApplication.h"
#include "Model/Camera.h"
#include "Vk/Texture/Texture2D.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"
#include <cstdint>
#include <vector>

class MeshApplication : public VkCore::BaseApplication
{
  public:
    MeshApplication(const uint32_t winWidth, const uint32_t winHeight)
        : BaseApplication(winWidth, winHeight, "Mesh and Task Shading")
    {
        AddDeviceExtension(VK_NV_MESH_SHADER_EXTENSION_NAME);
    }

    void PreInitVulkan() override{};
    void PostInitVulkan() override;
    void DrawFrame() override;
    void Shutdown() override{};
    void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, const uint32_t imageIndex) override;

    bool OnMousePress(MouseButtonEvent& event) override;
    bool OnMouseMoved(MouseMovedEvent& event) override;
    bool OnMouseRelease(MouseButtonEvent& event) override;

    bool OnKeyPressed(KeyPressedEvent& event) override;
    bool OnKeyReleased(KeyReleasedEvent& event) override;

    VkCore::Texture2D m_Texture;

    vk::Pipeline m_Pipeline;
    vk::PipelineLayout m_PipelineLayout;

    vk::CommandPool m_CommandPool;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    std::vector<vk::Semaphore> m_ImageAvailableSemaphores;
    std::vector<vk::Semaphore> m_RenderFinishedSemaphores;

    std::vector<vk::Fence> m_InFlightFences;

    std::vector<VkCore::Buffer> m_MatBuffers;
    std::vector<vk::DescriptorSet> m_DescriptorSets;
    vk::DescriptorSetLayout m_DescriptorSetLayout;

    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNv;

    Camera m_Camera;
};
