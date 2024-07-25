#include "Query.h"
#include "Vk/Devices/DeviceManager.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "Log/Log.h"

DurationQuery::DurationQuery(const uint32_t timestampCounts) : m_QueryCounts(timestampCounts * 2)
{
    vk::PhysicalDeviceLimits limits = VkCore::DeviceManager::GetPhysicalDevice().GetDeviceLimits();

    m_TimestampPeriod = limits.timestampPeriod;

    vk::QueryPoolCreateInfo createInfo;

    createInfo.queryType = vk::QueryType::eTimestamp;
    createInfo.queryCount = timestampCounts * 2;

    m_QueryPool = VkCore::DeviceManager::GetDevice().CreateQueryPool(createInfo);
}

void DurationQuery::Reset(const vk::CommandBuffer& cmdBuffer)
{
	cmdBuffer.resetQueryPool(m_QueryPool, 0, m_QueryCounts);
}

DurationQuery::~DurationQuery()
{
    VkCore::DeviceManager::GetDevice().DestroyQueryPool(m_QueryPool);
}

void DurationQuery::Begin(const vk::CommandBuffer& cmdBuffer)
{
    cmdBuffer.beginQuery(m_QueryPool, 0, {});
}

void DurationQuery::End(const vk::CommandBuffer& cmdBuffer)
{
    cmdBuffer.endQuery(m_QueryPool, 0);
}

void DurationQuery::StartTimestamp(const vk::CommandBuffer& cmdBuffer, const vk::PipelineStageFlagBits pipelineStage)
{
    ASSERT(m_ParityCount % 2 == 0 && m_ParityCount < m_QueryCounts,
           "You have start the timestamp before even ending the previous one or you have used too many timestamps!")

    cmdBuffer.writeTimestamp(pipelineStage, m_QueryPool, m_ParityCount++);
}

void DurationQuery::EndTimestamp(const vk::CommandBuffer& cmdBuffer, const vk::PipelineStageFlagBits pipelineStage)
{
    ASSERT(m_ParityCount % 2 == 1 && m_ParityCount < m_QueryCounts,
           "You have ended the timestamp before even starting it beforehand or you have used too many timestamps!")

    cmdBuffer.writeTimestamp(pipelineStage, m_QueryPool, m_ParityCount++);
}

uint64_t DurationQuery::GetResults()
{
    vk::ResultValue<std::vector<uint64_t>> rv =
        (*VkCore::DeviceManager::GetDevice())
            .getQueryPoolResults<uint64_t>(m_QueryPool, 0, 2, 2 * sizeof(uint64_t), sizeof(uint64_t),
                                           vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    if (rv.result != vk::Result::eSuccess)
    {
        LOGF(Vulkan, Error, "Failed to get result from the query!: %d", rv.result)
    }


    return (rv.value[1] - rv.value[0]) * m_TimestampPeriod;
}
