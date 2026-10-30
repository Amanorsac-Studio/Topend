# Amanorsac Top End — bass amp, cab, pedalboard, looper, and the Learn tab.
# Configure:  cmake -S apps/topend -B apps/topend/build
# Build:      cmake --build apps/topend/build --config Release --parallel
# Test:       ctest --test-dir apps/topend/build -C Release
#
# The UI is resources/ui/index.html hosted in JUCE's WebBrowserComponent
# (WebView2 on Windows, WKWebView on macOS), wired to the engine through
# src/plugin/TopEndEditor.cpp. On Windows the WebView2 SDK NuGet package is
# needed at configure time:
#     nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory <dir>
# and the block below stages it out of the NuGet cache automatically.
#
# Reuses (read-only) the JUCE checkout that lives next to JM Signature. Nothing
# here writes into apps/signature or apps/sanctuary.
cmake_minimum_required(VERSION 3.24)
project(TopEnd VERSION 1.0.0 LANGUAGES C CXX)

# The C++ runtime is linked STATICALLY into our own binaries. A plug-in is
# loaded into somebody else's process on a machine that may never have had the
# Visual C++ redistributable installed; depending on it being there is how a
# plug-in fails to load on exactly the clean machine a buyer owns. (ONNX
# Runtime is a prebuilt DLL and still needs the runtime: the installer places
# those four DLLs beside it - see installer/TopEnd.iss.) Must be set before JUCE.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# ------------------------------------------------------------------ JUCE 8
set(TOPEND_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../signature/external/JUCE" CACHE PATH "JUCE checkout")
if(WIN32)
    file(GLOB _wv2_headers "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/*/build/native/include/WebView2.h")
    if(_wv2_headers)
        list(SORT _wv2_headers)
        list(GET _wv2_headers -1 _wv2_header)
        get_filename_component(_wv2_version_dir "${_wv2_header}/../../../.." ABSOLUTE)
        set(_wv2_staged "${CMAKE_BINARY_DIR}/webview2/Microsoft.Web.WebView2")
        if(NOT EXISTS "${_wv2_staged}/build/native/include/WebView2.h")
            file(COPY "${_wv2_version_dir}/build" DESTINATION "${_wv2_staged}")
        endif()
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/webview2" CACHE PATH "Folder containing Microsoft.Web.WebView2" FORCE)
        message(STATUS "Top End: WebView2 SDK staged from ${_wv2_version_dir}")
    endif()
endif()

if(EXISTS "${TOPEND_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${TOPEND_JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# TEST BUILD. Adds a key check that silences the audio until a valid, unexpired
# test key is entered (src/plugin/TesterGate.h). Off for a release: a release
# contains none of it. It is not the studio's licensing (see TesterGate.h) and
# carries a second signing key, so a build made with this ON must never be sold.
option(TOPEND_TESTER_BUILD "Build a key-gated test build" OFF)
if(TOPEND_TESTER_BUILD)
    message(STATUS "Top End: TEST BUILD - key required, not for release")
endif()

set(TOPEND_AUDIO_DEFS JUCE_WASAPI=1 JUCE_DIRECTSOUND=1)
set(TOPEND_JUCE_ROOT "${JUCE_SOURCE_DIR}")
if(NOT TOPEND_JUCE_ROOT)
    set(TOPEND_JUCE_ROOT "${juce_SOURCE_DIR}")
endif()
if(WIN32 AND EXISTS "${TOPEND_JUCE_ROOT}/modules/juce_audio_devices/native/asio/iasiodrv.h")
    list(APPEND TOPEND_AUDIO_DEFS JUCE_ASIO=1)
    message(STATUS "Top End: ASIO enabled (JUCE-bundled headers)")
endif()

set(TOPEND_WEB_DEFS JUCE_WEB_BROWSER=1 JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

# ------------------------------------------------------------------ Stem separation
# The separation network (HT-Demucs) runs on ONNX Runtime — Microsoft's official
# prebuilt CPU package, MIT licensed, fetched at configure time. Only the header
# and the import library are used here; onnxruntime.dll is copied next to the
# binaries below and ships with the app.
#
# This replaced demucs.cpp (a C++/Eigen port of the same model), which was
# correct but ran ~30x slower than real time under MSVC — see Separator.cpp.
# Consequences of the switch: no Eigen, no AVX2-only translation unit, and no
# CPU feature check, because ONNX Runtime dispatches on the instruction sets it
# finds at run time. The weights are not part of the build either: the
# INSTALLER places them (installer/), and the product only ever reads them.
set(TOPEND_ORT_VERSION 1.22.0)
if(WIN32)
    set(TOPEND_ORT_PKG "onnxruntime-win-x64-${TOPEND_ORT_VERSION}.zip")
elseif(APPLE)
    set(TOPEND_ORT_PKG "onnxruntime-osx-universal2-${TOPEND_ORT_VERSION}.tgz")
else()
    set(TOPEND_ORT_PKG "onnxruntime-linux-x64-${TOPEND_ORT_VERSION}.tgz")
endif()
include(FetchContent)
FetchContent_Declare(onnxruntime
    URL "https://github.com/microsoft/onnxruntime/releases/download/v${TOPEND_ORT_VERSION}/${TOPEND_ORT_PKG}")
FetchContent_MakeAvailable(onnxruntime)

add_library(topend_ort INTERFACE)
target_include_directories(topend_ort INTERFACE "${onnxruntime_SOURCE_DIR}/include")
if(WIN32)
    target_link_libraries(topend_ort INTERFACE "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib")
    set(TOPEND_ORT_RUNTIME "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll")
elseif(APPLE)
    target_link_libraries(topend_ort INTERFACE "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.dylib")
    set(TOPEND_ORT_RUNTIME "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.${TOPEND_ORT_VERSION}.dylib")
else()
    target_link_libraries(topend_ort INTERFACE "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so")
    set(TOPEND_ORT_RUNTIME "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${TOPEND_ORT_VERSION}")
endif()

# ------------------------------------------------------------------ Your Amp (NAM)
# Neural Amp Modeler's core (MIT) plays a player's own .nam amp captures. It and
# its dependencies, Eigen (MPL-2.0, held to its MPL-only code) and nlohmann/json
# (MIT, shipped inside the NAM tree), are pinned by hash and built into one static
# library. Only src/engine/nam/NamBridge.cpp includes their headers.
FetchContent_Declare(nam_core
    URL      "https://github.com/sdatkinson/NeuralAmpModelerCore/archive/0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842.tar.gz"
    URL_HASH SHA256=ca8bd36a66c0861c4359660440210455f436b48ba28529c7134a09a08eaaf616
    SOURCE_SUBDIR "no-cmake")
FetchContent_Declare(nam_eigen
    URL      "https://gitlab.com/libeigen/eigen/-/archive/bc3b39870ecb690a623a3f49149a358b95c5781d/eigen-bc3b39870ecb690a623a3f49149a358b95c5781d.tar.gz"
    URL_HASH SHA256=5f6557d1aca6e959a4b955c55f2935261fd30de59c0c79e35bc6d5cbe1de22d1
    SOURCE_SUBDIR "no-cmake")
# Signalsmith Stretch (MIT) is the pitch shifter behind the Hall's Shimmer; it is
# header-only and uses Signalsmith Linear (MIT) for its FFT. Both pinned by hash.
# Only src/engine/shimmer/PitchShiftBridge.cpp includes their headers.
FetchContent_Declare(ss_stretch
    URL      "https://github.com/Signalsmith-Audio/signalsmith-stretch/archive/a670068d9aeb64913331d5cc29337b19a457a7df.tar.gz"
    URL_HASH SHA256=948b8382bb13f316d13536cdfe06111789a538e8fee7f7a531ee2aec94de1747
    SOURCE_SUBDIR "no-cmake")
FetchContent_Declare(ss_linear
    URL      "https://github.com/Signalsmith-Audio/linear/archive/6d8488e1c239975d240a93dc91824878a0ca088b.tar.gz"
    URL_HASH SHA256=77e1b11f2c80415a920439f490bbefc86ced3eea1066c627a814e094d472cd7b
    SOURCE_SUBDIR "no-cmake")
# Sources only: SOURCE_SUBDIR names a folder with no CMakeLists.txt, so no
# project's own build (tools, tests) is run.
FetchContent_MakeAvailable(nam_core nam_eigen ss_stretch ss_linear)
file(GLOB TOPEND_NAM_SOURCES CONFIGURE_DEPENDS
    "${nam_core_SOURCE_DIR}/NAM/*.cpp"
    "${nam_core_SOURCE_DIR}/NAM/*/*.cpp")
# OBJECT, not STATIC: NAM's architectures register themselves from static
# initialisers, which a static library would drop at link time. The Signalsmith
# bridge rides in the same library: it is the other third-party DSP doorway.
add_library(topend_nam OBJECT ${TOPEND_NAM_SOURCES} src/engine/nam/NamBridge.cpp src/engine/shimmer/PitchShiftBridge.cpp)
target_include_directories(topend_nam PRIVATE
    "${nam_core_SOURCE_DIR}"
    "${nam_core_SOURCE_DIR}/NAM"
    "${nam_core_SOURCE_DIR}/Dependencies/nlohmann"
    "${ss_stretch_SOURCE_DIR}"
    "${ss_linear_SOURCE_DIR}/include")
target_include_directories(topend_nam SYSTEM PRIVATE "${nam_eigen_SOURCE_DIR}")
target_compile_definitions(topend_nam PRIVATE NAM_SAMPLE_FLOAT EIGEN_MPL2_ONLY NAM_ENABLE_A2_FAST
    $<$<PLATFORM_ID:Windows>:NOMINMAX WIN32_LEAN_AND_MEAN>)
target_compile_features(topend_nam PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(topend_nam PRIVATE /bigobj /W0 $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(topend_nam PRIVATE -w $<$<CONFIG:Release>:-O3>)
endif()
set_target_properties(topend_nam PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Every target that separates needs the runtime library with it, and needs to
# find THAT copy rather than any other:
#
#  * Windows: the import is DELAY-LOADED and Separator.cpp loads our DLL by full
#    path first. A plug-in's folder is not on the DLL search path (the host's
#    is), and System32 carries an older onnxruntime.dll of its own that a search
#    by name would find instead.
#  * macOS: the dylib goes into the bundle's Frameworks folder and is found
#    through @rpath. It is signed IN THIS SAME build phase, immediately after
#    being copied in - see the comment on the codesign line below for why.
function(topend_stage_onnxruntime target)
    if(NOT TARGET ${target})
        return()
    endif()
    if(WIN32)
        target_link_options(${target} PRIVATE "/DELAYLOAD:onnxruntime.dll")
        target_link_libraries(${target} PRIVATE delayimp)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TOPEND_ORT_RUNTIME}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    elseif(APPLE)
        get_target_property(_is_bundle ${target} BUNDLE)
        get_target_property(_is_macos_bundle ${target} MACOSX_BUNDLE)
        if(_is_bundle OR _is_macos_bundle)
            set_target_properties(${target} PROPERTIES BUILD_RPATH "@loader_path/../Frameworks"
                                                        INSTALL_RPATH "@loader_path/../Frameworks")
            get_filename_component(_ort_filename "${TOPEND_ORT_RUNTIME}" NAME)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${TOPEND_ORT_RUNTIME}" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks/"
                # Apple rejects an app whose embedded code isn't signed BEFORE the
                # app itself is - "code object is not signed at all, in
                # subcomponent: .../libonnxruntime.dylib" is exactly what Xcode's
                # own automatic CodeSign phase says if this dylib is left as a
                # bare copy. That phase runs as the LAST step for a signed
                # target, after this POST_BUILD command, so signing the dylib
                # right here - in the same phase that copies it in - satisfies
                # Apple's ordering rule. Xcode exports CODE_SIGN_IDENTITY to
                # every Run Script phase, so this only fires on a signed (CI or
                # release) configure; a local unsigned dev build leaves the
                # dylib as a plain copy, exactly as before.
                COMMAND sh -c "if [ -n \"$CODE_SIGN_IDENTITY\" ] && [ \"$CODE_SIGNING_ALLOWED\" != \"NO\" ]; then codesign --force --options runtime --timestamp --sign \"$CODE_SIGN_IDENTITY\" \"$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks/${_ort_filename}\"; fi"
                VERBATIM)
        else()
            set_target_properties(${target} PROPERTIES BUILD_RPATH "${onnxruntime_SOURCE_DIR}/lib")
        endif()
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TOPEND_ORT_RUNTIME}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

# ------------------------------------------------------------------ UI resource
juce_add_binary_data(TopEndUi HEADER_NAME TopEndUi.h NAMESPACE TopEndUi SOURCES
    resources/ui/index.html
    resources/ui/analyzer.js
    resources/ui/stemplayer.js
    # Fonts are shipped inside the product and never fetched (Master Standard §5).
    resources/ui/fonts/Inter.woff2
    resources/ui/fonts/JetBrainsMono.woff2
    resources/ui/fonts/BarlowSemiCondensed-600.woff2
    resources/ui/fonts/BarlowSemiCondensed-700.woff2
    resources/ui/about-lockup.png
    THIRD_PARTY_NOTICES.txt)

# ------------------------------------------------------------------ Plugin
juce_add_plugin(TopEnd
    COMPANY_NAME              "Amanorsac Studio"
    PLUGIN_MANUFACTURER_CODE  Amsc
    PLUGIN_CODE               TpEd
    PRODUCT_NAME              "Top End"
    BUNDLE_ID                 com.amanorsac.topend
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    NEEDS_WEB_BROWSER         TRUE
    NEEDS_WEBVIEW2            TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    ICON_BIG                  "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-1024.png"
    ICON_SMALL                "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-256.png"
    # macOS: the hardened runtime is a notarisation requirement, and an amp that
    # cannot open the audio input is not an amp.
    HARDENED_RUNTIME_ENABLED  TRUE
    HARDENED_RUNTIME_OPTIONS  "com.apple.security.device.audio-input"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Top End needs the audio input to hear your guitar."
    FORMATS                   VST3 AU Standalone
    VST3_CATEGORIES           Fx Distortion)

target_sources(TopEnd PRIVATE
    src/plugin/TopEndProcessor.cpp
    src/plugin/TopEndEditor.cpp
    src/learn/StemSeparator.cpp
    src/learn/Separator.cpp)

target_include_directories(TopEnd PRIVATE src)

target_compile_definitions(TopEnd PUBLIC
    TOPEND_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${TOPEND_TESTER_BUILD}>:TOPEND_TESTER_BUILD=1>
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    ${TOPEND_WEB_DEFS}
    ${TOPEND_AUDIO_DEFS})

if(TARGET TopEnd_Standalone)
    target_sources(TopEnd_Standalone PRIVATE src/plugin/StandaloneApp.cpp)
    target_include_directories(TopEnd_Standalone PRIVATE src
        "${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_TopEndUi/JuceLibraryCode")
endif()

target_link_libraries(TopEnd PRIVATE
    TopEndUi
    topend_ort
    topend_nam
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------ Tests
juce_add_console_app(TopEndTests PRODUCT_NAME "TopEndTests")
target_sources(TopEndTests PRIVATE tests/TopEndTests.cpp)
target_include_directories(TopEndTests PRIVATE src)
target_compile_definitions(TopEndTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TopEndTests PRIVATE
    topend_nam
    juce::juce_audio_formats
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Processor-level soak test: audio runs while presets change, blocks are
# reordered and the looper is driven underneath it. Compiles the processor only
# (the editor is web-hosted and not needed here).
juce_add_console_app(TopEndSoak PRODUCT_NAME "TopEndSoak")
target_sources(TopEndSoak PRIVATE
    tests/SoakTest.cpp
    src/plugin/TopEndProcessor.cpp
    src/learn/StemSeparator.cpp
    src/learn/Separator.cpp)
target_include_directories(TopEndSoak PRIVATE src)
target_compile_definitions(TopEndSoak PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    TOPEND_HEADLESS=1)
target_link_libraries(TopEndSoak PRIVATE
    topend_ort
    topend_nam
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# The test-build gate, checked in the real processor. Always compiled with the gate
# ON (TOPEND_TESTER_BUILD=1) whatever the rest of the build is doing, because what it
# proves - that a locked build is silent - is only true of a build that has one.
juce_add_console_app(TopEndGateTest PRODUCT_NAME "TopEndGateTest")
target_sources(TopEndGateTest PRIVATE
    tests/GateTest.cpp
    src/plugin/TopEndProcessor.cpp
    src/learn/StemSeparator.cpp
    src/learn/Separator.cpp)
target_include_directories(TopEndGateTest PRIVATE src)
target_compile_definitions(TopEndGateTest PRIVATE
    TOPEND_TESTER_BUILD=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    TOPEND_HEADLESS=1)
target_link_libraries(TopEndGateTest PRIVATE
    topend_ort
    topend_nam
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# The signal path as a player hears it: Bypass All, and pedals that fade in
# rather than comb against the clean signal.
juce_add_console_app(TopEndProcTest PRODUCT_NAME "TopEndProcTest")
target_sources(TopEndProcTest PRIVATE
    tests/ProcessorTest.cpp
    src/plugin/TopEndProcessor.cpp
    src/learn/StemSeparator.cpp
    src/learn/Separator.cpp)
target_include_directories(TopEndProcTest PRIVATE src)
target_compile_definitions(TopEndProcTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    TOPEND_HEADLESS=1)
target_link_libraries(TopEndProcTest PRIVATE
    topend_ort
    topend_nam
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
topend_stage_onnxruntime(TopEndProcTest)
add_test(NAME TopEndSignalPath COMMAND TopEndProcTest)

# Stem separation on a real file, timed:
#   TopEndStems <song.wav> <out-dir> [best]
# Uses the weights in Documents/Amanorsac Studio/Top End/Models.
juce_add_console_app(TopEndStems PRODUCT_NAME "TopEndStems")
target_sources(TopEndStems PRIVATE tests/StemTest.cpp src/learn/StemSeparator.cpp src/learn/Separator.cpp)
target_include_directories(TopEndStems PRIVATE src)
target_compile_definitions(TopEndStems PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(TopEndStems PRIVATE
    topend_ort
    topend_nam
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags)

# onnxruntime.dll beside each binary that separates.
topend_stage_onnxruntime(TopEnd_Standalone)
topend_stage_onnxruntime(TopEnd_VST3)
topend_stage_onnxruntime(TopEnd_AU)
topend_stage_onnxruntime(TopEndStems)
topend_stage_onnxruntime(TopEndSoak)
topend_stage_onnxruntime(TopEndGateTest)

enable_testing()
add_test(NAME TopEndUnit COMMAND TopEndTests)
add_test(NAME TopEndTesterGate COMMAND TopEndGateTest)
set_tests_properties(TopEndTesterGate PROPERTIES TIMEOUT 120)
add_test(NAME TopEndSoakShort COMMAND TopEndSoak --seconds 30)
set_tests_properties(TopEndSoakShort PROPERTIES TIMEOUT 600)

# The Learn tab's song analyser is JavaScript (it runs in a Web Worker in the
# UI); its tests run under Node when Node is installed.
find_program(TOPEND_NODE node)
if(TOPEND_NODE)
    add_test(NAME TopEndLearnAnalyzer COMMAND ${TOPEND_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/learn/analyzer.test.js)
    set_tests_properties(TopEndLearnAnalyzer PROPERTIES TIMEOUT 120)
    # The stem player is an AudioWorklet; the test stubs the three globals a
    # worklet is given and drives the real process() in 128-sample blocks.
    add_test(NAME TopEndStemPlayer COMMAND ${TOPEND_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/learn/stemplayer.test.js)
    set_tests_properties(TopEndStemPlayer PROPERTIES TIMEOUT 60)
endif()
