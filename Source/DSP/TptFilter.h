// ==========================================
// TptFilter.h
// ==========================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// 状態変数（カテゴリファイルと共有）
#include "TptFilterState.h"

// カテゴリ実装
#include "TptFilter/SVF/TptFilter_SVF.h"
#include "TptFilter/Ladder/TptFilter_Ladder.h"
#include "TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.h"
#include "TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.h"
#include "TptFilter/Spectral/TptFilter_Spectral.h"
#include "TptFilter/Experimental/TptFilter_Experimental.h"

class TptFilter
{
public:
    TptFilter();
    ~TptFilter() = default;

    void prepare(double newSampleRate, int samplesPerBlock, int numChannels);
    void reset();

    void setModel(int newModel);
    void setCutoff(float newCutoff);
    void setResonance(float newResonance);
    void setType(int newType);
    void setSlope(int slopeIndex);

    void  process(juce::AudioBuffer<float>& buffer);
    float getMagnitudeForFrequency(float frequency) const;

    // Oversampling API
    void setOsMode(int mode);
    int  getOsLatencySamples() const;

private:
    // ===== DSP 状態（カテゴリファイルが参照）=====
    TptFilterState state;

    // ===== TptFilter 固有（カテゴリファイルに非公開）=====
    // 【V1.1.0】preparedBlockSize は旧 rebuildOversampler() 専用だったため削除。
    // オーバーサンプラーは prepare() でブロックサイズを直接受け取って初期化する。
    int    maxChannels = 2;

    juce::SmoothedValue<float> cutoff;
    juce::SmoothedValue<float> resonance;
    float lastCutoff = -1.0f;
    float lastRes = -1.0f;

    // ===== Oversampling =====
    // 【V1.1.0 修正】オーディオスレッド上での再構築を廃止。
    //
    // 旧実装は setOsMode() / setModel() から rebuildOversampler() を呼び、
    // その中で std::make_unique と initProcessing()（内部バッファ確保）、
    // さらに旧インスタンスの解放を行っていた。これらはすべて processBlock から
    // 呼ばれる経路であり、CLAUDE.md §1 の「動的メモリ確保の禁止」に抵触する。
    // しかも OS Quality の既定値が Auto のため、ユーザーが Model ツマミを
    // 回すたびに確保と解放が走っていた（モデル切替時のプチノイズの原因）。
    //
    // 現在は prepare() で 2x / 4x の両方を生成しておき、切り替えは
    // ポインタを選び直すだけにしている。確保はオーディオスレッドで一切起きない。
    // メモリコストは 1 フィルターあたり数十 KB 程度。
    std::unique_ptr<juce::dsp::Oversampling<float>> os2x;   // factor 1 (2倍)
    std::unique_ptr<juce::dsp::Oversampling<float>> os4x;   // factor 2 (4倍)
    int currentOsFactor = 0;      // 0 = OS 無効 / 1 = 2x / 2 = 4x
    int osMode = 0;

    /** 現在の OS ファクターに対応するオーバーサンプラー（無効時は nullptr）。 */
    juce::dsp::Oversampling<float>* activeOversampler() const noexcept
    {
        switch (currentOsFactor)
        {
        case 1:  return os2x.get();
        case 2:  return os4x.get();
        default: return nullptr;
        }
    }

    /** OS ファクターを切り替える（確保を伴わない）。 */
    void selectOsFactor(int newFactor);

    // ===== 内部メソッド =====
    float processSample(int channel, float x);
    void  updateCoefficients();

    // ===== SR 依存タイムコンスタント係数の更新 =====
    // AGC (rmsCoef / agcCoef) と MS-20 DC ブロッカー (ms20DcAlpha) は
    // processSample() の内部で使われるため、オーバーサンプリング有効時は
    // 「OS 後のレート」を基準に計算しなければ時定数がずれる。
    // lastCoeffRate によるガードで、レートが実際に変化したときだけ
    // std::exp() を再計算する（ブロック毎の無駄な超越関数呼び出しを回避）。
    // 動的確保・ロック・システムコールを含まないためオーディオスレッドから呼んで安全。
    void   updateRateDependentCoeffs(double effectiveRate);
    double lastCoeffRate = 0.0;

    int  getAutoOsFactor(int modelIdx) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TptFilter)
};