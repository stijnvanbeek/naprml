/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// External includes
#include <renderwindow.h>
#include <nap/resourcemanager.h>
#include <renderservice.h>
#include <inputservice.h>
#include <RmlUi/Core.h>
#include <app.h>

// Local module includes
#include <rmlservice.h>

namespace nap
{
	/**
	 * Minimal application that proves the naprml module lifecycle:
	 * one window, one Rml context, one loaded document.
	 *
	 * Input events are forwarded to the window's Rml context; a click on
	 * the demo button and key presses show up in the log. Press escape
	 * to quit.
	 */
	class RmlTestApp : public App, public Rml::EventListener
	{
		RTTI_ENABLE(App)
	public:
		RmlTestApp(nap::Core& core) : App(core) { }

		// Initialize app specific data structures
		bool init(utility::ErrorState& error) override;

		// Update is called before render, performs all the app logic
		void update(double deltaTime) override;

		// Render is called after update, pushes all renderable objects to the GPU
		void render() override;

		// Forwards the received window event to the render service
		void windowMessageReceived(WindowEventPtr windowEvent) override;

		// Forwards the received input event to the input service
		void inputMessageReceived(InputEventPtr inputEvent) override;

		// Called for Rml events the document listens to ('click', 'keydown')
		void ProcessEvent(Rml::Event& event) override;

		// Called when loop finishes
		int shutdown() override;

	private:
		RenderService* mRenderService = nullptr;			//< Render service that handles render calls
		RmlService* mRmlService = nullptr;					//< Manages the Rml contexts and documents
		InputService* mInputService = nullptr;				//< Input service for processing input
		ResourceManager* mResourceManager = nullptr;		//< Manages all the loaded resources
		rtti::ObjectPtr<RenderWindow> mRenderWindow;		//< Pointer to the render window
		Rml::ElementDocument* mDocument = nullptr;			//< The loaded Rml document
	};
}
