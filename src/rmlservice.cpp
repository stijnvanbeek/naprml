/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "rmlservice.h"

// NAP includes
#include <renderservice.h>
#include <nap/core.h>
#include <nap/logger.h>
#include <cmath>
#include <keyboard.h>
#include <windowevent.h>
#include <RmlUi/Core.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RmlService, "Manages the global RmlUi state")
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	/**
	 * Returns the size of the window's render target, the swapchain. On most
	 * platforms the swapchain matches the window's drawable size, but on
	 * macOS the presentation surface reports its size in screen coordinates,
	 * which makes it smaller than the drawable size SDL reports.
	 */
	static glm::ivec2 getTargetSize(const RenderWindow& window)
	{
		// The drawable size is the size of the render target the swapchain converges to.
		glm::ivec2 size = window.getBufferSize();
		if (size.x <= 0 || size.y <= 0)
			size = window.getSwapchainSize();
		return size;
	}

	/**
	 * Returns the factor window screen coordinates are multiplied by to
	 * obtain render target pixels. RCSS 'dp' units follow the same factor,
	 * so that one 'dp' corresponds to one screen coordinate on platforms
	 * where the target is sized in screen coordinates (macOS), and to one
	 * logical pixel elsewhere.
	 */
	static float getTargetScale(const RenderWindow& window)
	{
		const float width = float(window.getWidth());
		return width > 0.0f ? float(getTargetSize(window).x) / width : 1.0f;
	}

	/**
	 * Translates a nap key code into the RmlUi key identifier used by
	 * Rml::Context::ProcessKeyDown() and ProcessKeyUp().
	 */
	static Rml::Input::KeyIdentifier toRmlKeyCode(EKeyCode key)
	{
		using K = Rml::Input::KeyIdentifier;
		switch (key)
		{
			case EKeyCode::KEY_RETURN:			return K::KI_RETURN;
			case EKeyCode::KEY_ESCAPE:			return K::KI_ESCAPE;
			case EKeyCode::KEY_BACKSPACE:		return K::KI_BACK;
			case EKeyCode::KEY_TAB:				return K::KI_TAB;
			case EKeyCode::KEY_SPACE:			return K::KI_SPACE;
			case EKeyCode::KEY_SEMICOLON:		return K::KI_OEM_1;
			case EKeyCode::KEY_COMMA:			return K::KI_OEM_COMMA;
			case EKeyCode::KEY_MINUS:			return K::KI_OEM_MINUS;
			case EKeyCode::KEY_PERIOD:			return K::KI_OEM_PERIOD;
			case EKeyCode::KEY_SLASH:			return K::KI_OEM_2;
			case EKeyCode::KEY_QUOTE:			return K::KI_OEM_7;
			case EKeyCode::KEY_0:				return K::KI_0;
			case EKeyCode::KEY_1:				return K::KI_1;
			case EKeyCode::KEY_2:				return K::KI_2;
			case EKeyCode::KEY_3:				return K::KI_3;
			case EKeyCode::KEY_4:				return K::KI_4;
			case EKeyCode::KEY_5:				return K::KI_5;
			case EKeyCode::KEY_6:				return K::KI_6;
			case EKeyCode::KEY_7:				return K::KI_7;
			case EKeyCode::KEY_8:				return K::KI_8;
			case EKeyCode::KEY_9:				return K::KI_9;
			case EKeyCode::KEY_COLON:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_LESS:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_EQUALS:			return K::KI_OEM_PLUS;
			case EKeyCode::KEY_GREATER:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_QUESTION:		return K::KI_UNKNOWN;
			case EKeyCode::KEY_AT:				return K::KI_UNKNOWN;
			case EKeyCode::KEY_LEFTBRACKET:		return K::KI_OEM_4;
			case EKeyCode::KEY_BACKSLASH:		return K::KI_OEM_5;
			case EKeyCode::KEY_RIGHTBRACKET:	return K::KI_OEM_6;
			case EKeyCode::KEY_CARET:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_UNDERSCORE:		return K::KI_UNKNOWN;
			case EKeyCode::KEY_BACKQUOTE:		return K::KI_OEM_3;
			case EKeyCode::KEY_a:				return K::KI_A;
			case EKeyCode::KEY_b:				return K::KI_B;
			case EKeyCode::KEY_c:				return K::KI_C;
			case EKeyCode::KEY_d:				return K::KI_D;
			case EKeyCode::KEY_e:				return K::KI_E;
			case EKeyCode::KEY_f:				return K::KI_F;
			case EKeyCode::KEY_g:				return K::KI_G;
			case EKeyCode::KEY_h:				return K::KI_H;
			case EKeyCode::KEY_i:				return K::KI_I;
			case EKeyCode::KEY_j:				return K::KI_J;
			case EKeyCode::KEY_k:				return K::KI_K;
			case EKeyCode::KEY_l:				return K::KI_L;
			case EKeyCode::KEY_m:				return K::KI_M;
			case EKeyCode::KEY_n:				return K::KI_N;
			case EKeyCode::KEY_o:				return K::KI_O;
			case EKeyCode::KEY_p:				return K::KI_P;
			case EKeyCode::KEY_q:				return K::KI_Q;
			case EKeyCode::KEY_r:				return K::KI_R;
			case EKeyCode::KEY_s:				return K::KI_S;
			case EKeyCode::KEY_t:				return K::KI_T;
			case EKeyCode::KEY_u:				return K::KI_U;
			case EKeyCode::KEY_v:				return K::KI_V;
			case EKeyCode::KEY_w:				return K::KI_W;
			case EKeyCode::KEY_x:				return K::KI_X;
			case EKeyCode::KEY_y:				return K::KI_Y;
			case EKeyCode::KEY_z:				return K::KI_Z;
			case EKeyCode::KEY_CAPSLOCK:		return K::KI_CAPITAL;
			case EKeyCode::KEY_F1:				return K::KI_F1;
			case EKeyCode::KEY_F2:				return K::KI_F2;
			case EKeyCode::KEY_F3:				return K::KI_F3;
			case EKeyCode::KEY_F4:				return K::KI_F4;
			case EKeyCode::KEY_F5:				return K::KI_F5;
			case EKeyCode::KEY_F6:				return K::KI_F6;
			case EKeyCode::KEY_F7:				return K::KI_F7;
			case EKeyCode::KEY_F8:				return K::KI_F8;
			case EKeyCode::KEY_F9:				return K::KI_F9;
			case EKeyCode::KEY_F10:				return K::KI_F10;
			case EKeyCode::KEY_F11:				return K::KI_F11;
			case EKeyCode::KEY_F12:				return K::KI_F12;
			case EKeyCode::KEY_F13:				return K::KI_F13;
			case EKeyCode::KEY_F14:				return K::KI_F14;
			case EKeyCode::KEY_F15:				return K::KI_F15;
			case EKeyCode::KEY_F16:				return K::KI_F16;
			case EKeyCode::KEY_F17:				return K::KI_F17;
			case EKeyCode::KEY_F18:				return K::KI_F18;
			case EKeyCode::KEY_F19:				return K::KI_F19;
			case EKeyCode::KEY_F20:				return K::KI_F20;
			case EKeyCode::KEY_F21:				return K::KI_F21;
			case EKeyCode::KEY_F22:				return K::KI_F22;
			case EKeyCode::KEY_F23:				return K::KI_F23;
			case EKeyCode::KEY_F24:				return K::KI_F24;
			case EKeyCode::KEY_PRINTSCREEN:		return K::KI_SNAPSHOT;
			case EKeyCode::KEY_SCROLLLOCK:		return K::KI_SCROLL;
			case EKeyCode::KEY_PAUSE:			return K::KI_PAUSE;
			case EKeyCode::KEY_INSERT:			return K::KI_INSERT;
			case EKeyCode::KEY_HOME:			return K::KI_HOME;
			case EKeyCode::KEY_PAGEUP:			return K::KI_PRIOR;
			case EKeyCode::KEY_DELETE:			return K::KI_DELETE;
			case EKeyCode::KEY_END:				return K::KI_END;
			case EKeyCode::KEY_PAGEDOWN:		return K::KI_NEXT;
			case EKeyCode::KEY_RIGHT:			return K::KI_RIGHT;
			case EKeyCode::KEY_LEFT:			return K::KI_LEFT;
			case EKeyCode::KEY_DOWN:			return K::KI_DOWN;
			case EKeyCode::KEY_UP:				return K::KI_UP;
			case EKeyCode::KEY_NUMLOCKCLEAR:	return K::KI_NUMLOCK;
			case EKeyCode::KEY_KP_DIVIDE:		return K::KI_DIVIDE;
			case EKeyCode::KEY_KP_MULTIPLY:		return K::KI_MULTIPLY;
			case EKeyCode::KEY_KP_MINUS:		return K::KI_SUBTRACT;
			case EKeyCode::KEY_KP_PLUS:			return K::KI_ADD;
			case EKeyCode::KEY_KP_ENTER:		return K::KI_NUMPADENTER;
			case EKeyCode::KEY_KP_1:			return K::KI_NUMPAD1;
			case EKeyCode::KEY_KP_2:			return K::KI_NUMPAD2;
			case EKeyCode::KEY_KP_3:			return K::KI_NUMPAD3;
			case EKeyCode::KEY_KP_4:			return K::KI_NUMPAD4;
			case EKeyCode::KEY_KP_5:			return K::KI_NUMPAD5;
			case EKeyCode::KEY_KP_6:			return K::KI_NUMPAD6;
			case EKeyCode::KEY_KP_7:			return K::KI_NUMPAD7;
			case EKeyCode::KEY_KP_8:			return K::KI_NUMPAD8;
			case EKeyCode::KEY_KP_9:			return K::KI_NUMPAD9;
			case EKeyCode::KEY_KP_0:			return K::KI_NUMPAD0;
			case EKeyCode::KEY_KP_PERIOD:		return K::KI_DECIMAL;
			case EKeyCode::KEY_APPLICATION:		return K::KI_APPS;
			case EKeyCode::KEY_EXECUTE:			return K::KI_EXECUTE;
			case EKeyCode::KEY_HELP:			return K::KI_HELP;
			case EKeyCode::KEY_MENU:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_SELECT:			return K::KI_SELECT;
			case EKeyCode::KEY_CANCEL:			return K::KI_UNKNOWN;
			case EKeyCode::KEY_CLEAR:			return K::KI_CLEAR;
			case EKeyCode::KEY_LCTRL:			return K::KI_LCONTROL;
			case EKeyCode::KEY_RCTRL:			return K::KI_RCONTROL;
			case EKeyCode::KEY_LSHIFT:			return K::KI_LSHIFT;
			case EKeyCode::KEY_RSHIFT:			return K::KI_RSHIFT;
			case EKeyCode::KEY_LALT:			return K::KI_LMENU;
			case EKeyCode::KEY_RALT:			return K::KI_RMENU;
			case EKeyCode::KEY_LGUI:			return K::KI_LMETA;
			case EKeyCode::KEY_RGUI:			return K::KI_RMETA;
			default:							return K::KI_UNKNOWN;
		}
	}

	/**
	 * Translates the nap key modifier bit-mask into the RmlUi modifier
	 * bit-mask used by Rml::Context::ProcessKeyDown() etc.
	 */
	static int toRmlKeyModifier(KeyModifier modifier)
	{
		using M = Rml::Input::KeyModifier;
		int result = 0;
		if (modifier & static_cast<KeyModifier>(EKeyModifier::Shift))
			result |= M::KM_SHIFT;
		if (modifier & static_cast<KeyModifier>(EKeyModifier::Control))
			result |= M::KM_CTRL;
		if (modifier & static_cast<KeyModifier>(EKeyModifier::Alt))
			result |= M::KM_ALT;
		if (modifier & static_cast<KeyModifier>(EKeyModifier::Gui))
			result |= M::KM_META;
		return result;
	}

	/**
	 * Translates a nap mouse button into the RmlUi mouse button index used by
	 * Rml::Context::ProcessMouseButtonDown() and ProcessMouseButtonUp().
	 * RmlUi uses 0 = left, 1 = right, 2 = middle, equal to nap.
	 */
	static int toRmlMouseButton(PointerClickEvent::EButton button)
	{
		return static_cast<int>(button);
	}

	RmlService::RmlService(ServiceConfiguration* configuration) : Service(configuration)
	{
	}

	bool RmlService::init(utility::ErrorState& errorState)
	{
		mRenderService = getCore().getService<RenderService>();
		if (!errorState.check(mRenderService != nullptr, "RmlService requires the RenderService"))
			return false;

		mSystemInterface = std::make_unique<RmlSystemInterface>();
		Rml::SetSystemInterface(mSystemInterface.get());

		mRenderInterface = std::make_unique<RmlRenderInterface>(*mRenderService);
		Rml::SetRenderInterface(mRenderInterface.get());

		mFileInterface = std::make_unique<RmlFileInterface>();
		Rml::SetFileInterface(mFileInterface.get());

		Rml::Initialise();

		// Compile shaders and create Vulkan state after RmlUi is up
		if (!mRenderInterface->init(errorState))
			return false;
		return true;
	}

	void RmlService::update(double deltaTime)
	{
		// Update all contexts once per frame, before the render pass
		for (auto& entry : mContexts)
			entry.second->Update();
	}

	void RmlService::shutdown()
	{
		mContexts.clear();
		Rml::Shutdown();

		// Destroys the Vulkan state; geometry and textures are released by
		// Rml::Shutdown() through the render interface
		if (mRenderInterface != nullptr)
			mRenderInterface->shutdown();
	}

	Rml::Context* RmlService::createContext(RenderWindow& window)
	{
		auto found = mContexts.find(&window);
		if (found != mContexts.end())
			return found->second;

		std::string contextName = window.getTitle();

		// The context works in render target pixels, so it is sized by the
		// window's swapchain; the density-independent pixel ratio lets RCSS
		// use 'dp' units that follow the platform's screen coordinates
		glm::ivec2 size = getTargetSize(window);
		Rml::Context* context = Rml::CreateContext(contextName, Rml::Vector2i(size.x, size.y));
		if (context == nullptr)
		{
			nap::Logger::error("Failed to create Rml context for window %s", contextName.c_str());
			return nullptr;
		}
		context->SetDensityIndependentPixelRatio(getTargetScale(window));
		mContexts.emplace(&window, context);
		return context;
	}

	Rml::ElementDocument* RmlService::loadDocument(RenderWindow& window, const std::string& documentName)
	{
		Rml::Context* context = createContext(window);
		if (context == nullptr)
			return nullptr;

		Rml::ElementDocument* document = context->LoadDocument(documentName + ".rml");
		if (document != nullptr)
			document->Show();
		return document;
	}

	void RmlService::draw(RenderWindow& window)
	{
		auto found = mContexts.find(&window);
		if (found == mContexts.end())
			return;

		// Tell the render interface which window, render pass and size we
		// are drawing into before the context records its geometry
		mRenderInterface->beginDraw(window);
		found->second->Render();
	}

	void RmlService::processInputEvent(const InputEvent& inputEvent)
	{
		// Events that are not tied to a window have no Rml context to go to
		const WindowInputEvent* windowEvent = rtti_cast<const WindowInputEvent>(&inputEvent);
		if (windowEvent == nullptr)
			return;

		RenderWindow* window = mRenderService->findWindow(static_cast<uint>(windowEvent->mWindow));
		if (window == nullptr)
			return;

		auto found = mContexts.find(window);
		if (found == mContexts.end())
			return;
		Rml::Context* context = found->second;

		// Nap pointer coordinates are in window points with their origin in
		// the bottom-left, RmlUi works in device pixels with the origin in
		// the top-left. The scale converts screen coordinates into the pixels
		// of the render target, the same factor RCSS 'dp' units follow.
		const glm::ivec2 targetSize = getTargetSize(*window);
		const float targetScale = getTargetScale(*window);

		// Converts a nap point coordinate into an RmlUi mouse coordinate
		auto toRmlCoordinate = [&](float x, float y) -> Rml::Vector2i
		{
			return Rml::Vector2i(
				int(std::lround(x * targetScale)),
				int(std::lround(static_cast<float>(targetSize.y - 1) - y * targetScale)));
		};

		if (const PointerMoveEvent* moveEvent = rtti_cast<const PointerMoveEvent>(&inputEvent))
		{
			const Rml::Vector2i coord = toRmlCoordinate(moveEvent->mX, moveEvent->mY);
			context->ProcessMouseMove(coord.x, coord.y, toRmlKeyModifier(0));
		}
		else if (const PointerPressEvent* pressEvent = rtti_cast<const PointerPressEvent>(&inputEvent))
		{
			if (pressEvent->mButton == PointerClickEvent::EButton::UNKNOWN)
				return;
			// Make sure the context knows where the pointer is before the
			// press lands, platforms do not guarantee a move event first
			const Rml::Vector2i coord = toRmlCoordinate(pressEvent->mX, pressEvent->mY);
			context->ProcessMouseMove(coord.x, coord.y, toRmlKeyModifier(0));
			context->ProcessMouseButtonDown(toRmlMouseButton(pressEvent->mButton), toRmlKeyModifier(0));
		}
		else if (const PointerReleaseEvent* releaseEvent = rtti_cast<const PointerReleaseEvent>(&inputEvent))
		{
			if (releaseEvent->mButton == PointerClickEvent::EButton::UNKNOWN)
				return;
			context->ProcessMouseButtonUp(toRmlMouseButton(releaseEvent->mButton), toRmlKeyModifier(0));
			const Rml::Vector2i coord = toRmlCoordinate(releaseEvent->mX, releaseEvent->mY);
			context->ProcessMouseMove(coord.x, coord.y, toRmlKeyModifier(0));
		}
		else if (rtti_cast<const WindowResizedEvent>(&inputEvent) != nullptr)
		{
			// Re-query the window instead of trusting the event values: nap
			// maps both point and pixel size changes to this event, in
			// different units. A display scale change also fires it, so the
			// ratio is refreshed alongside the dimensions.
			glm::ivec2 size = getTargetSize(*window);
			context->SetDimensions(Rml::Vector2i(size.x, size.y));
			context->SetDensityIndependentPixelRatio(getTargetScale(*window));
		}
		else if (const MouseWheelEvent* wheelEvent = rtti_cast<const MouseWheelEvent>(&inputEvent))
		{
			// Nap wheel ticks follow the SDL convention, RmlUi's scroll
			// direction is the opposite: a positive vertical value scrolls up
			context->ProcessMouseWheel(Rml::Vector2f(
				static_cast<float>(-wheelEvent->mX),
				static_cast<float>(-wheelEvent->mY)), toRmlKeyModifier(0));
		}
		else if (const KeyPressEvent* keyPressEvent = rtti_cast<const KeyPressEvent>(&inputEvent))
		{
			context->ProcessKeyDown(toRmlKeyCode(keyPressEvent->mKey), toRmlKeyModifier(keyPressEvent->mModifier));
		}
		else if (const KeyReleaseEvent* keyReleaseEvent = rtti_cast<const KeyReleaseEvent>(&inputEvent))
		{
			context->ProcessKeyUp(toRmlKeyCode(keyReleaseEvent->mKey), toRmlKeyModifier(keyReleaseEvent->mModifier));
		}
		else if (const TextInputEvent* textEvent = rtti_cast<const TextInputEvent>(&inputEvent))
		{
			context->ProcessTextInput(textEvent->mText);
		}
	}

	void RmlService::processWindowEvent(const WindowEvent& windowEvent)
	{
		// Only size changes matter to RmlUi, everything else is ignored
		const WindowResizedEvent* resizedEvent = rtti_cast<const WindowResizedEvent>(&windowEvent);
		if (resizedEvent == nullptr)
			return;

		RenderWindow* window = mRenderService->findWindow(static_cast<uint>(windowEvent.mWindow));
		if (window == nullptr)
			return;

		auto found = mContexts.find(window);
		if (found == mContexts.end())
			return;

		// Re-query the window instead of trusting the event values: nap maps
		// both point and pixel size changes to this event, in different
		// units. A display scale change also fires it, so the ratio is
		// refreshed alongside the dimensions.
		Rml::Context* context = found->second;
		glm::ivec2 size = getTargetSize(*window);
		context->SetDimensions(Rml::Vector2i(size.x, size.y));
		context->SetDensityIndependentPixelRatio(getTargetScale(*window));
	}
}
