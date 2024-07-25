#include <cstdint>
#include <vector>

#include "../Model/PushConstants.h"
#include "../Renderer/VulkanRenderer.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Event/WindowEvent.h"
#include "Model/Camera.h"
#include "Model/MouseState.h"
#include "Vk/Descriptors/DescriptorBuilder.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "Vk/Texture/Image2D.h"

class TessApplication
{
  public:
    TessApplication() {};

    void Run(const uint32_t winWidth, const uint32_t winHeight);

    void DrawFrame();
    void Loop();
    void Shutdown();

    void InitializeTessPipeline();
    void InitializeAxisPipeline();
    void InitializeNoisePipeline();

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

    vk::Pipeline m_WaterPipeline;
    vk::PipelineLayout m_WaterPipelineLayout;

    vk::Pipeline m_ComputePipeline;
    vk::PipelineLayout m_ComputePipelineLayout;

    std::vector<VkCore::Buffer> m_MatBuffers;
    std::vector<vk::DescriptorSet> m_MatrixDescriptorSets;
    vk::DescriptorSetLayout m_MatrixDescSetLayout;

    VkCore::Buffer m_AxisBuffer;
    VkCore::Buffer m_AxisIndexBuffer;

    VkCore::Image2D m_NoiseHeight;
    VkCore::Image2D m_NoiseNormals;

    vk::DescriptorSet m_ComputeNoiseSet;
    vk::DescriptorSetLayout m_ComputeNoiseSetLayout;

    vk::DescriptorSet m_MeshNoiseSet;
    vk::DescriptorSetLayout m_MeshNoiseSetLayout;

    // ImGui Params
    glm::ivec2 m_PatchCounts = {5, 5};
    VkPolygonMode m_PolygonMode = VK_POLYGON_MODE_FILL;
    bool m_EnableLineMode = false;
    uint64_t m_Duration = 0;
	int32_t m_GridSize = 1;

	const uint32_t m_NoiseResolution = 2048;

	TessPC tess_pc = {
		.scale = 1.f
	};

    NoisePC noise_pc = {
        .time = 0.f,
        .speed = 1.f,
        .height = 1.f,
		.octaves = 1,
    };

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

    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;

    VkCore::DescriptorBuilder m_DescriptorBuilder;

    bool m_FramebufferResized = false;

    VulkanRenderer m_Renderer;
    VkCore::Window* m_Window = nullptr;

    Camera m_Camera;
};
