#pragma once

#include <vulkan/vulkan.hpp>

class DurationQuery
{
  public:
	DurationQuery(DurationQuery&& other) = delete;
	DurationQuery(const DurationQuery& other) = delete;
    DurationQuery(const uint32_t timestampCounts = 1);
	~DurationQuery();

	void Reset(const vk::CommandBuffer& cmdBuffer);
	void StartTimestamp(const vk::CommandBuffer& cmdBuffer, const vk::PipelineStageFlagBits pipelineStage);
	void EndTimestamp(const vk::CommandBuffer& cmdBuffer, const vk::PipelineStageFlagBits pipelineStage);
	void Begin(const vk::CommandBuffer& cmdBuffer);
	void End(const vk::CommandBuffer& cmdBuffer);
	uint64_t GetResults();

  private:
	uint32_t m_QueryCounts = 0;
	uint32_t m_ParityCount = 0;
    vk::QueryPool m_QueryPool;
	uint32_t m_TimestampPeriod = 0;
};
