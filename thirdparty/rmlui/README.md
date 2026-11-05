# rmlui

Prebuilt RmlUi (https://github.com/mikke89/RmlUi, MIT) libraries for
linking into nap through `add_import_library` / `add_static_import_library`,
mirroring the layout of `system_modules/napfont/thirdparty/freetype`:

    {platform}/{arch}/{bin,include,lib}

- `macos/universal`: universal (arm64 + x86_64) binaries for macOS
- `include`: RmlUi headers, included as `<RmlUi/Core/...>`
- `lib`: shared library (`librmlui.dylib`), the import library
- `bin`: static library (`librmlui.a`) for static builds

The sources these libraries are built from live in
`../source/RmlUi` (upstream, with `.git` and `.github` removed).

## Rebuilding the prebuilt libraries

Static:

    cmake -S ../source/RmlUi -B <builddir> -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_BUILD_TYPE=Release -DRMLUI_SAMPLES=OFF
    cmake --build <builddir> --target rmlui_core
    cp <builddir>/librmlui.a macos/universal/bin/

Shared:

    cmake -S ../source/RmlUi -B <builddir> -DBUILD_SHARED_LIBS=ON \
          -DCMAKE_BUILD_TYPE=Release -DRMLUI_SAMPLES=OFF
    cmake --build <builddir> --target rmlui_core
    cp <builddir>/librmlui.dylib macos/universal/lib/
