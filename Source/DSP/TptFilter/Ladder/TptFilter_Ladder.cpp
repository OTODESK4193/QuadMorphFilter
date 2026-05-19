// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
//
// Model 2: TB-303 Diode Ladder
//   Wurtz (Karrikuh) + Stinchcombe 実装
//   8Hz HPF フィードバック + ハードクリッパー
//
// Model 1,12,13,15: Moog Ladder 系
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Ladder
{

    // ==========================================
    // TPT 1次ハイパスフィルター
    // Stinchcombe: 結合キャパシタの極低域特性を模倣
    // fc ≈ 8Hz（固定）→ これがアシッドうねりの物理的根拠
    // ==========================================
    static inline float tpt1HPF(float x, float& s, float g)
    {
        // v = (x - s) * g / (1 + g)  ← ZDF積分器
        // lp = v + s_prev
        // hp = x - lp
        // s_new = s_prev + 2*v       ← 台形積分の状態更新
        float v = (x - s) * g / (1.0f + g);
        float lp = v + s;
        float hp = x - lp;
        s = lp + v;
        return hp;
    }

    // ==========================================
    // ハードクリッパー
    // Wurtz: ソフトクリッパーではチューニングがドリフトする
    // ハードクリッパーなら発振時でも V/Oct が安定
    // ==========================================
    static inline float hardClip(float x, float threshold = 1.0f)
    {
        return std::clamp(x, -threshold, threshold);
    }

    // ==========================================
    // updateCoeffs（TB-303 専用）
    // ==========================================
    void updateCoeffs(TptFilterState& st)
    {
        if (st.filterModel != 2) return;

        const float fc = st.smoothedDigitalCutoff;
        const float fs = (float)st.sampleRate;

        // ZDF係数: g = tan(π * fc / fs)
        float safeFc = std::clamp(fc, 20.0f, fs * 0.45f);
        st.diode_g = std::tan(juce::MathConstants<float>::pi * safeFc / fs);

        // 8Hz HPF係数（固定）
        // TB-303の結合キャパシタによる自然発生的な極低域HPF
        const float hpfFc = 8.0f;
        st.diode_h = std::tan(juce::MathConstants<float>::pi * hpfFc / fs);
    }

    // ==========================================
    // TB-303 Diode Ladder メイン処理
    //
    // 設計方針（Wurtz/Stinchcombe準拠）:
    //   1. 4段ZDF LPF（直列、1段ずつ順次計算）
    //   2. 入力はハードクリッパーで制限
    //   3. フィードバックは 8Hz HPF を通す
    //      → これがTB-303アシッドうねりの本質
    //
    // Moog Ladderとの主な違い:
    //   - フィードバックパスに HPF が入っている
    //   - レゾナンスをより高くできる（~17倍）
    //   - ハードクリッパーによる安定した発振
    // ==========================================
    static float processDiodeLadder(int ch, float x, TptFilterState& st)
    {
        const float g = st.diode_g;
        const float res = st.currentResVal;

        // 状態変数への参照（直接更新される）
        float& s1 = st.zdfState[0][ch][0];
        float& s2 = st.zdfState[0][ch][1];
        float& s3 = st.zdfState[0][ch][2];
        float& s4 = st.zdfState[0][ch][3];

        // レゾナンス係数
        // TB-303は Moog より高いフィードバック量が必要
        // 0.0〜17.0 の範囲（≈ 0.9 付近で自己発振）
        float k = juce::jmap(res, 0.1f, 10.0f, 0.0f, 17.0f);

        // ===== Step 1: 8Hz HPF フィードバック =====
        // Stinchcombe の最大の発見:
        //   TB-303 は ACカップリングにより 8Hz HPF が
        //   フィードバックパス内部に存在する
        //   → これがアシッドスクエルチの物理的根拠
        //   → 人為的な非対称 tanh ではなく自然発生する非対称性
        float fb = tpt1HPF(k * 0.25f * s4, st.diodeHpfS[ch], st.diode_h);

        // ===== Step 2: ハードクリッパー =====
        // Wurtz の重要なポイント:
        //   ソフトクリッパー（x / (1 + |x|) 等）は
        //   継続的なゲイン低下でチューニングをドリフトさせる
        //   ハードクリッパーなら発振時でも安定
        float x_in = hardClip(x - fb, 1.0f);

        // ===== Step 3: 4段 ZDF LP フィルター =====
        // 各段: v = (x - s) * g / (1+g), y = v + s, s += 2*v
        // Diode Ladder と Moog の違い:
        //   Moog  → 各段が抵抗でバッファリングされ独立
        //   Diode → 各段が直結で相互にインピーダンス負荷
        //           → 実効的に g が段を経るごとに変化
        // ここでは実用的な近似として標準ZDFを使用し
        // 8Hz HPFフィードバックでTB-303の音色特性を再現する
        float inv1pg = 1.0f / (1.0f + g);

        float v1 = (x_in - s1) * g * inv1pg;
        float y1 = v1 + s1;
        s1 += 2.0f * v1;

        float v2 = (y1 - s2) * g * inv1pg;
        float y2 = v2 + s2;
        s2 += 2.0f * v2;

        float v3 = (y2 - s3) * g * inv1pg;
        float y3 = v3 + s3;
        s3 += 2.0f * v3;

        float v4 = (y3 - s4) * g * inv1pg;
        float y4 = v4 + s4;
        s4 += 2.0f * v4;

        // ===== Step 4: 出力選択 =====
        // TB-303 は LP のみが有効（UIでも制限）
        // 互換性のため BP/HP も出力可能にしておく
        float out = 0.0f;
        if (st.filterType == 0) out = y4;           // LP
        else if (st.filterType == 1) out = y2 - y4;      // BP
        else if (st.filterType == 2) out = x_in - y2;    // HP
        else                          out = y4 + x_in - y2; // Notch

        return out;
    }

    // ==========================================
    // processSample（全Ladderモデルのディスパッチ）
    // ==========================================
    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 2: TB-303 Diode Ladder -----
        if (m == 2)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
                out = processDiodeLadder(ch, out, st);
            return out;
        }

        // ----- Model 1,12,13,15: Moog Ladder 系 -----
        if (m == 1 || m == 12 || m == 13 || m == 15)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float s1_ = st.zdfState[stage][ch][0];
                float s2_ = st.zdfState[stage][ch][1];
                float s3_ = st.zdfState[stage][ch][2];
                float s4_ = st.zdfState[stage][ch][3];

                float S1 = s1_ / (1.0f + st.g);
                float S2 = s2_ / (1.0f + st.g);
                float S3 = s3_ / (1.0f + st.g);
                float S4 = s4_ / (1.0f + st.g);

                float sigma = st.ladderG * st.ladderG * st.ladderG * S1
                    + st.ladderG * st.ladderG * S2
                    + st.ladderG * S3 + S4;

                float u = out - st.ladderRes * sigma;
                if (m == 1)  u = std::tanh(u / (1.0f + st.ladderRes
                    * st.ladderG * st.ladderG * st.ladderG * st.ladderG));
                else if (m == 12) u = std::tanh(u * 1.1f) / 1.1f;
                else if (m == 13) u = std::tanh(u * 1.5f) / 1.5f;
                else if (m == 15) u = u / (1.0f + std::abs(u * 0.5f));

                float v1 = (u - s1_) * st.ladderG;
                float y1 = v1 + s1_;
                if (m == 15) y1 = std::tanh(y1);
                st.zdfState[stage][ch][0] = s1_ + 2.0f * v1;

                float v2 = (y1 - s2_) * st.ladderG;
                float y2 = v2 + s2_;
                if (m == 15) y2 = std::tanh(y2);
                st.zdfState[stage][ch][1] = s2_ + 2.0f * v2;

                float v3 = (y2 - s3_) * st.ladderG;
                float y3 = v3 + s3_;
                if (m == 15) y3 = std::tanh(y3);
                st.zdfState[stage][ch][2] = s3_ + 2.0f * v3;

                float v4 = (y3 - s4_) * st.ladderG;
                float y4 = v4 + s4_;
                if (m == 15) y4 = std::tanh(y4);
                st.zdfState[stage][ch][3] = s4_ + 2.0f * v4;

                if (st.slopeIdx == 0) {
                    if (st.filterType == 0) out = y2;
                    else if (st.filterType == 1) out = 2.0f * (y1 - y2);
                    else if (st.filterType == 2) out = out - 2.0f * y1 + y2;
                    else                         out = y2 + (out - 2.0f * y1 + y2);
                }
                else {
                    if (st.filterType == 0) out = y4;
                    else if (st.filterType == 1) out = 4.0f * (y2 - y4);
                    else if (st.filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
                    else                         out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4);
                }
            }
        }

        return out;
    }

} // namespace TptFilter_Ladder