include(${NAP_ROOT}/cmake/nap_module.cmake)

include(${NAP_ROOT}/cmake/nap_utilities.cmake)

# RmlUi (https://github.com/mikke89/RmlUi, MIT) is linked as a prebuilt
# import library from thirdparty/rmlui, in the same way napfont links
# freetype. The prebuilt libraries are built from the sources in
# thirdparty/source/RmlUi, see thirdparty/rmlui/README.md.
