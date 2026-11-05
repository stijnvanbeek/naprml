/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// External includes
#include <RmlUi/Core/RenderInterface.h>
#include <vk_mem_alloc.h>

// NAP includes
#include <utility/errorstate.h>

// Std includes
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace nap
{
	class RenderService;
	class RenderWindow;

	/**
	 * Compiled geometry: vertex and index buffers that survive between frames.
	 * Buffers are host-visible and persistently mapped; UI geometry is small
	 * enough that the copy is negligible and no upload command buffer is needed.
	 */
	struct RmlGeometry
	{
		VkBuffer		mVertexBuffer = VK_NULL_HANDLE;
		VmaAllocation	mVertexAllocation = nullptr;
		VkBuffer		mIndexBuffer = VK_NULL_HANDLE;
		VmaAllocation	mIndexAllocation = nullptr;
		uint32_t		mIndexCount = 0;
	};

	/**
	 * Uploaded texture: RGBA8 device-local image with one descriptor set that
	 * points at it through the shared sampler.
	 */
	struct RmlTexture
	{
		VkImage			mImage = VK_NULL_HANDLE;
		VmaAllocation	mAllocation = nullptr;
		VkImageView		mView = VK_NULL_HANDLE;
		VkDescriptorSet	mDescriptorSet = VK_NULL_HANDLE;
	};

	/**
	 * RmlUi render interface, translates RmlUi geometry into Vulkan draws
	 * recorded into the render service's current command buffer.
	 *
	 * The shaders are compiled to SPIR-V at runtime with the same glslang
	 * runtime that naprender uses. Graphics pipelines are created lazily,
	 * per (render pass, sample count) combination, on the first draw into a
	 * window. Both pipelines blend premultiplied alpha (src = 1, dst = 1 - src
	 * alpha) and use dynamic viewport and scissor state.
	 *
	 * RmlService calls beginDraw() right before Rml::Context::Render() so the
	 * interface knows which window, render pass and size it is drawing into.
	 */
	class RmlRenderInterface final : public Rml::RenderInterface
	{
	public:
		RmlRenderInterface(RenderService& renderService);
		~RmlRenderInterface() override;

		/**
		 * Compiles the shaders and creates sampler, descriptor pool and upload
		 * command pool. Call once, from the owning service's init().
		 */
		bool init(utility::ErrorState& errorState);

		/**
		 * Destroys all Vulkan resources. Call after Rml::Shutdown(), from the
		 * owning service's shutdown().
		 */
		void shutdown();

		/**
		 * Called by RmlService directly before the context render of the given
		 * window. Prepares the pipelines for that window's render pass and
		 * resets viewport and scissor.
		 */
		void beginDraw(RenderWindow& window);

		// Geometry is compiled once and re-used between frames
		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
		void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
		void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

		// Textures; backed by NAP textures so documents can reference images
		Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
		Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
		void ReleaseTexture(Rml::TextureHandle texture) override;

		// Scissor state
		void EnableScissorRegion(bool enable) override;
		void SetScissorRegion(Rml::Rectanglei region) override;

		// Transform; RmlUi passes nullptr when the transform is reset to identity
		void SetTransform(const Rml::Matrix4f* transform) override;

	private:
		// Pipeline set for one (render pass, sample count) combination
		struct PipelineSet
		{
			VkPipeline			mColorPipeline = VK_NULL_HANDLE;
			VkPipeline			mTexturePipeline = VK_NULL_HANDLE;
		};

		// Push constant block, matches the vertex shader's PushConstants
		struct PushConstants
		{
			float		mTransform[16];		// combined projection * transform, column-major
			float		mTranslate[4];		// per-draw translation in pixels
		};

		bool createPipelines(VkRenderPass renderPass, VkSampleCountFlagBits samples, utility::ErrorState& errorState);
		Rml::TextureHandle createTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i sourceDimensions, utility::ErrorState& errorState);
		bool uploadImage(VkImage image, const void* data, uint32_t width, uint32_t height, utility::ErrorState& errorState);
		void applyFullScissor();
		void applyScissor(const VkRect2D& rect);

	private:
		RenderService&			mRenderService;

		// Shaders
		VkShaderModule			mVertexShader = VK_NULL_HANDLE;
		VkShaderModule			mColorShader = VK_NULL_HANDLE;
		VkShaderModule			mTextureShader = VK_NULL_HANDLE;

		// Pipeline state
		VkDescriptorSetLayout	mDescriptorLayout = VK_NULL_HANDLE;
		VkPipelineLayout		mPipelineLayout = VK_NULL_HANDLE;
		std::unordered_map<uint64_t, PipelineSet> mPipelines;
		PipelineSet*			mActivePipelines = nullptr;

		// Textures
		VkSampler				mSampler = VK_NULL_HANDLE;
		VkDescriptorPool		mDescriptorPool = VK_NULL_HANDLE;
		std::unordered_map<Rml::TextureHandle, std::unique_ptr<RmlTexture>> mTextures;
		Rml::TextureHandle		mNextTextureHandle = 1;

		// Upload support for textures
		VkCommandPool			mUploadPool = VK_NULL_HANDLE;
		VkCommandBuffer			mUploadBuffer = VK_NULL_HANDLE;

		// Current draw state
		int						mWidth = 0;			///< Render target size in pixels
		int						mHeight = 0;
		int						mPixelWidth = 0;	///< Render target size in pixels (same as mWidth)
		int						mPixelHeight = 0;
		bool					mScissorEnabled = false;
		Rml::Matrix4f			mTransform;
		bool					mTransformActive = false;
		bool					mInitialized = false;
	};
}
