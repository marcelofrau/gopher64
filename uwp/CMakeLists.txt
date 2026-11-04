cmake_minimum_required(VERSION 3.24)
project(Gopher64UWP LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    message(FATAL_ERROR "Gopher64 UWP must be configured for WindowsStore")
endif()

set(GOPHER64_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(UWP_DEPS "${GOPHER64_ROOT}/out/uwp-deps" CACHE PATH "UWP dependency prefix")
set(GOPHER64_PACKAGE_VERSION "0.1.0.0" CACHE STRING "Four-part UWP development package version")
configure_file(Package.appxmanifest.in "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest" @ONLY)

include(FetchContent)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/b916186e66aa3c02b72c6ca17455516512ed8049.tar.gz
    URL_HASH SHA256=a8e9ed7adb544e2d528722e96a229272716fb9e220a4b9e2ceee5469d548f562
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_executable(gopher64-uwp WIN32
    host/main.cpp
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest"
)
set_source_files_properties(host/main.cpp PROPERTIES COMPILE_OPTIONS /ZW)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT gopher64-uwp)
set_target_properties(gopher64-uwp PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17763.0"
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL
)
target_compile_features(gopher64-uwp PRIVATE cxx_std_17)
target_include_directories(gopher64-uwp PRIVATE "${UWP_DEPS}/include" "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(gopher64-uwp PRIVATE
    "${GOPHER64_ROOT}/target/x86_64-uwp-windows-msvc/$<IF:$<CONFIG:Debug>,debug,release>/deps/gopher64.lib"
    "${UWP_DEPS}/lib/SDL3.lib"
    "${UWP_DEPS}/lib/SDL3_ttf.lib"
    ntdll userenv ws2_32 dbghelp windowsapp
)

set(package_assets
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Square44x44Logo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Square150x150Logo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/StoreLogo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SplashScreen.png"
)
target_sources(gopher64-uwp PRIVATE ${package_assets})
set_source_files_properties(${package_assets} PROPERTIES VS_DEPLOYMENT_CONTENT 1 VS_DEPLOYMENT_LOCATION Assets)
set(package_dlls "${UWP_DEPS}/bin/SDL3.dll" "${UWP_DEPS}/bin/SDL3_ttf.dll")
target_sources(gopher64-uwp PRIVATE ${package_dlls})
set_source_files_properties(${package_dlls} PROPERTIES VS_DEPLOYMENT_CONTENT 1)

set(UWP_CERTIFICATE "" CACHE FILEPATH "Optional UWP package signing certificate")
if(NOT UWP_CERTIFICATE)
    set(UWP_CERTIFICATE "${CMAKE_CURRENT_BINARY_DIR}/Gopher64_TemporaryKey.pfx")
    configure_file("${CMAKE_ROOT}/Templates/Windows/Windows_TemporaryKey.pfx" "${UWP_CERTIFICATE}" COPYONLY)
endif()
if(NOT EXISTS "${UWP_CERTIFICATE}")
    message(FATAL_ERROR "UWP signing certificate does not exist: ${UWP_CERTIFICATE}")
endif()
target_sources(gopher64-uwp PRIVATE "${UWP_CERTIFICATE}")
