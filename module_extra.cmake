
# naprml compiles its RmlUi shaders to SPIR-V at runtime through the same
# glslang libraries that naprender uses; link those in as well
list(APPEND CMAKE_MODULE_PATH ${NAP_ROOT}/system_modules/naprender/thirdparty/cmake_find_modules)
find_package(glslang REQUIRED)

target_link_libraries(${PROJECT_NAME} debug "${GLSLANG_LIBS_DEBUG}" optimized "${GLSLANG_LIBS_RELEASE}")
target_include_directories(${PROJECT_NAME} PUBLIC ${GLSLANG_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb)

target_link_libraries(${static_target} INTERFACE debug "${GLSLANG_LIBS_DEBUG}" optimized "${GLSLANG_LIBS_RELEASE}")
target_include_directories(${static_target} INTERFACE ${GLSLANG_INCLUDE_DIR})

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} atomic)
    target_link_libraries(${static_target} INTERFACE atomic)
endif()

add_license(glslang ${NAP_ROOT}/system_modules/naprender/thirdparty/glslang/source/LICENSE.txt)
add_license(RmlUi ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/RmlUi/LICENSE.txt)
