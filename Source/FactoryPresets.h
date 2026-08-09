// ==========================================
// FactoryPresets.h
// コード埋め込みのファクトリープリセット（10 カテゴリ × 20 = 200 種）
//
// 記法: params = "id=value;id=value;..."
//   ・id は APVTS のパラメータ ID そのまま
//   ・Choice / Bool はインデックス（0 始まり）／0-1 で書く
//   ・Float は実値（Hz、%、dB など、パラメータのレンジと同じ単位）
//   ・書かれていないパラメータは apply() が既定値へ戻す
//     （プリセット間で設定が残らないようにするため）
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class QuadMorphFilterAudioProcessor;

namespace FactoryPresets
{
    struct Item
    {
        const char* category;   // サブカテゴリとして表示する
        const char* name;
        const char* params;
    };

    const std::vector<Item>& items();
    int count();

    juce::String nameOf(int index);
    juce::String categoryOf(int index);

    /** 全パラメータを既定値へ戻してから、指定プリセットを適用する。
        メッセージスレッド専用（setValueNotifyingHost を使うため）。 */
    void apply(QuadMorphFilterAudioProcessor& proc, int index);

    /** 全パラメータを既定値へ戻す（Reset ボタン / Init 用）。 */
    void applyInit(QuadMorphFilterAudioProcessor& proc);
}
