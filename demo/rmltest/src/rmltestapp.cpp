/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "rmltestapp.h"

// External includes
#include <nap/core.h>
#include <nap/logger.h>

// Register this application with RTTI, this is required by the AppRunner to
// validate that this object is indeed an application
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RmlTestApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap
{
	/**
	 * Initialize all the resources and store the objects we need later on
	 */
	bool RmlTestApp::init(utility::ErrorState& error)
	{
		// Retrieve services
		mRenderService = getCore().getService<nap::RenderService>();
		mRmlService = getCore().getService<nap::RmlService>();
		mInputService = getCore().getService<nap::InputService>();

		if (!error.check(mRmlService != nullptr, "RmlService not found, is mod_naprml in app.json?"))
			return false;

		// Get resource manager and the main window
		mResourceManager = getCore().getResourceManager();
		mRenderWindow = mResourceManager->findObject<nap::RenderWindow>("Window0");
		if (!error.check(mRenderWindow != nullptr, "Window0 not found"))
			return false;

		// Create the Rml context for the window and load the example document
		mDocument = mRmlService->loadDocument(*mRenderWindow, "example");
		if (!error.check(mDocument != nullptr, "Failed to load Rml document 'example'"))
			return false;

		nap::Logger::info("Rml document 'example' loaded, %s", mDocument->GetTitle().c_str());

		// Listen for clicks on the demo button and key presses on the document,
		// so input forwarding from nap to Rml can be verified in the log
		mDocument->AddEventListener("click", this);
		mDocument->AddEventListener("keydown", this);
		return true;
	}


	/**
	 * Nothing to do yet, input forwarding to Rml is a TODO in the module
	 */
	void RmlTestApp::update(double deltaTime)
	{
	}


	/**
	 * Renders the document through the module's render interface
	 */
	void RmlTestApp::render()
	{
		// Signal the beginning of a new frame, allowing it to be recorded
		mRenderService->beginFrame();

		// Begin recording the render commands for the main render window
		if (mRenderService->beginRecording(*mRenderWindow))
		{
			// Begin the render pass
			mRenderWindow->beginRendering();

			// Draw the Rml documents of this window
			mRmlService->draw(*mRenderWindow);

			// End the render pass
			mRenderWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}

		// Signal the ending of the frame
		mRenderService->endFrame();
	}


	/**
	 * Forwards window events to the render service
	 */
	void RmlTestApp::windowMessageReceived(WindowEventPtr windowEvent)
	{
		mRmlService->processWindowEvent(*windowEvent);
		mRenderService->addEvent(std::move(windowEvent));
	}


	/**
	 * Forwards input events to the input service, escape quits the app
	 */
	void RmlTestApp::inputMessageReceived(InputEventPtr inputEvent)
	{
		if (inputEvent->get_type().is_derived_from(RTTI_OF(nap::KeyPressEvent)))
		{
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();
		}
		// Rml consumes the events it needs from the window's context first,
		// then the event still reaches the input service for the rest of the app
		mRmlService->processInputEvent(*inputEvent);
		mInputService->addEvent(std::move(inputEvent));
	}


	/**
	 * Logs the Rml events the demo listens to, so input forwarding
	 * from nap to Rml is verifiable: clicks and key presses
	 */
	void RmlTestApp::ProcessEvent(Rml::Event& event)
	{
		if (event.GetId() == Rml::EventId::Keydown)
		{
			const int key = event.GetParameter("key_identifier", -1);
			const int modifiers = event.GetParameter("key_modifier_state", 0);
			nap::Logger::info("Rml keydown: key=%d modifiers=0x%x", key, modifiers);
		}
		else
		{
			Rml::Element* target = event.GetTargetElement();
			nap::Logger::info("Rml %s on '%s'", event.GetType().c_str(),
				target != nullptr ? target->GetId().c_str() : "<none>");
		}
	}


	/**
	 * Called when loop finishes
	 */
	int RmlTestApp::shutdown()
	{
		return 0;
	}
}
