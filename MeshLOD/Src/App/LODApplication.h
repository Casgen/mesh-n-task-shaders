#include <cstdint>
#include <vector>

#include "../Model/PushConstants.h"
#include "../../Common/Renderer/VulkanRenderer.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Event/WindowEvent.h"
#include "Mesh/LODModel.h"
#include "Mesh/Model.h"
#include "Model/Camera.h"
#include "Model/MouseState.h"
#include "Model/Structures/Sphere.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"

class LODApplication
{
  public:
    LODApplication() {};

    void Run(const uint32_t winWidth, const uint32_t winHeight);

    void DrawFrame();
    void Loop();
    void Shutdown();

    void InitializeModelPipeline();
    void InitializeAxisPipeline();
	void InitializeBoundsPipeline();
	void InitializeFrustumPipeline();
	void InitializeInstancing();

    void RecreateSwapchain();

    void OnEvent(Event& event);

    bool OnMousePress(MouseButtonEvent& event);
    bool OnMouseMoved(MouseMovedEvent& event);
    bool OnMouseRelease(MouseButtonEvent& event);
	bool OnMouseScrolled(MouseScrolledEvent& event);

    bool OnKeyPressed(KeyPressedEvent& event);
    bool OnKeyReleased(KeyReleasedEvent& event);

    bool OnWindowResize(WindowResizedEvent& event);

  private:
    MouseState m_MouseState;

    vk::Pipeline m_AxisPipeline;
    vk::PipelineLayout m_AxisPipelineLayout;

    vk::Pipeline m_ModelPipeline;
    vk::PipelineLayout m_ModelPipelineLayout;

	vk::Pipeline m_BoundsPipeline;
	vk::PipelineLayout m_BoundsPipelineLayout;

	vk::Pipeline m_FrustumPipeline;
	vk::PipelineLayout m_FrustumPipelineLayout;

    std::vector<VkCore::Buffer> m_MatBuffers;
    std::vector<vk::DescriptorSet> m_MatrixDescriptorSets;
    vk::DescriptorSetLayout m_MatrixDescSetLayout;

    VkCore::Buffer m_AxisBuffer;
    VkCore::Buffer m_AxisIndexBuffer;

	VkCore::Buffer m_FrustumBuffer;
	VkCore::Buffer m_FrustumIndexBuffer;

	VkCore::Buffer m_InstancesBuffer;
	vk::DescriptorSet m_InstancesDescSet;
	vk::DescriptorSetLayout m_InstancesDescSetLayout;

    glm::vec2 angles = {0.f, 0.f};

    LodPC lod_pc;

	glm::uvec2 m_InstanceSize = glm::uvec3(200);
	const uint32_t m_InstanceCountMax = m_InstanceSize.x * m_InstanceSize.y;


    FragmentPC fragment_pc = {
        .diffusion_color = glm::vec3(1.f),
        .is_meshlet_view_on = 0,
        .ambient_color = glm::vec3(1.f),
        .specular_color = glm::vec3(1.f),
        .direction = glm::vec3(1.f),
        .cam_pos = glm::vec3(0.f),
        .cam_view_dir = glm::vec3(0.f),
    };

	SphereModel m_Sphere;

	// ImGui Params
	float m_ZenithAngle;
	float m_AzimuthAngle;
	bool m_AzimuthSweepEnabled = false;
	bool m_ZenithSweepEnabled = false;
	bool m_PossesCamera = true;
	bool m_EnableCulling = true;
	int m_InstanceCount = 30000;
	glm::vec3 m_Position;

	uint64_t m_Duration = 0;
	uint64_t m_AvgDuration = 0;
	uint64_t m_AccDuration = 0;
	uint32_t m_Counter = 0;


#ifndef VK_MESH_EXT
    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNv;
#else
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
#endif


    VkCore::DescriptorBuilder m_DescriptorBuilder;

    bool m_FramebufferResized = false;

    VulkanRenderer m_Renderer;
    VkCore::Window* m_Window = nullptr;

    LODModel* m_Model = nullptr;

	std::array<Model*, 3> m_AvailableModels = {nullptr, nullptr, nullptr};

    Camera m_Camera;
	Camera m_FrustumCamera;
	Camera* m_CurrentCamera = nullptr;
};
