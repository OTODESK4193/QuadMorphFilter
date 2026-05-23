# QuadMorphFilter — プロジェクト概要整理

> 更新日: 2026-05-23  
> 現在の作業: **Model 1 (Moog) と Model 2 (TB-303) の検証中**

---

## プロジェクト基本情報

| 項目 | 内容 |
|---|---|
| プラグイン名 | Quad-Morph Filter |
| 開発者 | OTODESK |
| バージョン | 1.0.0 |
| フォーマット | VST3 / Standalone |
| フレームワーク | JUCE 8 (`C:/JUCE`) |
| 言語 | C++20 |
| ビルドシステム | CMake 3.24 |
| SIMD | xsimd 13.0.0 (FetchContent), AVX2 |
| ビルド出力 | `out/build/x64-Release/` |

---

## ディレクトリ構成

```
QuadMorphFilter/
├── CMakeLists.txt          ← ビルド定義（xsimd, JUCE設定）
├── CMakeSettings.json
├── Source/
│   ├── PluginProcessor.h / .cpp   ← プラグイン本体・音声処理
│   ├── PluginEditor.h / .cpp      ← UI全体
│   ├── DSP/
│   │   ├── TptFilterState.h       ← 全DSP状態変数の共有構造体
│   │   ├── TptFilter.h / .cpp     ← フィルタースイッチャー（dispatcher）
│   │   ├── FilterA_SVF_SIMD.h/.cpp← SVF専用 AVX2 SIMD実装
│   │   ├── LfoEngine.h / .cpp     ← LFOモジュレーション
│   │   ├── MorphEngine.h / .cpp   ← 等パワーモーフィング計算
│   │   ├── ModelCapabilities.h    ← モデルごとのスロープ/タイプ有効範囲定義
│   │   └── TptFilter/
│   │       ├── SVF/               ← モデル 0,3,4,6,7,9,14,16,23
│   │       ├── Ladder/            ← モデル 1,2,12,13,15  ← 現在作業中
│   │       ├── AnalogEmulation/   ← モデル 5,21,22,27
│   │       ├── DigitalPrecision/  ← モデル 17-20
│   │       ├── Spectral/          ← モデル 8,11,24,25,26
│   │       └── Experimental/      ← モデル 10
│   └── UI/
│       ├── FilterVisualizer.h/.cpp← 周波数特性グラフ
│       ├── QuadMorphLookAndFeel   ← カスタムUI外観
│       └── XYPadComponent         ← モーフィングXYパッド
└── out/                           ← ビルド成果物
```

---

## アーキテクチャ（データフロー）

```
AudioIn
  ↓
PluginProcessor::processBlock()
  ├─ [SVF モデル] FilterA_SVF_SIMD (4インスタンス並列, AVX2)
  └─ [その他27モデル] TptFilter × 4 (filterA / B / C / D)
       └─ TptFilter::process()
            ├─ updateCoefficients() (16サンプルごと)
            └─ processSample()
                 └─ dispatch → TptFilter_Ladder / SVF / AnalogEmulation / ...
                               └─ TptFilterState を参照・更新

MorphEngine::computeEqualPowerWMix(x, y)  ← XYパッド位置
  → 4つのフィルター出力をウェイト合算

LfoEngine → モジュレーション値 → MorphEngine::computeModulation()

AudioOut
```

---

## フィルターモデル一覧

### Ladder カテゴリ（現在検証中）

| Model # | 名称 | 特性 | Type | Slope/Accent |
|---|---|---|---|---|
| **1** | **Moog Ladder** | 4段ZDF, tanh非線形, r_scale=4.5 | LP/BP/HP | 12dB / 24dB |
| **2** | **TB-303 Diode** | Wurtz/Stinchcombe, 8Hz HPF, Accent制御 | LP のみ | Off/Low/High |
| 12 | Prophet Curtis | tanh(u * 1.1f) / 1.1f | LP/BP/HP | 12dB / 24dB |
| 13 | SSM 2040 | tanh(u * 1.5f) / 1.5f, r_scale=5.0 | LP のみ | 12dB / 24dB |
| 15 | Jupiter Roland | ソフトクリップ `u/(1+|u*0.5|)`, 各段tanh | LP のみ | 12dB / 24dB |

### SVF カテゴリ

