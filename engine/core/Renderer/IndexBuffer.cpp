#include <Renderer.h>


namespace eg::Renderer
{
	IndexBuffer::IndexBuffer(uint32_t* data, size_t dataSize)
	{
		//Create vertex buffer
		vma::AllocationInfo info;

		//Create staging
		auto [stagingBuffer, stagingAllocation] = getAllocator().createBuffer(
			vk::BufferCreateInfo{
				{},
				dataSize,
				vk::BufferUsageFlagBits::eTransferSrc,
			},
			vma::AllocationCreateInfo{
				vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
				vma::MemoryUsage::eAutoPreferHost,
			},
			info
			);

		std::memcpy(info.pMappedData, data, dataSize);

		//Create index buffer
		auto [buffer, allocation] = getAllocator().createBuffer(
			vk::BufferCreateInfo{
				{},
				dataSize,
				vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			},
			vma::AllocationCreateInfo{
				{},
				vma::MemoryUsage::eAutoPreferDevice,
			}
			);

		this->mBuffer = buffer;
		this->mAllocation = allocation;

		//Copy staging to vertex buffer
		immediateSubmit([&](vk::CommandBuffer cmd)
			{
				vk::BufferCopy copyRegion{};
				copyRegion.setSize(dataSize);
				cmd.copyBuffer(stagingBuffer, buffer, copyRegion);
			});
		getAllocator().destroyBuffer(stagingBuffer, stagingAllocation);
	}

	IndexBuffer::~IndexBuffer()
	{
		getAllocator().destroyBuffer(this->mBuffer, this->mAllocation);
	}
}