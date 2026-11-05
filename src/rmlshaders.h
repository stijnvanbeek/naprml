/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

namespace nap
{
	/**
	 * Shaders used by the RmlUi render interface, compiled to SPIR-V at runtime
	 * through the same glslang runtime that naprender uses.
	 *
	 * These mirror the shader set of RmlUi's Vulkan backend
	 * (thirdparty/RmlUi/Backends/RmlUi_Vulkan) but use push constants instead of
	 * uniform buffers: a single vertex-stage push constant block carries the
	 * combined projection/transform matrix and the per-draw translation.
	 *
	 * RmlUi provides vertex colors in premultiplied alpha form, therefore both
	 * pipelines blend with src = 1, dst = 1 - src alpha.
	 */

	// Vertex shader, shared by the color and textured pipelines.
	// mTransform combines the orthographic projection and the RmlUi transform,
	// uploaded column-major. mTranslate.xy is the per-draw translation.
	static const char* rmlVertexShader = R"(
#version 450

layout(push_constant) uniform PushConstants
{
	mat4 mTransform;
	vec4 mTranslate;
} rml;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor0;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

void main()
{
	vec2 translatedPos = inPosition + rml.mTranslate.xy;
	vTexCoord = inTexCoord0;
	vColor = inColor0;
	gl_Position = rml.mTransform * vec4(translatedPos, 0.0, 1.0);
}
)";

	// Fragment shader for untextured geometry
	static const char* rmlFragmentColorShader = R"(
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vColor;
}
)";

	// Fragment shader for textured geometry; the texture is bound at set 0
	static const char* rmlFragmentTextureShader = R"(
#version 450

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vColor * texture(uTexture, vTexCoord);
}
)";
}
