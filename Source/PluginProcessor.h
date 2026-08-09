// ==========================================
// PluginProcessor.h
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>     // FastMathApproximations 等のために必須
#include <juce_events/juce_events.h> // juce::Timer（レイテンシ通知用）を明示的に取り込む
#include "DSP/TptFilter.h"
#include "DSP/LfoEngine.h"
#include "DSP/Lfo5Engine.h"
#include "DSP/MorphEngine.h"
#include <vector>
#include <array>
#include <atomic>

// juce::Timer は「OS 切替によるレイテンシ変化をホストへ通知する」ためだけに使う。
// setLatencySamples() は updateHostDisplay() を伴いオーディオスレッドから
// 呼べないため、processBlock 側はアトミックに値を置くだけにして、
// 実際の通知をメッセージスレッド（タイマー）から行う。
class QuadMorphFilterAudioProcessor : public juce::AudioProcessor,
                                      private juce::Timer
{
public:
    QuadMorphFilterAudioProcessor();
    ~QuadMorphFilterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::Point<float>   getLfoPos(int index)  const { return lfoEngine.getPosition(index); }
    std::array<float, 4> getLfoMod4(int index) const { return lfoEngine.getMod4(index); }

    // ===== Recording データセッター =====
    void setLfoRecordingData(int index, const std::array<juce::Point<float>, 2048>& buffer, int len)
    {
        lfoEngine.setRecordingData(index, buffer, len);
    }

    // ===== Recording 完了後フェーズリセット =====
    void resetLfoPhase(int index)
    {
        lfoEngine.resetPhase(index);
    }

    juce::AudioProcessorValueTreeState apvts;

    // ===== 【V1.1.0 追加】Solo =====
    // -1 = Solo なし / 0..3 = そのフィルターだけを鳴らす（排他）。
    // 一時的なモニタリング機能なので APVTS には入れない。
    //   ・保存/復元されない（プロジェクトを開き直すと必ず解除される）
    //   ・オートメーション対象にならない
    //   ・パラメータ数が増えないので既存プリセットとの互換も保たれる
    // UI スレッドが書き、オーディオスレッドが読むだけなので atomic で足りる。
    std::atomic<int> soloFilter { -1 };

    std::array<juce::Point<float>, 2048> recBuffer[3];
    std::atomic<int>   recLength[3]{ 0 };
    std::atomic<bool>  isWaitingForRecord[3]{ false };
    std::atomic<bool>  isRecordingDrag[3]{ false };
    std::atomic<float> currentRecX[3]{ 0.5f };
    std::atomic<float> currentRecY[3]{ 0.5f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // =====================================================================
    // 【V1.1.0 追加】パラメータポインタキャッシュ
    //
    // 旧実装は processBlock() 内で apvts.getRawParameterValue("cutoff" + s)
    // のように juce::String を連結して ID を組み立てていた。
    // juce::String は SSO を持たず常にヒープ確保するため、この連結だけで
    // 1 ブロック当たり 28 回の malloc がオーディオスレッド上で発生していた
    // （CLAUDE.md §1「動的メモリ確保の禁止」「ブロッキング・ロックの禁止」違反）。
    // さらに getRawParameterValue() 自体が文字列キー検索であり、
    // サンプルループ内から呼ぶと 48kHz で毎秒 19 万回の検索になっていた。
    //
    // 対策: 生ポインタ（std::atomic<float>*）をコンストラクタで一度だけ解決し
    // キャッシュする。APVTS が保持する atomic の寿命はプロセッサと同じなので
    // ポインタは無効化されない。processBlock() 側は load() するだけになる。
    // =====================================================================
    struct FilterParamPtrs
    {
        std::atomic<float>* cutoff    = nullptr;
        std::atomic<float>* res       = nullptr;
        std::atomic<float>* model     = nullptr;
        std::atomic<float>* type      = nullptr;
        std::atomic<float>* slope     = nullptr;
        std::atomic<float>* enable    = nullptr;
        std::atomic<float>* lfoCutSrc = nullptr;
        std::atomic<float>* lfoResSrc = nullptr;
    };

    struct GlobalParamPtrs
    {
        std::atomic<float>* posX             = nullptr;
        std::atomic<float>* posY             = nullptr;
        std::atomic<float>* morphBlend       = nullptr;
        std::atomic<float>* cutoffAlgo       = nullptr;
        std::atomic<float>* xyDepth          = nullptr;
        std::atomic<float>* osMode           = nullptr;
        std::atomic<float>* dryWet           = nullptr;
        std::atomic<float>* masterGain       = nullptr;
        std::atomic<float>* limiterCeiling   = nullptr;
        std::atomic<float>* lfo1en           = nullptr;
        std::atomic<float>* lfo1wave         = nullptr;
        std::atomic<float>* lfo2en           = nullptr;
        std::atomic<float>* lfo2wave         = nullptr;
        std::atomic<float>* lfo3en           = nullptr;
        std::atomic<float>* lfo3wave         = nullptr;
        std::atomic<float>* lfo5en           = nullptr;
        std::atomic<float>* envFollowEn      = nullptr;
        std::atomic<float>* envFollowAttack  = nullptr;
        std::atomic<float>* envFollowRelease = nullptr;
        std::atomic<float>* envFollowDepth   = nullptr;
        std::atomic<float>* envFollowInvert  = nullptr;
    };

    void cacheParameterPointers();

    std::array<FilterParamPtrs, 4> fp;                  // A / B / C / D
    GlobalParamPtrs                gp;
    juce::RangedAudioParameter*    cutoffAParam = nullptr;  // 正規化空間アクセス用

    LfoEngine lfoEngine;
    Lfo5Engine lfo5Engine;

    // ===== 【V1.1.0 削除】FilterA_SVF_SIMD svfQuad =====
    //   全 4 インスタンスが常時 disabled であり、出力は直後の TptFilter 処理に
    //   必ず上書きされていた（実質デッドコード）ためメンバごと除去。
    //   ソースファイルは Source/DSP/FilterA_SVF_SIMD.* に温存してあるが、
    //   V1.1.0 で CMakeLists のビルド対象からも外した（無駄なコンパイルの削減）。
    //   再利用する際は CMakeLists への追加も忘れないこと。

    // 全 28 モデル（Model 0 の Clean SVF を含む）を TptFilter が処理する
    TptFilter filterA, filterB, filterC, filterD;

    std::array<juce::AudioBuffer<float>, 4> filterBuffers;
    juce::AudioBuffer<float> dryBuffer;

    float currentGainReduction[2] = { 1.0f, 1.0f };

    // ===== Ableton Live フェイルセーフ用 =====
    double expectedSampleRate = 0.0;

    // ===== 【V1.1.0 追加】レイテンシ変化のホスト通知 =====
    // 旧実装は prepareToPlay() でしか setLatencySamples() を呼んでおらず、
    // OS Quality を切り替えたり（Auto 時に）モデルを変えたりして
    // オーバーサンプリング量が変わってもホストへ伝わらず、
    // 遅延補正（PDC）がずれたままになっていた。
    //
    // processBlock は pendingLatencySamples に値を置くだけ（ロックなし）。
    // timerCallback がメッセージスレッドで差分を検出して通知する。
    void timerCallback() override;

    std::atomic<int> pendingLatencySamples { 0 };
    int              reportedLatencySamples = -1;   // メッセージスレッドからのみ触る

    // ===== パラメータスムージング（統一的な 5ms タイムコンスタント）オリジナル復元 =====
    float lastDryWet = 0.5f;                   // 0.0-1.0 range（dB domain ではない）
    float lastMasterGainLinear = 1.0f;         // linear scale（dB→linear 変換済み）
    float lastCeilingLinear = 0.977f;          // linear scale
    float lastMorphX = 0.5f;                   // Morph X スムージング
    float lastMorphY = 0.5f;                   // Morph Y スムージング
    float lastLfo5Mod = 0.5f;                  // LFO5 modulation スムージング（P4）

    // ===== Envelope Follower =====
    float envFollowEnvelopeValue = 0.0f;  // Attack/Release で平滑化された入力レベル
    double envFollowerSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};