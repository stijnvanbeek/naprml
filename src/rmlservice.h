/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local includes
#include "rmlsysteminterface.h"
#include "rmlrenderinterface.h"
#include "rmlfileinterface.h"

// External includes
#include <nap/service.h>
#include <nap/resourceptr.h>
#include <renderwindow.h>
#include <inputevent.h>
#include <RmlUi/Core.h>
#include <unordered_map>

namespace nap
{
	// Forward declares
	class RenderService;
	class WindowEvent;

	/**
	 * Service that manages the global RmlUi state: the system and render
	 * interfaces and one Rml::Context per window.
	 *
	 * Mirrors the contract of the IMGuiService: call createContext() once per
	 * window, call draw() inside that window's render pass, and load documents
	 * from '.rml' files in the application data directory.
	 *
	 * Forward nap::InputEvent objects of a window to its context by calling
	 * processInputEvent() from the application's inputMessageReceived(), and
	 * nap::WindowEvent objects by calling processWindowEvent() from the
	 * application's windowMessageReceived().
	 */
	class NAPAPI RmlService : public Service
	{
		RTTI_ENABLE(Service)
	public:
		/**
		 * Called by core when the service is constructed
		 */
		RmlService(ServiceConfiguration* configuration);
		/**
		 * Creates and returns the Rml context for the given window, or returns
		 * the existing one when it has been created before.
		 */
		Rml::Context* createContext(RenderWindow& window);

		/**
		 * Loads a document by name (without extension) into the context of the
		 * given window. Returns nullptr on failure.
		 */
		Rml::ElementDocument* loadDocument(RenderWindow& window, const std::string& documentName);

		/**
		 * Draws the documents of the given window to screen. Call inside the
		 * window's render pass, after beginRendering().
		 */
		void draw(RenderWindow& window);

		/**
		 * Forwards a nap input event to the Rml context of the window the
		 * event belongs to. Mouse coordinates are converted from nap's
		 * bottom-left origin into RmlUi's top-left origin; key codes and
		 * modifiers are translated into their RmlUi equivalents. Events for
		 * windows without a context are ignored.
		 */
		void processInputEvent(const InputEvent& inputEvent);

		/**
		 * Forwards window events to the context of the event's window,
		 * keeping the context dimensions and density-independent pixel
		 * ratio in step with the window. Call from the application's
		 * windowMessageReceived().
		 */
		void processWindowEvent(const WindowEvent& windowEvent);

	protected:
		bool init(utility::ErrorState& errorState) override;
		void update(double deltaTime) override;
		void shutdown() override;

	private:
		RenderService* mRenderService = nullptr;
		std::unique_ptr<RmlSystemInterface> mSystemInterface;
		std::unique_ptr<RmlRenderInterface> mRenderInterface;
		std::unique_ptr<RmlFileInterface> mFileInterface;
		std::unordered_map<RenderWindow*, Rml::Context*> mContexts;
	};
}
