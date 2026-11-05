/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Local includes
#include "rmlrenderinterface.h"
#include "rmlshaders.h"

// NAP includes
#include <renderservice.h>
#include <renderwindow.h>
#include <nap/logger.h>

// Std includes
#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

// glslang, same runtime compilation as naprender's shader.cpp
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

// STB for image decoding of document textures
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_MSC
#include <stb_image.h>

// Rml includes
#include <RmlUi/Core/Vertex.h>

namespace nap
{
	using namespace rtti;

	static const uint32 RML_DESCRIPTOR_SETS = 512;

	// Size of the RmlUi vertex: position (8) + premultiplied color (4) + texcoord (8)
	static_assert(sizeof(Rml::Vertex) == 20, "Unexpected RmlUi vertex size");

	//////////////////////////////////////////////////////////////////////////

	// One-off glslang initialization
	static void initializeGlslang()
	{
		static std::once_flag flag;
		std::call_once(flag, []()
		{
			glslang::InitializeProcess();
		});
	}

	// Returns the Khronos default resource limits, same values as naprender's shader.cpp
	static const TBuiltInResource& getDefaultResources();

	// Compiles a single GLSL shader stage to SPIR-V, modeled after naprender's parseShader
	static bool compileShaderStage(const char* shaderCode, const char* shaderName, EShLanguage stage, std::vector<uint32>& outSpirV, utility::ErrorState& errorState)
	{
		glslang::TShader shader(stage);
		shader.setStrings(&shaderCode, 1);
		shader.setAutoMapBindings(true);
		shader.setAutoMapLocations(true);
		shader.setEntryPoint("main");

		const int defaultVersion = 450;

		EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
		if (!shader.parse(&getDefaultResources(), defaultVersion, false, messages))
		{
			errorState.fail("%s: failed to parse: %s\n%s", shaderName, shader.getInfoLog(), shader.getInfoDebugLog());
			return false;
		}

		glslang::TProgram program;
		program.addShader(&shader);
		if (!program.link(messages))
		{
			errorState.fail("%s: failed to link: %s\n%s", shaderName, program.getInfoLog(), program.getInfoDebugLog());
			return false;
		}

		glslang::SpvOptions options;
		options.validate = true;
		options.generateDebugInfo = false;
		glslang::GlslangToSpv(*program.getIntermediate(stage), outSpirV, &options);
		return true;
	}

	// Creates a shader module from SPIR-V
	static bool createShaderModule(const std::vector<uint32>& spirv, VkDevice device, VkShaderModule& outModule, utility::ErrorState& errorState)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = spirv.size() * sizeof(uint32);
		createInfo.pCode = spirv.data();

		VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &outModule);
		if (!errorState.check(result == VK_SUCCESS, "Failed to create shader module, error %d", result))
			return false;
		return true;
	}

	// Returns the Khronos default resource limits, same values as naprender's shader.cpp
	static const TBuiltInResource& getDefaultResources()
	{
		static const TBuiltInResource defaultResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,
		/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
		}};
		return defaultResource;
	}

	//////////////////////////////////////////////////////////////////////////

	RmlRenderInterface::RmlRenderInterface(RenderService& renderService) :
		mRenderService(renderService)
	{
	}

	RmlRenderInterface::~RmlRenderInterface()
	{
		shutdown();
	}

	bool RmlRenderInterface::init(utility::ErrorState& errorState)
	{
		assert(!mInitialized);
		initializeGlslang();

		VkDevice device = mRenderService.getDevice();

		// Compile the shaders to SPIR-V
		std::vector<uint32> vertexSpirv, colorSpirv, textureSpirv;
		if (!compileShaderStage(rmlVertexShader, "rml_vertex", EShLangVertex, vertexSpirv, errorState) ||
			!compileShaderStage(rmlFragmentColorShader, "rml_fragment_color", EShLangFragment, colorSpirv, errorState) ||
			!compileShaderStage(rmlFragmentTextureShader, "rml_fragment_texture", EShLangFragment, textureSpirv, errorState))
		{
			return false;
		}

		if (!createShaderModule(vertexSpirv, device, mVertexShader, errorState) ||
			!createShaderModule(colorSpirv, device, mColorShader, errorState) ||
			!createShaderModule(textureSpirv, device, mTextureShader, errorState))
		{
			return false;
		}

		// Descriptor set layout: one combined image sampler for the textured pipeline
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		if (!errorState.check(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mDescriptorLayout) == VK_SUCCESS, "Failed to create RmlUi descriptor set layout"))
			return false;

		// Pipeline layout: one descriptor set + push constants on the vertex stage
		VkPushConstantRange pushRange = {};
		pushRange.offset = 0;
		pushRange.size = sizeof(PushConstants);
		pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &mDescriptorLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushRange;
		if (!errorState.check(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &mPipelineLayout) == VK_SUCCESS, "Failed to create RmlUi pipeline layout"))
			return false;

		// Sampler: linear filtering, clamp to edge, no mipmaps
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.25f;
		if (!errorState.check(vkCreateSampler(device, &samplerInfo, nullptr, &mSampler) == VK_SUCCESS, "Failed to create RmlUi texture sampler"))
			return false;

		// Descriptor pool, individual sets are freed when textures are released
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = RML_DESCRIPTOR_SETS;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = RML_DESCRIPTOR_SETS;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		if (!errorState.check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool) == VK_SUCCESS, "Failed to create RmlUi descriptor pool"))
			return false;

		// Command pool for one-off image uploads on the render queue
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolCreateInfo.queueFamilyIndex = mRenderService.getQueueIndex();
		if (!errorState.check(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &mUploadPool) == VK_SUCCESS, "Failed to create RmlUi upload command pool"))
			return false;

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = mUploadPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		if (!errorState.check(vkAllocateCommandBuffers(device, &allocInfo, &mUploadBuffer) == VK_SUCCESS, "Failed to allocate RmlUi upload command buffer"))
			return false;

		mInitialized = true;
		return true;
	}

	void RmlRenderInterface::shutdown()
	{
		if (!mInitialized)
			return;

		// Textures and geometry release their buffers through the render
		// service's destructor queue; anything still alive is destroyed here.
		VkDevice device = mRenderService.getDevice();
		VmaAllocator allocator = mRenderService.getVulkanAllocator();

		for (auto& entry : mTextures)
		{
			RmlTexture& texture = *entry.second;
			vkDestroyImageView(device, texture.mView, nullptr);
			vmaDestroyImage(allocator, texture.mImage, texture.mAllocation);
		}
		mTextures.clear();

		vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
		vkDestroySampler(device, mSampler, nullptr);

		for (auto& entry : mPipelines)
		{
			vkDestroyPipeline(device, entry.second.mColorPipeline, nullptr);
			vkDestroyPipeline(device, entry.second.mTexturePipeline, nullptr);
		}
		mPipelines.clear();

		vkDestroyShaderModule(device, mVertexShader, nullptr);
		vkDestroyShaderModule(device, mColorShader, nullptr);
		vkDestroyShaderModule(device, mTextureShader, nullptr);
		vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, mDescriptorLayout, nullptr);

		vkFreeCommandBuffers(device, mUploadPool, 1, &mUploadBuffer);
		vkDestroyCommandPool(device, mUploadPool, nullptr);

		mInitialized = false;
	}

	void RmlRenderInterface::beginDraw(RenderWindow& window)
	{
		// The RmlUi context works in the pixels of the render target, which is
		// the window's drawable size (the size the swapchain converges to)
		glm::ivec2 size = window.getBufferSize();
		if (size.x <= 0 || size.y <= 0)
			size = window.getSwapchainSize();
		mWidth = size.x;
		mHeight = size.y;
		mPixelWidth = size.x;
		mPixelHeight = size.y;

		VkRenderPass renderPass = window.getRenderPass();
		VkSampleCountFlagBits samples = window.getSampleCount();

		// Pipelines are created per (render pass, sample count) combination
		uint64 key = (static_cast<uint64>(reinterpret_cast<uintptr_t>(renderPass)) << 8) | static_cast<uint64>(samples);
		auto it = mPipelines.find(key);
		if (it == mPipelines.end())
		{
			utility::ErrorState error;
			if (!createPipelines(renderPass, samples, error))
			{
				nap::Logger::error("RmlUi: failed to create pipelines for window '%s': %s", window.getTitle().c_str(), error.toString().c_str());
				mActivePipelines = nullptr;
				return;
			}
			it = mPipelines.find(key);
		}
		mActivePipelines = &it->second;

		applyFullScissor();
	}

	bool RmlRenderInterface::createPipelines(VkRenderPass renderPass, VkSampleCountFlagBits samples, utility::ErrorState& errorState)
	{
		VkDevice device = mRenderService.getDevice();

		// Vertex input: Rml::Vertex, tightly packed
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Rml::Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position) };
		attributeDescriptions[1] = { 1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour) };
		attributeDescriptions[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord) };

		VkPipelineVertexInputStateCreateInfo vertexInput = {};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexBindingDescriptions = &bindingDescription;
		vertexInput.vertexAttributeDescriptionCount = static_cast<uint32>(attributeDescriptions.size());
		vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = samples;

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState blendAttachment = {};
		blendAttachment.blendEnable = VK_TRUE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlend = {};
		colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments = &blendAttachment;

		std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = mVertexShader;
		stages[0].pName = "main";

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlend;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = mPipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		PipelineSet pipelineSet;

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = mColorShader;
		stages[1].pName = "main";
		if (!errorState.check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineSet.mColorPipeline) == VK_SUCCESS, "Failed to create RmlUi color pipeline"))
			return false;

		stages[1].module = mTextureShader;
		if (!errorState.check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineSet.mTexturePipeline) == VK_SUCCESS, "Failed to create RmlUi texture pipeline"))
		{
			vkDestroyPipeline(device, pipelineSet.mColorPipeline, nullptr);
			return false;
		}

		uint64 key = (static_cast<uint64>(reinterpret_cast<uintptr_t>(renderPass)) << 8) | static_cast<uint64>(samples);
		mPipelines.emplace(key, pipelineSet);
		return true;
	}

	Rml::CompiledGeometryHandle RmlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
	{
		if (vertices.size() == 0 || indices.size() == 0)
			return {};

		VmaAllocator allocator = mRenderService.getVulkanAllocator();

		std::unique_ptr<RmlGeometry> geometry = std::make_unique<RmlGeometry>();
		geometry->mIndexCount = static_cast<uint32>(indices.size());

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		// Vertex buffer
		bufferInfo.size = vertices.size() * sizeof(Rml::Vertex);
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		VmaAllocationInfo vertexInfo = {};
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &geometry->mVertexBuffer, &geometry->mVertexAllocation, &vertexInfo) != VK_SUCCESS)
		{
			nap::Logger::error("RmlUi: failed to allocate vertex buffer of %d bytes", bufferInfo.size);
			return {};
		}
		memcpy(vertexInfo.pMappedData, vertices.data(), bufferInfo.size);

		// Index buffer
		bufferInfo.size = indices.size() * sizeof(int);
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		VmaAllocationInfo indexInfo = {};
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &geometry->mIndexBuffer, &geometry->mIndexAllocation, &indexInfo) != VK_SUCCESS)
		{
			nap::Logger::error("RmlUi: failed to allocate index buffer of %d bytes", bufferInfo.size);
			vmaDestroyBuffer(allocator, geometry->mVertexBuffer, geometry->mVertexAllocation);
			return {};
		}
		memcpy(indexInfo.pMappedData, indices.data(), bufferInfo.size);

		return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry.release());
	}

	void RmlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
	{
		if (mActivePipelines == nullptr)
			return;

		RmlGeometry* renderGeometry = reinterpret_cast<RmlGeometry*>(geometry);
		if (renderGeometry == nullptr)
			return;

		VkCommandBuffer commandBuffer = mRenderService.getCurrentCommandBuffer();

		// Combined projection and transform, uploaded column-major.
		// Vulkan's clip space differs from OpenGL: y points down and depth spans [0, 1].
		// The standard correction matrix flips y and remaps z accordingly.
		PushConstants pushConstants = {};
		Rml::Matrix4f projection = Rml::Matrix4f::ProjectOrtho(0.0f, static_cast<float>(mWidth), static_cast<float>(mHeight), 0.0f, -1.0f, 1.0f);
		Rml::Matrix4f correction = Rml::Matrix4f::Identity();
		correction.SetColumns(
			Rml::Vector4f(1.0f, 0.0f, 0.0f, 0.0f),
			Rml::Vector4f(0.0f, -1.0f, 0.0f, 0.0f),
			Rml::Vector4f(0.0f, 0.0f, 0.5f, 0.0f),
			Rml::Vector4f(0.0f, 0.0f, 0.5f, 1.0f));
		projection = correction * projection;
		Rml::Matrix4f combined = projection * (mTransformActive ? mTransform : Rml::Matrix4f::Identity());
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
				pushConstants.mTransform[column * 4 + row] = combined[column][row];
		}
		pushConstants.mTranslate[0] = translation.x;
		pushConstants.mTranslate[1] = translation.y;

		VkBuffer vertexBuffers[] = { renderGeometry->mVertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, renderGeometry->mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

		if (texture != 0)
		{
			auto it = mTextures.find(texture);
			if (it == mTextures.end())
			{
				assert(false);
				return;
			}
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mActivePipelines->mTexturePipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &it->second->mDescriptorSet, 0, nullptr);
		}
		else
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mActivePipelines->mColorPipeline);
		}

		vkCmdDrawIndexed(commandBuffer, renderGeometry->mIndexCount, 1, 0, 0, 0);
	}

	void RmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
	{
		RmlGeometry* renderGeometry = reinterpret_cast<RmlGeometry*>(geometry);
		if (renderGeometry == nullptr)
			return;

		VkBuffer vertexBuffer = renderGeometry->mVertexBuffer;
		VmaAllocation vertexAllocation = renderGeometry->mVertexAllocation;
		VkBuffer indexBuffer = renderGeometry->mIndexBuffer;
		VmaAllocation indexAllocation = renderGeometry->mIndexAllocation;

		// Frames in flight may still reference the buffers
		mRenderService.queueVulkanObjectDestructor([this, vertexBuffer, vertexAllocation, indexBuffer, indexAllocation](RenderService& service)
		{
			VmaAllocator allocator = service.getVulkanAllocator();
			vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
			vmaDestroyBuffer(allocator, indexBuffer, indexAllocation);
		});

		delete renderGeometry;
	}

	Rml::TextureHandle RmlRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
	{
		texture_dimensions = Rml::Vector2i(0, 0);

		FILE* file = fopen(source.c_str(), "rb");
		if (file == nullptr)
		{
			nap::Logger::warn("RmlUi: unable to open texture file: %s", source.c_str());
			return 0;
		}

		int width = 0, height = 0, channels = 0;
		unsigned char* data = stbi_load_from_file(file, &width, &height, &channels, 4);
		fclose(file);
		if (data == nullptr)
		{
			nap::Logger::warn("RmlUi: unable to decode texture file: %s", source.c_str());
			return 0;
		}

		texture_dimensions = Rml::Vector2i(width, height);
		Rml::Span<const Rml::byte> span(data, static_cast<size_t>(width) * height * 4);
		utility::ErrorState errorState;
		Rml::TextureHandle handle = createTexture(span, texture_dimensions, errorState);
		stbi_image_free(data);

		if (handle == 0)
			nap::Logger::error("RmlUi: failed to upload texture: %s", source.c_str());
		return handle;
	}

	Rml::TextureHandle RmlRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
	{
		utility::ErrorState errorState;
		Rml::TextureHandle handle = createTexture(source, source_dimensions, errorState);
		if (handle == 0)
			nap::Logger::error("RmlUi: failed to generate texture: %s", errorState.toString().c_str());
		return handle;
	}

	Rml::TextureHandle RmlRenderInterface::createTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i sourceDimensions, utility::ErrorState& errorState)
	{
		if (!mInitialized || sourceDimensions.x <= 0 || sourceDimensions.y <= 0 || source.empty())
			return 0;

		VkDevice device = mRenderService.getDevice();
		VmaAllocator allocator = mRenderService.getVulkanAllocator();

		const size_t pixelCount = static_cast<size_t>(sourceDimensions.x) * sourceDimensions.y;

		// RmlUi font atlases arrive as single channel data, one byte per pixel;
		// everything else is RGBA8
		std::vector<Rml::byte> rgbaData;
		const Rml::byte* uploadData = source.data();
		if (source.size() == pixelCount)
		{
			rgbaData.resize(pixelCount * 4);
			for (size_t i = 0; i < pixelCount; ++i)
			{
				rgbaData[i * 4 + 0] = 255;
				rgbaData[i * 4 + 1] = 255;
				rgbaData[i * 4 + 2] = 255;
				rgbaData[i * 4 + 3] = source[i];
			}
			uploadData = rgbaData.data();
		}
		else if (source.size() != pixelCount * 4)
		{
			errorState.fail("Unexpected texture data size %d for dimensions %dx%d", source.size(), sourceDimensions.x, sourceDimensions.y);
			return 0;
		}

		std::unique_ptr<RmlTexture> texture = std::make_unique<RmlTexture>();

		// Image, device-local, sampled and transfer destination
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.extent = { static_cast<uint32>(sourceDimensions.x), static_cast<uint32>(sourceDimensions.y), 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo imageAllocInfo = {};
		imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		if (!errorState.check(vmaCreateImage(allocator, &imageInfo, &imageAllocInfo, &texture->mImage, &texture->mAllocation, nullptr) == VK_SUCCESS, "Failed to allocate RmlUi texture image"))
			return 0;

		if (!uploadImage(texture->mImage, uploadData, sourceDimensions.x, sourceDimensions.y, errorState))
			return 0;

		// Image view
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture->mImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		if (!errorState.check(vkCreateImageView(device, &viewInfo, nullptr, &texture->mView) == VK_SUCCESS, "Failed to create RmlUi texture image view"))
			return 0;

		// Descriptor set pointing at the image through the shared sampler
		VkDescriptorSetAllocateInfo descriptorAllocInfo = {};
		descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorAllocInfo.descriptorPool = mDescriptorPool;
		descriptorAllocInfo.descriptorSetCount = 1;
		descriptorAllocInfo.pSetLayouts = &mDescriptorLayout;
		if (!errorState.check(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &texture->mDescriptorSet) == VK_SUCCESS, "Failed to allocate RmlUi descriptor set"))
			return 0;

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.sampler = mSampler;
		descriptorImageInfo.imageView = texture->mView;
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = texture->mDescriptorSet;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &descriptorImageInfo;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		Rml::TextureHandle handle = mNextTextureHandle++;
		mTextures.emplace(handle, std::move(texture));
		return handle;
	}

	bool RmlRenderInterface::uploadImage(VkImage image, const void* data, uint32_t width, uint32_t height, utility::ErrorState& errorState)
	{
		VmaAllocator allocator = mRenderService.getVulkanAllocator();
		VkDeviceSize dataSize = static_cast<VkDeviceSize>(width) * height * 4;

		// Staging buffer
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = dataSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = nullptr;
		VmaAllocationInfo stagingInfo = {};
		if (!errorState.check(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, &stagingInfo) == VK_SUCCESS, "Failed to allocate RmlUi staging buffer"))
			return false;

		memcpy(stagingInfo.pMappedData, data, dataSize);

		VkResult result = vkResetCommandBuffer(mUploadBuffer, 0);
		if (!errorState.check(result == VK_SUCCESS, "Failed to reset RmlUi upload command buffer, error %d", result))
			return false;

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (!errorState.check(vkBeginCommandBuffer(mUploadBuffer, &beginInfo) == VK_SUCCESS, "Failed to begin RmlUi upload command buffer"))
			return false;

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(mUploadBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy copyRegion = {};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(mUploadBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(mUploadBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (!errorState.check(vkEndCommandBuffer(mUploadBuffer) == VK_SUCCESS, "Failed to end RmlUi upload command buffer"))
			return false;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mUploadBuffer;
		result = vkQueueSubmit(mRenderService.getQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		if (!errorState.check(result == VK_SUCCESS, "Failed to submit RmlUi upload command buffer, error %d", result))
			return false;

		result = vkQueueWaitIdle(mRenderService.getQueue());
		if (!errorState.check(result == VK_SUCCESS, "Failed to wait for RmlUi upload queue, error %d", result))
			return false;

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return true;
	}

	void RmlRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
	{
		auto it = mTextures.find(texture);
		if (it == mTextures.end())
			return;

		std::unique_ptr<RmlTexture> owned = std::move(it->second);
		mTextures.erase(it);

		VkDevice device = mRenderService.getDevice();
		VkImageView view = owned->mView;
		VkImage image = owned->mImage;
		VmaAllocation allocation = owned->mAllocation;
		VkDescriptorSet descriptorSet = owned->mDescriptorSet;
		VkDescriptorPool descriptorPool = mDescriptorPool;

		// Frames in flight may still sample the image
		mRenderService.queueVulkanObjectDestructor([view, image, allocation, descriptorSet, descriptorPool](RenderService& service)
		{
			VkDevice device = service.getDevice();
			VmaAllocator allocator = service.getVulkanAllocator();
			vkDestroyImageView(device, view, nullptr);
			vmaDestroyImage(allocator, image, allocation);
			vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
		});
	}

	void RmlRenderInterface::EnableScissorRegion(bool enable)
	{
		mScissorEnabled = enable;
		if (!enable)
			applyFullScissor();
	}

	void RmlRenderInterface::SetScissorRegion(Rml::Rectanglei region)
	{
		// RmlUi provides the region in device pixels, which matches the
		// framebuffer 1:1, so no scaling is needed
		const float scaleX = mWidth > 0 ? static_cast<float>(mPixelWidth) / static_cast<float>(mWidth) : 1.0f;
		const float scaleY = mHeight > 0 ? static_cast<float>(mPixelHeight) / static_cast<float>(mHeight) : 1.0f;
		const int minX = std::max<int>(0, region.p0.x);
		const int minY = std::max<int>(0, region.p0.y);
		const int maxX = std::min<int>(region.p1.x, mWidth);
		const int maxY = std::min<int>(region.p1.y, mHeight);
		VkRect2D rect;
		rect.offset.x = static_cast<int>(std::round(static_cast<float>(minX) * scaleX));
		rect.offset.y = static_cast<int>(std::round(static_cast<float>(minY) * scaleY));
		rect.extent.width = static_cast<uint32>(std::max<int>(0, static_cast<int>(std::round(static_cast<float>(maxX - minX) * scaleX))));
		rect.extent.height = static_cast<uint32>(std::max<int>(0, static_cast<int>(std::round(static_cast<float>(maxY - minY) * scaleY))));
		applyScissor(rect);
	}

	void RmlRenderInterface::SetTransform(const Rml::Matrix4f* transform)
	{
		if (transform != nullptr)
		{
			mTransform = *transform;
			mTransformActive = true;
		}
		else
		{
			mTransformActive = false;
		}
	}

	void RmlRenderInterface::applyFullScissor()
	{
		VkRect2D rect;
		rect.offset = { 0, 0 };
		rect.extent = { static_cast<uint32>(std::max<int>(mPixelWidth, 1)), static_cast<uint32>(std::max<int>(mPixelHeight, 1)) };
		applyScissor(rect);
	}

	void RmlRenderInterface::applyScissor(const VkRect2D& rect)
	{
		VkCommandBuffer commandBuffer = mRenderService.getCurrentCommandBuffer();
		if (commandBuffer == VK_NULL_HANDLE)
			return;

		// The viewport is a dynamic pipeline state and must be set every frame,
		// the pipelines would otherwise rasterize into an undefined viewport
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mPixelWidth);
		viewport.height = static_cast<float>(mPixelHeight);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		vkCmdSetScissor(commandBuffer, 0, 1, &rect);
	}
}
