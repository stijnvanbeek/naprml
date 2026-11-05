include(${NAP_ROOT}/cmake/nap_module.cmake)

include(${NAP_ROOT}/cmake/nap_utilities.cmake)

# RmlUi (https://github.com/mikke89/RmlUi, MIT), expected in thirdparty/RmlUi:
#   git clone https://github.com/mikke89/RmlUi thirdparty/RmlUi
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/RmlUi/CMakeLists.txt)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/RmlUi)
    target_link_libraries(${PROJECT_NAME} rmlui_core)
    add_license(rmlui ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/RmlUi/LICENSE.txt)
else()
    message(WARNING "naprml: thirdparty/RmlUi not found, clone it first:")
    message(WARNING "  git clone https://github.com/mikke89/RmlUi thirdparty/RmlUi")
endif()
