cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "C:/Users/RSmith/VST3/VST_SDK/vst3sdk")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(LA2A_Compressor
    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0 
    DESCRIPTION "LA2A_Compressor VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(LA2A_Compressor
    source/version.h
    source/LA2Acids.h
    source/LA2Aprocessor.h
    source/LA2Aprocessor.cpp
    source/LA2Acontroller.h
    source/LA2Acontroller.cpp
    source/LA2Aentry.cpp
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(LA2A_Compressor
        PRIVATE
            resource/LA2Aeditor.uidesc
    )
    target_link_libraries(LA2A_Compressor
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(LA2A_Compressor
        RESOURCES
            "resource/LA2Aeditor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

smtg_target_add_plugin_snapshots (LA2A_Compressor
    RESOURCES
        resource/3E2117D28482586B8A7C0F0AD3C3EDC3_snapshot.png
        resource/3E2117D28482586B8A7C0F0AD3C3EDC3_snapshot_2.0x.png
)

target_link_libraries(LA2A_Compressor
    PRIVATE
        sdk
)

smtg_target_configure_version_file(LA2A_Compressor)

if(SMTG_MAC)
    smtg_target_set_bundle(LA2A_Compressor
        BUNDLE_IDENTIFIER com.RSmith.company
        COMPANY_NAME "My Plug-in Company"
    )
    smtg_target_set_debug_executable(LA2A_Compressor
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(LA2A_Compressor PRIVATE 
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LA2A_Compressor)

        smtg_target_set_debug_executable(LA2A_Compressor
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
