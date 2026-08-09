// ==========================================
// DSP/LfoEngine.h
// LFO状態管理・波形計算エンジン
//
// 責務:
//   - 3系統のLFO状態を保持・更新
//   - 19種類の波形を計算
//   - Boundary処理 (Clip / Bounce / Wrap)
//   - Recordingバッファの読み出し
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class LfoEngine
{
public:
    // ===== 録音状態をまとめた構造体 =====
    // PluginProcessor の public メンバへの参照をまとめて渡すための構造体
    // XYPadComponent からも参照される
    struct RecordingContext
    {
        std::array<juce::Point<float>, 2048>* buffers;   // recBuffer[3]
        std::atomic<int>* lengths;                      // recLength[3]
        std::atomic<bool>* isWaiting;                    // isWaitingForRecord[3]
        std::atomic<bool>* isDragging;                   // isRecordingDrag[3]
        std::atomic<float>* currentX;                     // currentRecX[3]
        std::atomic<float>* currentY;                     // currentRecY[3]
    };

    LfoEngine();

    void prepare(double newSampleRate);

    // ===== パラメータポインタのキャッシュ =====
    // 【V1.1.0 追加】
    // 旧実装は processSingleLfo() の冒頭で
    //   juce::String id = "lfo" + juce::String(i + 1);
    // を組み立て、さらに id + "en" / id + "wave" … と連結していた。
    // juce::String は SSO を持たず常にヒープ確保するため、
    // LFO1〜3 と LFO4 を合わせてブロック毎に約 40 回の malloc が
    // オーディオスレッド上で発生していた（CLAUDE.md §1 違反）。
    //
    // 必ず AudioProcessor のコンストラクタ（メッセージスレッド）から
    // 一度だけ呼ぶこと。以後 process() は apvts に触れない。
    void cacheParams(juce::AudioProcessorValueTreeState& apvts);

    // ===== メイン処理 =====
    // processBlock から毎ブロック呼び出す
    // （パラメータは cacheParams() でキャッシュ済みのポインタから読む）
    void process(float dt,
        double bpm,
        float baseX,
        float baseY,
        const RecordingContext& rec);

    // ===== 出力アクセサ =====
    juce::Point<float>   getPosition(int index) const;
    std::array<float, 4> getMod4(int index) const;

    // ===== Recording データセッター =====
    // XYPadComponent から呼び出される。
    // ・recordingData: 生ピクセル中心列（Step モード用）
    // ・smoothedRecData: 3パス移動平均で平滑化（Smooth モード用）
    void setRecordingData(int index, const std::array<juce::Point<float>, 2048>& buffer, int len)
    {
        if (index < 0 || index >= 3) return;
        recordingData[index]    = buffer;
        recordingLength[index]  = len;

        // ===== Smooth モード用: 3パス移動平均で事前平滑化 =====
        // 移動平均を繰り返すことで B-スプライン的な滑らかな曲線に近づく。
        // ループ接続（先頭・末尾をリングで扱う）で途切れのないループが作れる。
        smoothedRecData[index] = buffer;
        if (len > 2)
        {
            auto& sd = smoothedRecData[index];
            std::array<juce::Point<float>, 2048> tmp;
            for (int pass = 0; pass < 3; ++pass)
            {
                tmp = sd;
                for (int k = 0; k < len; ++k)
                {
                    const int km1 = (k - 1 + len) % len;
                    const int kp1 = (k + 1) % len;
                    sd[k].x = 0.25f * tmp[km1].x + 0.5f * tmp[k].x + 0.25f * tmp[kp1].x;
                    sd[k].y = 0.25f * tmp[km1].y + 0.5f * tmp[k].y + 0.25f * tmp[kp1].y;
                }
            }
        }
    }

    // ===== フェーズリセット =====
    void resetPhase(int index)
    {
        if (index < 0 || index >= 3) return;
        states[index].phase = 0.0f;
    }

    // ===== Filter Phase Spread =====
    // Spread>0 のとき mod4[i][f] がフィルターごとに異なる位相の波形値になる
    bool isSpreadActive(int idx) const { return idx >= 0 && idx < 3 && spreadActive[idx]; }

    // ===== LFO4 レート変調の状態（UI 表示用）=====
    // 【V1.1.0 追加】LFO4 は LFO1/2/3 のレートを 2^(waveX*depth) 倍する。
    // UI 側でレートスライダーとレート表示を動かすために公開する。

    /** 現在のレート変調係数。1.0 = 変調なし、2.0 = 2倍速。 */
    float getRateModulation() const { return lfo4RateModulation; }

    /** その LFO が LFO4 のアサイン先になっているか。 */
    bool isRateModulated(int idx) const
    {
        return idx >= 0 && idx < 3 && lfo4RateModulationActive[idx];
    }

    /** 実際に位相を進めるのに使われているレート [Hz]。
        Sync / Free の別と LFO4 変調をすべて反映した最終値なので、
        UI はこれを表示するだけで DSP と必ず一致する。 */
    float getEffectiveRate(int idx) const
    {
        return (idx >= 0 && idx < 3) ? effectiveRateHz[idx] : 0.0f;
    }

private:
    // ===== LFO内部状態 =====
    struct LfoState
    {
        juce::Random rng;
        float phase = 0.0f;

        juce::Point<float>   currentRandom{ 0.5f, 0.5f };
        juce::Point<float>   nextRandom{ 0.5f, 0.5f };
        std::array<float, 4> currentRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, 4> nextRand1{ 0.5f, 0.5f, 0.5f, 0.5f };

        float bilX = 0.0f, bilY = 0.0f;
        float bilVx = 1.3f, bilVy = 1.7f;

        float smoothNx = 0.5f, smoothNy = 0.5f;
        float tNextNx = 0.5f, tNextNy = 0.5f;

        // Fade-in エンベロープ [0, 1]
        // LFO 無効時にリセット → 次に有効化されたとき 0 から立ち上がる
        float fadeEnv = 0.0f;
    };

    double sampleRate = 48000.0;

    // ===== パラメータポインタキャッシュ =====
    // APVTS が保持する std::atomic<float> の寿命はプロセッサと同一なので
    // 一度解決したポインタが途中で無効になることはない。
    struct LfoParamPtrs
    {
        std::atomic<float>* en       = nullptr;
        std::atomic<float>* wave     = nullptr;
        std::atomic<float>* step     = nullptr;
        std::atomic<float>* sync     = nullptr;
        std::atomic<float>* minVal   = nullptr;
        std::atomic<float>* maxVal   = nullptr;
        std::atomic<float>* phase    = nullptr;
        std::atomic<float>* fade     = nullptr;
        std::atomic<float>* rateSync = nullptr;
        std::atomic<float>* rateFree = nullptr;
        std::atomic<float>* spread   = nullptr;
    };

    struct Lfo4ParamPtrs
    {
        std::atomic<float>* en       = nullptr;
        std::atomic<float>* wave     = nullptr;
        std::atomic<float>* step     = nullptr;
        std::atomic<float>* sync     = nullptr;
        std::atomic<float>* rateSync = nullptr;
        std::atomic<float>* rateFree = nullptr;
        std::atomic<float>* depth    = nullptr;
        std::atomic<float>* assignA  = nullptr;
        std::atomic<float>* assignB  = nullptr;
        std::atomic<float>* assignC  = nullptr;
    };

    LfoParamPtrs   lp[3];    // LFO1 / LFO2 / LFO3
    Lfo4ParamPtrs  lp4;      // LFO4 (Rate Modulation)
    bool           paramsCached = false;

    LfoState             states[3];
    juce::Point<float>   positions[3];
    std::array<float, 4> mod4[3];
    bool                 spreadActive[3] = { false, false, false };

    // 表示用: 各 LFO が実際に使っているレート [Hz]（LFO4 変調適用後）
    float                effectiveRateHz[3] = { 0.0f, 0.0f, 0.0f };

    // ===== LFO4: Rate Modulation 専用 =====
    // LFO1と同じLfoStateを使用、Recording不要
    LfoState             lfo4State;
    float                lfo4RateModulation = 1.0f;  // Rate変調係数 (1.0 = no mod)
    bool                 lfo4RateModulationActive[3] = { false, false, false };  // アサイン先フラグ

    // ===== Recording バッファ =====
    std::array<juce::Point<float>, 2048> recordingData[3];     // Step モード用 (生ピクセル中心)
    std::array<juce::Point<float>, 2048> smoothedRecData[3];   // Smooth モード用 (事前平滑化済み)
    int recordingLength[3] = { 0, 0, 0 };

    // ===== ヘルパー =====
    static float getSyncTime(int selection, double bpm);
    static float hermite(float p0, float p1, float p2, float p3, float t);

    // Filter Phase Spread 用: 単一位相でのX軸波形値を返す補助関数
    // 周期的な数式波形にのみ対応（Billiard/SmoothNoise/Recording等のステートフル波形は非対応）
    static float evaluateWaveX(int wave, float p, float t);

    // 1つのLFOを処理する内部メソッド
    void processSingleLfo(int index,
        float dt,
        double bpm,
        float baseX,
        float baseY,
        const RecordingContext& rec);

    // LFO4専用処理（Rate Modulation）
    void processLFO4(float dt, double bpm);
};