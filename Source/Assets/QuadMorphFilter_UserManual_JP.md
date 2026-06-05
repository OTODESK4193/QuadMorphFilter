# Quad Morph Filter — ユーザーマニュアル 日本語版

**バージョン：v1.0.0**  
**最終更新：2024年**  
**開発元：OTODESK**

---

## 目次

1. [はじめに](#はじめに)
2. [安全上の重要な注意](#安全上の重要な注意)
3. [インストール](#インストール)
4. [基本操作](#基本操作)
5. [フィルターシステム](#フィルターシステム)
6. [モーフィング機能](#モーフィング機能)
7. [LFOシステム](#lfoシステム)
8. [高度な機能](#高度な機能)
9. [パラメータ詳細説明](#パラメータ詳細説明)
10. [使用例とテクニック](#使用例とテクニック)
11. [トラブルシューティング](#トラブルシューティング)
12. [FAQ](#faq)
13. [テクニカル仕様](#テクニカル仕様)
14. [ライセンス](#ライセンス)

---

## はじめに

**Quad Morph Filter** は、28種類の厳選されたフィルターアルゴリズムをリアルタイムでモーフィングできるVST3プラグインです。XYパッド上で直感的に4つのフィルターを融合させたり、19種類のLFO波形で複雑な変調を施したり、手描きのXYパターンを記録・再生することで、無限の音響表現が可能です。

本マニュアルでは、プラグインの全機能、詳細なパラメータ説明、実践的な使用例を提供します。

### 主な特徴

- **28個のプレミアムフィルターモデル** — Moog Ladder、TB-303、SVF、FDNリバーブなど
- **等パワーモーフィングエンジン** — 最大4つのフィルターを無アーティファクト融合
- **19波形LFOシステム** — 幾何学的パターンから混沌的アトラクタまで
- **リアルタイム周波数応答ビジュアライザー** — フィルターの変化を視覚的に確認
- **XYパターンレコーディング** — マウス描画軌跡を記録・再生
- **Ableton Live 対応** — 厳格なリアルタイム安全性を実装

---

## 安全上の重要な注意

### ⚠️ 聴覚保護

**このプラグインは大きな音量を出力できます。聴覚保護はあなたの責任です。**

- スピーカーやヘッドフォン使用時は、必ず出力レベルをモニタリングしてください
- **低い音量から徐々に上げてください**
- **ヘッドフォンを最大音量で使用しないでください**
- 長時間の使用中は定期的に休憩を取ってください
- 85 dB SPL 以上の長時間露出は、永久的な聴覚損傷の原因になります
- 本プラグインを使用することで、あなたは自身の聴覚安全と機器保護について、全責任を負うことに同意します

### 使用上の注意

- 予期しない音量出力に備えて、マスターボリュームを低く設定してスタートしてください
- フィルタリゾナンスを高い値に設定すると、高い周波数で共鳴ピークが生じます
- オーバーサンプリングを有効にすることで、特に Moog および TB-303 モデルのエイリアシングを抑制します

---

## インストール

### VST3 プラグインのインストール

1. [最新リリース](https://github.com/OTODESK4193/QuadMorphFilter/releases/latest) から `QuadMorphFilter.vst3` をダウンロード
2. 以下のディレクトリに `.vst3` フォルダをコピー：
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. DAW（Ableton Live など）でプラグインをスキャン

### スタンドアロン版

別途提供される `Quad-Morph Filter.exe` をダイレクトに実行できます。DAWは不要です。

### 必要な環境

- **Windows 10 / 11（64-bit）**
- **AVX2 対応 CPU**
- **テスト済み DAW：Ableton Live 11 / 12**
- **その他 DAW（Reaper, Studio One など）での動作は未検証**

---

## 基本操作

### インターフェース概要

Quad Morph Filter のメインウィンドウは以下のセクションで構成されています：

```
┌─────────────────────────────────────────────┐
│ Filter Panel                                │
│  ├─ Filter A/B/C/D (On/Off, Model, Type)  │
│  ├─ Cutoff / Resonance スライダー          │
│  └─ LFO Cut/Res ボタン                     │
├─────────────────────────────────────────────┤
│ XY Morph Pad                                │
│  ├─ モーフパッド（4コーナーに A/B/C/D）  │
│  ├─ LFO Tab (LFO1/2/3 選択)               │
│  └─ Recording Grid                         │
├─────────────────────────────────────────────┤
│ Visualizer & E-Button                       │
│  ├─ リアルタイム周波数応答グラフ            │
│  └─ E-ボタン（背景色ランダム化）          │
├─────────────────────────────────────────────┤
│ LFO Control Section                         │
│  ├─ LFO1/2/3 パラメータ                   │
│  └─ LFO4/5 専用パラメータ                  │
├─────────────────────────────────────────────┤
│ Master Controls                             │
│  ├─ Output Gain / Dry-Wet Mix             │
│  ├─ Oversampling / Limiter Ceiling        │
│  └─ Envelope Follower                      │
└─────────────────────────────────────────────┘
```

### マウス操作

| 操作 | 対象 | 効果 |
|---|---|---|
| **ドラッグ** | XYパッド | モーフポジション変更 |
| **左クリック** | E-ボタン | 背景色をランダム化 |
| **右クリック** | E-ボタン | 背景色をデフォルトにリセット |
| **スライダードラッグ** | パラメータ | 値をスムーズに変更 |
| **Ctrl + クリック** | スライダー | デフォルト値にリセット |

---

## フィルターシステム

### 4フィルター構成

Quad Morph Filter は、**A、B、C、D の4つのフィルター**を同時に動作させることで、複雑な周波数応答を実現します。

各フィルターは以下を独立して制御できます：

- **Enable** — オン/オフ
- **Model** — 0～27の28種類から選択
- **Type** — LP（ローパス）/ BP（バンドパス）/ HP（ハイパス）/ Notch（ノッチ）
- **Slope** — 12 / 24 / 48 / 96 dB/oct（モデルごとに異なる選択肢）
- **Cutoff** — 20 Hz ～ 20 kHz
- **Resonance（Res / Ctrl）** — 0.1 ～ 10.0

### フィルターモデルの分類

#### Ladder フィルター（5モデル）

アナログシンセの象徴的な4極ローパスフィルター。強い共鳴特性。

- **Model 1: Moog Ladder** — 最も有名なアナログフィルター。温かく、象徴的な響き
- **Model 2: TB-303 Diode** — 酸っぱいベースサウンドの定義。Accent モード対応
- **Model 12: CEM3320 (Prophet)** — Curtis エレクトロニクス。柔らかいサチュレーション
- **Model 13: SSM 2040 (Oberheim)** — Oberheim 古典。非対称的なクリッピング
- **Model 15: Jupiter (Roland)** — Roland SH-101/Jupiter-8。ソフトクリップ

#### SVF フィルター（9モデル）

高精度ステートバリアブルフィルター。複数タイプ対応で柔軟性高し。

- **Model 0: Clean SVF** — 最も透明で忠実。推奨スタート地点
- **Model 3: SEM (Oberheim)** — Oberheim モダン版。ソフトサチュレーション付き
- **Model 5: Formant (Vowel)** — 3つの並列SVF。母音フォルマント模倣
- **Model 6: Comb Filter** — 遅延ライン基準。メタリックな響き
- **Model 9: Wavefolder** — サイン波展開。驚異的なウォーム感

#### AnalogEmulation（4モデル）

実回路シミュレーション。複雑で有機的な響き。

- **Model 10: FDN Reverb** — 4遅延フィードバック。リバーブ兼用
- **Model 21: Vactrol LPG** — LED 駆動 LPG。ビンテージなウォーム感
- **Model 22: Modal Resonator** — 8フォルマント帯域。スピーチライク
- **Model 27: Nyquist Anti-alias** — 高周波リミッター。エイリアシング抑制

#### DigitalPrecision（4モデル）

数学的に完璧なデジタルフィルター。エレクトロニックスの美学。

- **Model 17: Butterworth** — 最大平坦。中立的
- **Model 18: Chebyshev** — パスバンドリップル。エッジが効いた響き
- **Model 19: Bessel** — 線形位相。最小リンギング
- **Model 20: Elliptic** — ストップバンド零点。急峻な遮断

#### Spectral（5モデル）

周波数領域加工。高度な音響エンジニアリング。

- **Model 11: Kilo All-Pass** — 16段オールパス。密な位相操作
- **Model 24: Bode Frequency Shifter** — Hilbert 変換。周波数シフト（±5 kHz）
- **Model 25: Z-Plane 2D Morph** — 7ビクワッド配置。Z平面モーフィング
- **Model 26: Phased Array** — オールパス並列。ステレオ装飾効果

#### Experimental（1モデル）

実験的な音響処理。

- **Model 8: All-Pass Phaser** — 16段オールパス。微妙なフェイジング

### Cutoff Algo（カットオフアルゴリズム）

XYパッド上でカットオフ周波数を直接制御するモード：

#### Absolute（絶対位置）

```
20 Hz ─────────────────── 20 kHz
Left (0)              Right (1)
```

左端で 20 Hz、右端で 20 kHz。直感的で予測可能。

#### Relative（相対位置）

```
       Center (632 Hz)
Left: -4 oct    Right: +4 oct
```

中央を中心に ±4 オクターブ変動。音楽的で対称的。

#### Zone（ゾーンベース）

```
Left: 20Hz ──── 200Hz  |  Right: 200Hz ──── 20kHz
短い範囲              長い範囲
```

左右で異なるスケーリング。細かい制御と広い範囲の両立。

---

## モーフィング機能

### XYパッドとは

XYパッドは、**2次元平面上でフィルターパラメータを連続的に変化させる**インターフェースです。

```
        B (top-left)          D (top-right)
          ●─────────────────●
          │                 │
          │                 │
          │    XY Pad       │
          │                 │
          │                 │
          ●─────────────────●
        A (bottom-left)      C (bottom-right)
```

- **各コーナーに A, B, C, D の4つのフィルターを配置**
- **マウスドラッグでモーフポジション変更**
- **リアルタイムに周波数応答が変化**

### モーフブレンドアルゴリズム

4つのフィルターをどのように融合させるかを決定：

#### Equal Power（等パワー）

$$w_A^2 + w_B^2 + w_C^2 + w_D^2 = 1 \text{ (常に成立)}$$

**特徴：** 音量が変わらない。音響エネルギー保存。最も推奨。

**用途：** ほぼ全てのサウンドデザイン。スムーズで自然。

#### Linear（直線）

$$w_{i} = \max(0, 1 - |d_i|)$$

**特徴：** 双一次内挿。中央で各フィルター 0.25。

**用途：** より急峻な遷移。エッジが効いた変化。

#### Smoothstep（S字）

$$w_i = \text{smoothstep}(0, 1, 1 - |d_i|)$$

**特徴：** S字カーブバイアス。遠いフィルターの寄与を強調。

**用途：** オーガニックな変化。自然な滑らかさ。

#### Radial（逆距離二乗）

$$w_i \propto 1 / d_i^2$$

**特徴：** 最も近いフィルターが支配的。距離による急峻な減衰。

**用途：** ターゲット指向的な変化。最も近いコーナーを強調。

### LFO1によるモーフ変調

**LFO1** は、XYパッドの位置そのものを時間的に変調します。

```
LFO1 Wave        XY Morph Pad Position
────────────────────────────────────────
Sine Wave     →  滑らかで音楽的な循環
Sawtooth      →  急峻なスイープ
Random        →  予測不可能な跳躍
Spirograph    →  複雑な軌跡
```

**設定例：**
- LFO1: Sine, 0.5 Hz, Min=0, Max=1
- モーフパッド上でゆっくりと円形に移動

---

## LFOシステム

### LFO1, LFO2, LFO3 の役割

| LFO | 目的 | モジュレート対象 |
|---|---|---|
| **LFO1** | Morph 変調 | XY パッド位置（A/B/C/D 重み） |
| **LFO2** | Cutoff 変調 | 4つのフィルターのカットオフ周波数 |
| **LFO3** | Resonance 変調 | 4つのフィルターのレゾナンス |

### 19波形のガイド

#### 基本波形（4種）

- **Sine** — 滑らかで自然。最も音楽的
- **Sawtooth** — 明るく急峻。スイープ効果
- **Pulse** — 明るい。ステップ感
- **Triangle** — 中間的。線形斜線

#### ランダムファミリ（4種）

- **Random 1** — ステップ的ランダム。ジャンプ
- **Random 2** — スムーズランダム。有機的
- **Noise** — 白色ノイズ。激しく不規則
- **Smooth Noise** — Hermite補間ノイズ。より滑らか

#### 幾何学的パターン（6種）

複雑で予測不可能な軌跡を描く。実験的なサウンドデザインに最適。

- **Spirograph** — エピトロコイド。螺旋模様
- **Torus Knot** — 3D結び目展開。複雑なトポロジー
- **Lissajous** — 3:2 周波数比。楕円軌跡
- **Spiral** — 対数螺旋。有機成長
- **Star** — 星形放射。定期的なパルス
- **Rose** — ローズ曲線。花弁模様

#### ダイナミック波形（3種）

物理シミュレーション。環境と相互作用。

- **Billiard** — ボール衝突。バウンス物理
- **Polygon** — N角形ステップ。幾何学的
- **Attractor** — Lorenz/Henon。混沌的アトラクタ

#### 特殊波形（1種）

- **Recording** — 手描きXYパターン。完全なカスタマイズ

### テンポシンク

**Sync = On** の場合、LFO周波数は DAW のテンポに同期：

```
Tempo Divisions:
8/1 (8 小節)    → ゆっくり
4/1, 2/1, 1/1   → 標準的なテンポ同期
1/2 ～ 1/64     → 高速モジュレーション
1/1D～1/32D     → Dotted（1.5倍）
1/1T～1/32T     → Triplet（2/3倍）
```

**Sync = Off** の場合、フリーラン：

```
0.01 ～ 20.0 Hz → 自由な周波数設定
```

### LFO Min/Max による境界制御

LFO が 0～1 の範囲を振動する際の上下限を設定：

```
Min = 0.0, Max = 1.0   → フル範囲
Min = 0.3, Max = 0.7   → 中央付近で変動
Min = 0.9, Max = 1.0   → ほぼ動かない（微妙な変調）
```

### LFO4: レートモジュレーション

LFO1, LFO2, LFO3 の **周波数自体を時間的に変調**：

```
LFO4: Sine, 0.1 Hz, Depth = 2.0
LFO1: Sine, 1.0 Hz, Assign LFO4
       ↓
LFO1 周波数: 0.5 ～ 2.0 Hz に変動
            ↓
Morph速度が遅速に明滅
```

### LFO5: Dry/Wet レンジモジュレーション

**混ぜ込み比率（Wet/Dry）そのものを LFO で変調**：

```
Dry/Wet: 固定 50%
   ↓
LFO5: Sine, 1.0 Hz, Min=20%, Max=80%
   ↓
Wet 信号: 20% ～ 80% の間で振動
```

音像の奥行き変化、スイープリバーブ効果など。

### Recording 機能

**マウスでXYパッド上に波形を手描きして、カスタム LFO 波形を作成**：

#### Recording 開始手順

1. LFO タブで **LFO1, LFO2, LFO3** のいずれかを選択
2. **Wave = Recording** に設定
3. XYパッド上を **ドラッグして描画**
4. マウスを放すと自動的に記録完了

#### Recording グリッド

- **16×16 グリッド** — 描画精度を表示
- **各セルが1フレーム対応**
- **最大 256 フレーム記録**

#### LFO タブ機能

Recording 中に誤った LFO に上書きされることを防ぐため、タブで現在の LFO を明示：

```
[LFO1] [LFO2] [LFO3]
 ★              ← Recording 中はこのタブが選択状態
```

---

## 高度な機能

### Envelope Follower（エンベロープ追従）

**入力信号の包絡線に基づいて、フィルターパラメータを自動変調**：

| パラメータ | 役割 |
|---|---|
| **Enable** | オン/オフ |
| **Attack** | 信号上昇時の応答速度（ms）|
| **Release** | 信号低下時の応答速度（ms）|
| **Depth** | モジュレーション量（0～100%）|
| **Invert** | 反転（包絡線を反転して適用）|

**使用例：**
- ドラムを検出して、カットオフを動的に上げ下げ
- ボーカルのエネルギーに追従してリバーブ深度を変更

### Oversampling（オーバーサンプリング）

高い共鳴設定下でのエイリアシングを抑制：

| モード | サンプルレート倍率 | 遅延 | CPU コスト |
|---|---|---|---|
| **Off** | 1× | 0 | 最小 |
| **Auto** | 2× / 4× | 可変 | 中程度 |
| **2×** | 2× | ~5ms | 中程度 |
| **4×** | 4× | ~10ms | 高 |

**推奨：**
- Moog Ladder, TB-303: 最低 **2×** 推奨
- その他: **Auto** で十分

### Limiter Ceiling（出力リミッター）

最終出力段で RMS 追従式のリミッター：

```
Threshold = -0.1 dB  → ほぼ透明
Threshold = -3.0 dB  → 顕著な制限
```

フィルター共鳴ピークからの保護。

---

## パラメータ詳細説明

### フィルター A/B/C/D（共通）

#### Enable（有効化）

```
Off  → フィルター無視（重みが0）
On   → モーフブレンドに参加
```

フィルター数が1以下のとき、モーフィングは自動的に無効化されます。

#### Model（モデル選択）

0～27の28個のモデルから選択。セクション「フィルターシステム」参照。

#### Type（フィルタータイプ）

| タイプ | 説明 |
|---|---|
| **LP** | Lowpass（ローパス）— 高周波カット。最も一般的 |
| **BP** | Bandpass（バンドパス）— 特定周波数のみ通す |
| **HP** | Highpass（ハイパス）— 低周波カット |
| **Notch** | Notch（ノッチ）— 特定周波数を鋭くカット |

#### Slope（スロープ）

フィルター急峻さ（dB/oct）：

```
12 dB/oct  → 柔らか、滑らか
24 dB/oct  → バランス型
48 dB/oct  → 急峻
96 dB/oct  → 最も急峻（CPU高）
```

**注記：** 全モデルが全スロープに対応しているわけではありません。

#### Cutoff（カットオフ周波数）

20 Hz ～ 20 kHz（対数スケール）。

**LFO2** でモジュレーション可能。

#### Res / Ctrl（レゾナンス / コントロール）

0.1 ～ 10.0。フィルター共鳴度。

```
0.1   → デッド、共鳴なし
0.707 → フラット（推奨スタート）
5.0   → 強い共鳴
10.0  → 危険な領域、極端なピーク
```

**LFO3** でモジュレーション可能。
**Oversampling** で安全性向上。

### XY Morph Pad（モーフパッド）

#### Base X, Base Y

XYパッドの現在位置（0.0～1.0）。

**LFO1** で自動的に変調可能。

#### Morph Blend

モーフアルゴリズム選択。セクション「モーフィング機能」参照。

#### Cutoff Algo

カットオフ周波数の計算方法。セクション「フィルターシステム」参照。

### LFO コントロール

#### Enable（LFO有効化）

```
Off → 無操作
On  → フル動作
```

#### Wave（波形選択）

1～19の19波形から選択。セクション「LFOシステム」参照。

#### Step Mode（ステップ化）

```
Off → 滑らかな連続波形
On  → 量子化されたステップ波形
```

#### Sync（テンポシンク）

```
Off → フリーラン（0.01～20.0 Hz）
On  → DAW テンポ同期（8/1～1/64）
```

#### Rate Sync（同期周波数）

テンポ同期時の音符分割値。8/1 ～ 1/64、Dotted、Triplet 対応。

#### Rate Free（フリー周波数）

テンポシンク Off 時の周波数（Hz）。0.01～20.0 Hz。

#### Min / Max（モジュレーション範囲）

LFO の上下限を設定。Min > Max は無効。

#### Phase（位相）

LFO の開始位相（0°～360°）。

複数LFO を同じ波形で動かしながら、位相をずらす効果。

#### Fade In（フェード時間）

プラグイン読み込み後、LFO が段階的に振幅に達するまでの時間。

```
0 s    → 即座に最大振幅
5 s    → ゆっくり立ち上がり
10 s   → 非常にゆっくり
```

**用途：** 唐突な変化を避ける、スムーズなイントロ。

#### Spread（フィルター間位相差）

各フィルター（A/B/C/D）に対して LFO 位相をずらす：

```
0°    → 全フィルター同期
90°   → フィルター間で90°位相差
180°  → 対称
360°  → 一周（全パターン）
```

### LFO4: レートモジュレーション

#### Enable / Wave / Depth

LFO1/2/3 の周波数を変調する量。

#### Assign LFO1/2/3

どのLFOを変調するか選択。複数可。

### LFO5: Dry/Wet Range

#### Enable / Wave / Min / Max

Wet 信号の混合比を LFO で変調。

---

## 使用例とテクニック

### 例1: 動的カットオフスイープ

**目的：** ダブステップベースのような、Cutoff の時間的変化

**設定：**
- Filter A: Moog Ladder, LP, Cutoff 500 Hz
- Filter B: TB-303, LP, Cutoff 2 kHz
- LFO2: Sawtooth, Sync On, Rate = 1/8（8分音符）, Min=0, Max=1
- Morph Blend: Equal Power
- XY Pad: Center (0.5, 0.5)

**結果：** Moog ～ TB-303 を 8分間隔で順次スイープ。酸っぱいベース効果。

### 例2: オーガニック Pad サウンド

**目的：** ゆっくりと変化するパッド。温かく、タイムリー

**設定：**
- Filter A: Clean SVF, LP, Cutoff 1000 Hz, Res 0.707
- Filter B: Formant (Vowel), LP, Cutoff 800 Hz, Res 1.5
- Filter C: CEM3320, LP, Cutoff 1200 Hz, Res 2.0
- Filter D: CS-80, LP, Cutoff 600 Hz, Res 1.0
- Enable A/B/C/D: 全て On
- LFO1: Smooth Noise, Sync Off, Rate 0.1 Hz, Min=0.2, Max=0.8
- Morph Blend: Smoothstep
- LFO5: Sine, 0.05 Hz, Min=30%, Max=70%

**結果：** XY 位置が Smooth Noise で有機的に移動。Wet 比も緩やかに変動。

### 例3: カオス的リード

**目的：** 予測不可能で実験的な音響。異世界的

**設定：**
- Filters A/B/C/D: ランダムに異なるモデル（例：Wavefolder, Bode Shifter, Kilo All-Pass, Modal Resonator）
- LFO1: Attractor (Lorenz), Sync Off, Rate 0.3 Hz, Phase=45°
- LFO2: Billiard, Sync On, Rate = 1/16
- LFO3: Lemniscate, Sync Off, Rate 0.7 Hz, Min=1, Max=8
- Morph Blend: Radial
- Envelope Follower: On, Depth 50%, Attack 50ms, Release 500ms

**結果：** 混沌的で生き物のような音響。毎回異なる響き。

### 例4: Hand-Drawn Wave Recording

**目的：** カスタム LFO 波形で完全なコントロール

**手順：**
1. LFO1 Tab: LFO1 を選択
2. Wave: Recording に変更
3. XY Pad 上を ゆっくり描画（例：∿ 波形）
4. マウスを放す → 記録完了
5. LFO1 Enable: On

**結果：** あなたの手描き波形が、Morph パッドの位置を制御。完全カスタマイズ。

---

## トラブルシューティング

### Q: 音が出ない

**A:**
1. **フィルター Enable を確認** — 最低1つのフィルターが On か？
2. **Wet/Dry Mix を確認** — Wet が 0 dB、Dry が -∞ dB ではないか？
3. **Limiter Ceiling を確認** — 極端に低い値ではないか？ （-36 dB 以上推奨）
4. **DAW のトラック出力を確認** — ボリュームが 0 ではないか？

### Q: 非常に大きな音が出た

**A:**
1. **直ちにヘッドフォンを外してください**
2. DAW のマスターボリュームを下げる
3. Plugin のマスターゲインを -36 dB に設定
4. Limiter Ceiling を -0.1 dB に設定
5. Resonance 値が高い場合、4.0 以下に低下
6. Oversampling を 4× に設定
7. 再度、低い音量から段階的にテスト

### Q: CPU が高くなっている

**A:**
1. **Oversampling を Off に設定** （重要）
2. 使用フィルター数を削減（Enable フィルターを Off に）
3. Model を「ダミー」な低 CPU モデル（Clean SVF など）に変更
4. LFO Wave を複雑な波形（Attractor、Spirograph）から単純波形に変更
5. FilterVisualizer を最小化（UI 描画 CPU コスト削減）

### Q: Ableton Live でプラグインが見つからない

**A:**
1. VST3 フォルダを確認：`C:\Program Files\Common Files\VST3\`
2. `QuadMorphFilter.vst3` フォルダが存在するか
3. Ableton Live を再起動
4. Preferences → Plug-in Devices → Rescan

### Q: パラメータの自動化が「ジッパーノイズ」を出す

**A:**
Quad Morph Filter は SmoothedValue で自動的に補間していますが、非常に急峻な自動化曲線の場合、補間時定数（τ=50ms）が追いつかない場合があります。

**解決策：**
1. 自動化曲線のスロープを緩和
2. 特に Wet/Dry Mix の自動化を確認
3. Oversampling を有効化

---

## FAQ

### Q: フィルターモデルの違いがわからない

**A:** 各モデルの音色比較：
- **暖かさ：** Ladder（Moog, TB-303）> Analog Emulation > SVF > Digital
- **共鳴性：** TB-303 > Moog > SEM > Clean SVF > Digital
- **実験性：** Spectral（Bode Shifter, Z-Plane）> Experimental > Analog
- **用途範囲：** Clean SVF（汎用） > Formant（スペシャル） > Experimental（特化）

### Q: モーフアルゴリズムはどれを選ぶべき？

**A:**
- **Equal Power（推奨）** — 最も自然。全てのジャンル
- **Linear** — エッジが効いた遷移。エレクトロニック
- **Smoothstep** — 滑らか。アンビエント、Pad
- **Radial** — ターゲット指向。動的なサウンド

### Q: LFO の Rate を DAW テンポと同期させるには？

**A:**
1. **Sync = On**
2. Rate Sync を選択（例：1/4 = 四分音符）
3. DAW の Tempo に自動的に同期

### Q: Recording 波形を編集・削除するには？

**A:**
現在、Recording は上書き記録のみです。削除するには：
1. 新しい XY パターンで再度描画（上書き）
2. または Wave を別の波形に変更

### Q: Envelope Follower の Invert を使う場面は？

**A:**
- **通常（Invert Off）** — 信号強 → 効果強（通常のサイドチェーン）
- **反転（Invert On）** — 信号強 → 効果弱（トリガー時に効果減少）

**例：** ドラムに反応しながら、ドラム打音時に Cutoff を下げる（ダックング効果）

---

## テクニカル仕様

### DSP アーキテクチャ

```
Input
  ├─ Dry Cache
  ├─ LFO Processing
  ├─ Filter A/B/C/D Dispatch
  ├─ Weight Mix Calculation (enabledCount チェック)
  ├─ Per-Sample Update
  ├─ Parallel Filter Processing (FilterA_SVF_SIMD + TptFilter ×3)
  ├─ 4-Filter Weighted Sum
  ├─ Dry/Wet Smooth Crossfade
  ├─ Master Gain + Limiter
  └─ Output
```

### リアルタイム安全性

- **Zero Heap Allocation on Audio Thread** — processBlock 内での動的メモリ確保なし
- **ScopedNoDenormals** — デノーマル CPU スパイク抑制
- **SmoothedValue Automation** — ジッパーノイズなし
- **Lock-Free Parameter Dispatch** — スレッド安全なパラメータ更新

### パフォーマンス

| 操作 | CPU コスト | 遅延 |
|---|---|---|
| Clean SVF × 1 | ~0.5% | 0 ms |
| Moog Ladder × 1 | ~1.2% | 0 ms |
| Oversampling 2× | +1～2% | ~5 ms |
| Oversampling 4× | +3～4% | ~10 ms |
| 16-tap Visualizer | ~1% | 変数 |

### 動作確認環境

- **DAW:** Ableton Live 11 / 12
- **CPU:** Intel i7-9700K（参考）
- **RAM:** 8 GB
- **OS:** Windows 10 / 11 64-bit

---

## ライセンス

Quad Morph Filter は GPLv3 ライセンスの下で配布されています。

詳細は README.md 及び LICENSE ファイルを参照してください。

---

## 付録：キーボードショートカット

現在のバージョンでは、キーボードショートカットはサポートされていません。

全て マウス操作。

---

**本マニュアル終了。ハッピーモーフィング！🎵**
