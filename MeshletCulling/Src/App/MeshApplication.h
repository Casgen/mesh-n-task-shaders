#include <cstdint>
#include <vector>

#include "../Model/PushConstants.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Event/WindowEvent.h"
#include "Mesh/Model.h"
#include "../../Common/Renderer/VulkanRenderer.h"
#include "Model/Camera.h"
#include "Model/MouseState.h"
#include "Model/Structures/Sphere.h"
#include "Platform/Window.h"
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

    void InitializeModelPipeline();
    void InitializeAxisPipeline();
	void InitializeBoundsPipeline();
	void InitializeFrustumPipeline();

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

	vk::Pipeline m_FrustumPipeline;
	vk::PipelineLayout m_FrustumPipelineLayout;

	vk::Pipeline m_BoundsPipeline;
	vk::PipelineLayout m_BoundsPipelineLayout;

    std::vector<VkCore::Buffer> m_MatBuffers;

    std::vector<vk::DescriptorSet> m_MatrixDescriptorSets;
    vk::DescriptorSet m_MeshDescSet;

    vk::DescriptorSetLayout m_MatrixDescSetLayout;

    VkCore::Buffer m_AxisBuffer;
    VkCore::Buffer m_AxisIndexBuffer;

    VkCore::Buffer m_FrustumBuffer;
    VkCore::Buffer m_FrustumIndexBuffer;

    glm::vec2 angles = {0.f, 0.f};

    MeshPC mesh_pc;

    FragmentPC fragment_pc = {
        .diffusion_color = glm::vec3(1.f),
        .is_meshlet_view_on = 0,
        .ambient_color = glm::vec3(1.f),
        .specular_color = glm::vec3(1.f),
        .direction = glm::vec3(1.f),
        .cam_pos = glm::vec3(0.f),
        .cam_view_dir = glm::vec3(0.f),
    };


	// ImGui Params
	float m_ZenithAngle;
	float m_AzimuthAngle;
	bool m_AzimuthSweepEnabled = true;
	bool m_ZenithSweepEnabled = false;
	glm::vec3 m_Position;

	SphereModel m_Sphere;


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
	Camera m_FrustumCamera;
};
