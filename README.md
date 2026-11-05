# naprml

RmlUi (https://github.com/mikke89/RmlUi, MIT) as a GUI framework next to ImGui,
as a nap module. RmlUi is a retained, HTML/CSS-inspired UI library that draws
through the module's render interface instead of owning the window or event
loop, so it integrates into nap the same way the IMGuiService does: one
`Rml::Context` per `RenderWindow`, `draw()` called inside the window's render
pass.

## Build

RmlUi is expected in `thirdparty/RmlUi`:

	git clone https://github.com/mikke89/RmlUi thirdparty/RmlUi

The CMakeLists builds it with samples disabled and links the `RmlCore` target.

## Usage

In an application that depends on this module (see module.json):

	RmlService& rmlService = getCore().getService<RmlService>();
	Rml::ElementDocument* document = rmlService.loadDocument(*mRenderWindow, "example");

and inside the window's render pass, after `beginRendering()`:

	rmlService.draw(*mRenderWindow);

Document and resource paths are resolved relative to the application data
directory.

## Status / TODO

- [x] Module skeleton: service, system interface, file interface
- [ ] Render interface: translate RmlUi geometry into nap RenderService draws
      (see rmlrenderinterface.cpp; reference: Backends/RmlUi_Vulkan in RmlUi)
- [ ] Input forwarding: map nap::InputEvent to Rml::Input (mouse, keyboard, text)
- [ ] HiDPI: set the context density per window
- [ ] CMake: verify on macOS, Windows and Linux, and check the Pi (GLES) case
