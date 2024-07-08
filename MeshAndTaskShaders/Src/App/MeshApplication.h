#include <cstdint>
#include <vector>

#include "../Model/PushConstants.h"
#include "../Renderer/VulkanRenderer.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Event/WindowEvent.h"
#include "Mesh/Meshlet.h"
#include "Mesh/Model.h"
#include "Model/Camera.h"
#include "Model/MouseState.h"
#include "Model/Structures/AABB.h"
#include "Model/Structures/OcTree.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"

class MeshApplication
{
  public:
    MeshApplication() {};

    void Run(const uint32_t winWidth, const uint32_t winHeight);

    void DrawFrame();
    void Loop();
    void Shutdown();

	void CreateImGuiOcTreeNode(const OcTreeTriangles& ocTreeNode, const uint32_t level, const uint32_t index = 0, int id = 0);

    void InitializeModelPipeline();
    void InitializeAxisPipeline();
    void InitializeNormalsPipeline();

    void RecreateSwapchain();

    void OnEvent(Event& event);

    bool OnMousePress(MouseButtonEvent& event);
    bool OnMouseMoved(MouseMovedEvent& event);
    bool OnMouseRelease(MouseButtonEvent& event);

    bool OnKeyPressed(KeyPressedEvent& event);
    bool OnKeyReleased(KeyReleasedEvent& event);

    bool OnWindowResize(WindowResizedEvent& event);

  private:
    MouseState m_MouseState;

    vk::Pipeline m_AxisPipeline;
    vk::PipelineLayout m_AxisPipelineLayout;

    vk::Pipeline m_ModelPipeline;
    vk::PipelineLayout m_ModelPipelineLayout;

    vk::Pipeline m_AabbPipeline;
    vk::PipelineLayout m_AabbPipelineLayout;

	vk::Pipeline m_NormalsPipeline;
	vk::PipelineLayout m_NormalsPipelineLayout;

    std::vector<VkCore::Buffer> m_MatBuffers;

    std::vector<vk::DescriptorSet> m_MatrixDescriptorSets;
    vk::DescriptorSet m_MeshDescSet;

    vk::DescriptorSetLayout m_MatrixDescSetLayout;
    vk::DescriptorSetLayout m_MeshDescSetLayout;

    VkCore::Buffer m_MeshletBuffer;
    VkCore::Buffer m_VertexBuffer;

    VkCore::Buffer m_AabbBuffer;
	OcTreeTriangles m_OcTree;
	uint32_t m_OcTreeEdgesCount = 0;

    VkCore::Buffer m_AxisBuffer;
    VkCore::Buffer m_AxisIndexBuffer;

    std::vector<Meshlet> m_Meshlets;

    glm::vec2 angles = {0.f, 0.f};

    MeshPC mesh_pc;

	AABB m_Aabb;

    FragmentPC fragment_pc = {
        .diffusion_color = glm::vec3(1.f),
        .is_meshlet_view_on = false,
        .ambient_color = glm::vec3(1.f),
        .specular_color = glm::vec3(1.f),
        .direction = glm::vec3(1.f),
        .cam_pos = glm::vec3(0.f),
        .cam_view_dir = glm::vec3(0.f),
    };

#ifndef VK_MESH_EXT
    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNv;
#else
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
#endif


    VkCore::DescriptorBuilder m_DescriptorBuilder;

    bool m_FramebufferResized = false;

    VulkanRenderer m_Renderer;
    VkCore::Window* m_Window = nullptr;

    Model* m_Model = nullptr;
    Camera m_Camera;
};
