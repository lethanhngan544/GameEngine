#include <Renderer.h>

namespace eg::Renderer
{
	//VertexBuffer constructor
	GPUBuffer::GPUBuffer(void* data, size_t dataSize, vk::BufferUsageFlags usage)
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

		if (data)
		{
			std::memcpy(info.pMappedData, data, dataSize);
		}

		//Create vertex buffer
		auto [buffer, allocation] = getAllocator().createBuffer(
			vk::BufferCreateInfo{
				{},
				dataSize,
				usage | vk::BufferUsageFlagBits::eTransferDst,
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

	//VertexBuffer destructor
	GPUBuffer::~GPUBuffer()
	{
		getAllocator().destroyBuffer(this->mBuffer, this->mAllocation);
	}

}