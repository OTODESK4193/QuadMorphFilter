cmake_minimum_required(VERSION 3.22)
project(QuadMorphFilter VERSION 1.0.0)

# VST3の互換エラーと過去キャッシュの干渉を徹底排除
set(JUCE_VST3_AUTO_MANIFEST OFF)
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

# 1. CMakeバージョンをアップ（JUCE8.0推奨）
cmake_minimum_required(VERSION 3.24)
project(QuadMorphFilter VERSION 1.0.0)

# 2. C++20の有効化
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. ビルド出力ディレクトリの指定（余計なゴミを避ける）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 4. JUCEディレクトリの指定
set(JUCE_DIR "C:/JUCE")
add_subdirectory(${JUCE_DIR} build_juce)

# 3. プラグインの定義
juce_add_plugin(QuadMorphFilter
    COMPANY_NAME "OTODESK"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE 
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE "Qmfl"
    FORMATS VST3 Standalone
    PRODUCT_NAME "Quad-Morph Filter")

# 4. SIMD最適化フラグと文字コードエラー無視の設定
target_compile_options(QuadMorphFilter PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        $<$<CONFIG:Release>:-O3 -ffast-math> -mavx2 -mfma
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Release>:/O2 /fp:fast> /arch:AVX2 /wd4819 /utf-8
    >
)

target_sources(QuadMorphFilter
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h

        
        
        Source/DSP/TptFilter.cpp
        Source/DSP/TptFilter.h
        Source/DSP/LfoEngine.cpp      # ← 新規追加
        Source/DSP/LfoEngine.h        # ← 新規追加
        Source/DSP/MorphEngine.cpp    # ← 新規追加
        Source/DSP/MorphEngine.h      # ← 新規追加

        Source/UI/QuadMorphLookAndFeel.cpp
        Source/UI/QuadMorphLookAndFeel.h
        Source/UI/FilterVisualizer.cpp
        Source/UI/FilterVisualizer.h
        Source/UI/XYPadComponent.cpp
        Source/UI/XYPadComponent.h
 "Source/DSP/FilterA_SVF_SIMD.h" "Source/DSP/FilterA_SVF_SIMD.cpp" "Source/DSP/TptFilterState.h" "Source/DSP/TptFilter/SVF/TptFilter_SVF.h" "Source/DSP/TptFilter/SVF/TptFilter_SVF.cpp" "Source/DSP/TptFilter/Ladder/TptFilter_Ladder.h" "Source/DSP/TptFilter/Ladder/TptFilter_Ladder.cpp" "Source/DSP/TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.h" "Source/DSP/TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.cpp" "Source/DSP/TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.h" "Source/DSP/TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.cpp" "Source/DSP/TptFilter/Spectral/TptFilter_Spectral.h" "Source/DSP/TptFilter/Spectral/TptFilter_Spectral.cpp" "Source/DSP/TptFilter/Experimental/TptFilter_Experimental.h" "Source/DSP/TptFilter/Experimental/TptFilter_Experimental.cpp")

# 6. 使用するJUCEモジュールのリンク（問題のTARGETを削除しました）
target_link_libraries(QuadMorphFilter PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)