| Model # | 名称 |
|---|---|
| 0 | Clean SVF (SIMD版と兼用) |
| 3 | MS-20 SK Filter |
| 4 | Bitcrush / Sample Rate Reducer |
| 6 | Comb Filter |
| 7 | MS-20 Style (高Res) |
| 9 | Kilo (All-Pass) |
| 14 | CS-80 |
| 16 | Phaser |
| 23 | Waveguide Mesh |

### その他カテゴリ

| Model # | 名称 | カテゴリ |
|---|---|---|
| 5 | Formant (Vowel) | AnalogEmulation |
| 21 | Vactrol LPG | AnalogEmulation |
| 22 | Modal Resonator | AnalogEmulation |
| 27 | Nyquist Anti-alias | AnalogEmulation |
| 10 | Reverb FDN | Experimental |
| 11 | Spectral Spread | Spectral |
| 24 | Bode Freq Shifter | Spectral |
| 25 | Z-Plane 2D Morph | Spectral |
| 26 | Phased Array | Spectral |
| 17-20 | Digital Precision (各種) | DigitalPrecision |

---

## 現在の検証内容（Moog & TB-303）

### Model 1: Moog Ladder

```
入力 → tanh(u) 非線形 → 4段 ZDF LP →
  フィードバック: k = jmap(res, 0.1, 10, 0, 4.5)
  出力: slopeIdx==0 → 12dB (y2)
        slopeIdx==1 → 24dB (y4)
```

- **comp補正**: `1.0f + 0.5f * ladderRes`（入力をブーストしてAGCと連携）
- **AGC**: Res高時はバイパスファクターで自然発振を守る

### Model 2: TB-303 Diode Ladder

```
入力 → tanh(u) → 4段 ZDF LP (各段tanh) → 8Hz HPF → 出力×0.95
  係数: g = tan(π * fc / fs)
        h = tan(π * 8 / fs)  ← 固定HPF
  フィードバック k_max:
    Accent Off  → 4.2
    Accent Low  → 4.6
    Accent High → 5.0
```

- **Slopeコンボ = Accentコンボ**として流用（UI上の表示変更が必要な場合あり）
- `diodeHpfS[]` と `diodePrevY4[]` が専用状態変数

### 共通: AGC（自動ゲイン補正）

```
targetGain = sqrt(rmsIn / rmsOut)  ← クランプ: 0.1 〜 15.0
agcGain[ch] += 0.0005 * (targetGain - agcGain[ch])  ← スムーズ追従
発振時バイパス: k_norm > 0.85 → agcGain → 1.0 に引き寄せ
```

---

## 係数更新の仕組み

- `process()` ループ内で **16サンプルごと** に `updateCoefficients()` を呼ぶ
- `smoothedDigitalCutoff` は RC一次ローパス相当でスムージング
- Model 2 (TB-303) は `TptFilter_Ladder::updateCoeffs()` が呼ばれ、`diode_g` と `diode_h` を更新

---

## TptFilterState 主要フィールド

```cpp
double sampleRate;       // サンプルレート
int    filterModel;      // モデル番号 (0-27)
int    filterType;       // 0=LP, 1=BP, 2=HP, 3=Notch
int    slopeIdx;         // 0〜3 (TB-303では Accent Off/Low/High)
int    currentStages;    // カスケード段数
float  smoothedDigitalCutoff;  // スムーズ後のカットオフ周波数
float  currentResVal;    // スムーズ後のレゾナンス
float  g, ladderG, ladderRes;  // 共通係数
float  zdfState[8][2][4];      // Ladder状態変数 [stage][ch][pole]
float  diode_g, diode_h;       // TB-303専用係数
float  diodeHpfS[2];           // TB-303 HPF状態変数
```

---

## ビルド方法

```
Visual Studio で out/build/x64-Release を選択
→ CMake の構成が自動実行
→ "ビルド" でVST3生成
出力先: out/build/x64-Release/QuadMorphFilter_artefacts/
```

---

## 今後の課題メモ

- [ ] Moog / TB-303 の音質最終確認（周波数特性グラフとの一致）
- [ ] ModelCapabilities.h のUI反映（TB-303のSlope→Accent表示切り替え）
- [ ] オーバーサンプリング: Moog(Model 1)とTB-303(Model 2)は `getAutoOsFactor` で×2推奨
- [ ] `FilterA_SVF_SIMD` とその他モデルのモーフィング統合確認
