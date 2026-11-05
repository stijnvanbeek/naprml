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
`source/` in this directory (upstream, with `.git` and `.github` removed).

## Rebuilding the prebuilt libraries

RmlUi is built with the freetype font engine linked against the vendored
freetype from system_modules/napfont, so the packaged apps stay
self-contained: librmlui.dylib depends on @rpath/libfreetype.6.16.0.dylib,
not on a system or Homebrew freetype. The FREETYPE_* variables pin
FindFreetype to that library:

    FT=../../../../../system_modules/napfont/thirdparty/freetype/macos/universal

Static:

    cmake -S source -B <builddir> -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_BUILD_TYPE=Release -DRMLUI_SAMPLES=OFF \
          -DFREETYPE_INCLUDE_DIR_freetype2=$FT/include/freetype2 \
          -DFREETYPE_INCLUDE_DIR_ft2build=$FT/include/freetype2 \
          -DFREETYPE_LIBRARY_RELEASE=$FT/lib/libfreetype.dylib
    cmake --build <builddir> --target rmlui_core
    cp <builddir>/librmlui.a macos/universal/bin/

Shared:

    cmake -S source -B <builddir> -DBUILD_SHARED_LIBS=ON \
          -DCMAKE_BUILD_TYPE=Release -DRMLUI_SAMPLES=OFF \
          -DFREETYPE_INCLUDE_DIR_freetype2=$FT/include/freetype2 \
          -DFREETYPE_INCLUDE_DIR_ft2build=$FT/include/freetype2 \
          -DFREETYPE_LIBRARY_RELEASE=$FT/lib/libfreetype.dylib
    cmake --build <builddir> --target rmlui_core
    cp <builddir>/librmlui.dylib macos/universal/lib/

Check the result before committing: `otool -L macos/universal/lib/librmlui.dylib`
must list @rpath/libfreetype.6.16.0.dylib, never /opt/homebrew or /usr/local.
