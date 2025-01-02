#pragma once

#include <MyVulkan.h>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <glm/vec3.hpp>
#include <functional>

namespace eg::Renderer
{
	//Global functions
	std::vector<uint32_t> compileShaderFromFile(const std::string& filePath, uint32_t kind);
	void immediateSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);


	void create(uint32_t width, uint32_t height);
	void destory();

	void drawTestTriangle();

	vk::Instance getInstance();
	vk::Device getDevice();
	vma::Allocator getAllocator();

	//Pipelines
	class DefaultPipeline
	{
	public:
		struct Vertex
		{
			glm::vec3 pos;
		};

	private:
		vk::Pipeline mPipeline;
		vk::PipelineLayout mPipelineLayout;
	public:
		DefaultPipeline();
		~DefaultPipeline();
		vk::Pipeline getPipeline() const { return mPipeline; }
		vk::PipelineLayout getPipelineLayout() const { return mPipelineLayout; };
	};


	//Datas
	class VertexBuffer
	{
	private:
		vk::Buffer mBuffer;
		vma::Allocation mAllocation;
	public:
		VertexBuffer(void* data, size_t dataSize);
		~VertexBuffer();
		vk::Buffer getBuffer() const { return mBuffer; }
	};

	class IndexBuffer
	{		
	private:
		vk::Buffer mBuffer;
		vma::Allocation mAllocation;
	public:
		IndexBuffer(uint32_t* data, size_t dataSize);
		~IndexBuffer();
		vk::Buffer getBuffer() const { return mBuffer; }
	};
}