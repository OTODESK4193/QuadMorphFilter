cmake_minimum_required(VERSION 3.22)
project(QuadMorphFilter VERSION 1.0.0)

# 1. C++20の有効化
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. JUCEディレクトリの指定 (WindowsのパスをCMake用にスラッシュで記述)
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
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE "Qmfl"
    FORMATS VST3 Standalone # 開発・デバッグ用にStandaloneも含めています
    PRODUCT_NAME "Quad-Morph Filter")

# 4. SIMDおよび最高レベルの最適化フラグの設定 (コンパイラ別)
target_compile_options(QuadMorphFilter PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -O3 -mavx2 -mfma -ffast-math
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /O2 /arch:AVX2 /fp:fast
    >
)

# 5. ソースコードの追加 (ステップ2以降で作成するファイル群)
target_sources(QuadMorphFilter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    # Source/TptFilter.cpp  # 今後追加
    # Source/TptFilter.h    # 今後追加
)

# 6. 使用するJUCEモジュールのリンク
target_link_libraries(QuadMorphFilter PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp        # SIMDレジスタやIIRフィルター基盤などに必須
    juce::juce_gui_basics # UI描画用
    juce::juce_gui_extra
    RECOMMENDED_WARNINGS_TARGET
)