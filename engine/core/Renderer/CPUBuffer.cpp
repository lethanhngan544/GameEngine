#include <Renderer.h>
#include <Logger.h>

namespace eg::Renderer
{
	CPUBuffer::CPUBuffer(void* data, size_t dataSize, vk::BufferUsageFlags usage)
	{
		//Create vertex buffer
		auto [stagingBuffer, stagingAllocation] = getAllocator().createBuffer(
			vk::BufferCreateInfo{
				{},
				dataSize,
				usage | vk::BufferUsageFlagBits::eTransferSrc,
			},
			vma::AllocationCreateInfo{
				vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
				vma::MemoryUsage::eAutoPreferHost,
			},
			mInfo
			);
		if (data != nullptr)
		{
			std::memcpy(mInfo.pMappedData, data, dataSize);
		}
		this->mBuffer = stagingBuffer;
		this->mAllocation = stagingAllocation;
	}

	CPUBuffer::~CPUBuffer()
	{
		getAllocator().destroyBuffer(this->mBuffer, this->mAllocation);
	}

	void CPUBuffer::write(const void* data, size_t size)
	{
		std::memcpy(mInfo.pMappedData, data, size);
	}
}