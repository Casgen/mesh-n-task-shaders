#include "App/BaseApplication.h"
#include "Model/Camera.h"
#include "Vk/Texture/Texture2D.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"
#include "Mesh/Meshlet.h"
#include <cstdint>
#include <vector>

class MeshApplication : public VkCore::BaseApplication
{
  public:
    MeshApplication(const uint32_t winWidth, const uint32_t winHeight)
        : BaseApplication(winWidth, winHeight, "Mesh and Task Shading")
    {
        AddDeviceExtension(VK_NV_MESH_SHADER_EXTENSION_NAME);
        AddDeviceExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        AddDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    }

    void PreInitVulkan() override{};
    void PostInitVulkan() override;
    void DrawFrame() override;
    void Shutdown() override{};
    void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, const uint32_t imageIndex) override;

    void InitializeMeshPipeline();
    void InitializeAxisPipeline();

    bool OnMousePress(MouseButtonEvent& event) override;
    bool OnMouseMoved(MouseMovedEvent& event) override;
    bool OnMouseRelease(MouseButtonEvent& event) override;

    bool OnKeyPressed(KeyPressedEvent& event) override;
    bool OnKeyReleased(KeyReleasedEvent& event) override;

    VkCore::Texture2D m_Texture;

    vk::Pipeline m_MeshPipeline;
    vk::PipelineLayout m_MeshPipelineLayout;

    vk::Pipeline m_AxisPipeline;
    vk::PipelineLayout m_AxisPipelineLayout;

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


    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNv;

    std::vector<glm::vec4> m_CubeVertices = {{-1, -1, -1, 1}, {1, -1, -1, 1}, {1, -1, 1, 1}, {-1, -1, 1, 1},
                                             {-1, 1, -1, 1},  {1, 1, -1, 1},  {1, 1, 1, 1},  {-1, 1, 1, 1}};

    std::vector<float> m_CubeVerticesFloats = {
        -1, -1, -1, 1,  1.0, 0.0, 0.0, 1.0,
        1, -1, -1, 1,   0.0, 1.0, 0.0, 0.0,
        1, 1, -1, 1,    0.0, 0.0, 1.0, 1.0,  
        -1, 1, -1, 1,   1.0, 1.0, 1.0, 1.0,

        1, -1, 1, 1,    0.0, 1.0, 0.0, 1.0,
        -1, -1, 1, 1,   1.0, 0.0, 0.0, 1.0,
        -1, 1, 1, 1,    1.0, 1.0, 1.0, 1.0,
        1, 1, 1, 1,     0.0, 0.0, 1.0, 1.0,
    };
    // std::vector<float> m_CubeVerticesFloats = {
    //     -1, -1, -1, 1,  1.0, 0.0, 0.0, 1.0,
    //     1, -1, -1, 1,   0.0, 1.0, 0.0, 0.0,
    //     1, 1, -1, 1,    0.0, 0.0, 1.0, 1.0,  
    //     -1, 1, -1, 1,   1.0, 1.0, 1.0, 1.0,
    //
    //     -1, -1, 1, 1,   1.0, 0.0, 0.0, 1.0,
    //     1, -1, 1, 1,    0.0, 1.0, 0.0, 1.0,
    //     1, 1, 1, 1,     0.0, 0.0, 1.0, 1.0,
    //     -1, 1, 1, 1,    1.0, 1.0, 1.0, 1.0,
    // };
    
    std::vector<uint32_t> m_CubeIndices = {
        0, 1, 2,    2, 3, 0, // Front
        // 1, 5, 6,    6, 2, 1, // Right
        4, 5, 6,    6, 7, 4, // Back
        // 4, 0, 3,    3, 7, 4, // Left
        // 1, 0, 4,    4, 5, 1, // Bottom
        // 2, 6, 7,    7, 3, 2, // Top
    };

    // std::vector<uint32_t> m_CubeIndices = {
    //     0, 1, 2,    2, 3, 0, // Front
        // 1, 5, 6,    6, 2, 1, // Right
        // 5, 4, 7,    7, 6, 5, // Back
        // 4, 0, 3,    3, 7, 4, // Left
        // 1, 0, 4,    4, 5, 1, // Bottom
        // 2, 6, 7,    7, 3, 2, // Top
    // };
    Camera m_Camera;
};
