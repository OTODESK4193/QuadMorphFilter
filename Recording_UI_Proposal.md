# Recording LFO UI 改善提案（16×16グリッド版）

## 1. UIレイアウト

### 案A: モーダルダイアログ内に16×16グリッド

Recording波形選択時に、以下のようなモーダルダイアログを表示。

```
┌─────────────────────────────────────┐
│  Recording Grid - LFO 1 (Morph)     │
├─────────────────────────────────────┤
│                                     │
│  ┌─────────────────────────────┐   │
│  │ ░░░░░░░░░░░░░░░░          │   │
│  │ ░░██░░░░░░░░░░░░░          │   │
│  │ ░░██░░░░░░░░░░░░░          │   │
│  │ ░░██████░░░░░░░░░          │   │
│  │ ░░░░░░██░░░░░░░░░          │   │
│  │ ░░░░░░░░░░░░░░░░░          │   │
│  │  ... (16×16 grid)           │   │
│  │                              │   │
│  └─────────────────────────────┘   │
│                                     │
│  [Clear]  [Auto-Trace]  [OK]       │
└─────────────────────────────────────┘

凡例:
  ░ = 未描画ピクセル
  █ = 描画済みピクセル
```

**長所**: フォーカスが明確、描画に集中できる  
**短所**: 別ウィンドウのため操作フローが増える

---

## 2. ユーザーフレンドリーな操作フロー

### Phase 1: Recording 準備モード

```
1. ユーザーが Wave = "Recording" を選択
   ↓
2. UI に "Recording Ready" インジケータ表示
   - 赤い点滅 "REC" マークが光る
   - グリッドはロック状態（描画不可）
   ↓
3. XYPad 上でマウスを押す（mouseDown）
   ↓
4. Recording 開始（グリッド描画可能）
```

### Phase 2: 描画モード（ドラッグ中）

```
マウスドラッグ中:
  - リアルタイムでピクセルが塗りつぶされる
  - 軌跡が履歴として残る
  - フレームレート 60Hz で記録（サンプリング）

実装:
  - mouseDrag() 時に、マウス座標から グリッド座標 (gx, gy) を計算
  - pixelMap[gy * 16 + gx] = 255 に設定
  - 既に描画済みなら上書き（同じピクセル複数回は1フレーム分）
```

### Phase 3: 記録完了

```
マウスをリリース（mouseUp）
  ↓
自動的に mod4[i][0..3] にフレームシーケンス変換
  ↓
Recording の再生開始
```

---

## 3. グリッド座標変換

### XYPad座標 → グリッド座標

```cpp
// XYPad の width=320px, height=290px を想定

int mouseX = e.getMouseDownX();  // 0~320
int mouseY = e.getMouseDownY();  // 0~290

// グリッド座標に変換（各セル 20×20px）
int gridX = (mouseX - 20) / 20;  // 0~15
int gridY = (mouseY - 20) / 20;  // 0~14 (少し小さい)

// 範囲チェック
if (gridX < 0 || gridX >= 16 || gridY < 0 || gridY >= 16) {
    return;  // グリッド外なら無視
}

// ピクセルマップに記録
pixelMap[gridY * 16 + gridX] = 255;  // 塗りつぶし
```

---

## 4. 記録 → 再生への変換

### ピクセルマップ → mod4 シーケンス

```cpp
void XYGridComponent::getRecordingData(
    std::array<juce::Point<float>, 2048>& buffer,
    int& len)
{
    len = 0;
    
    // 左上から右下へスキャン（Z-order）
    for (int y = 0; y < 16 && len < 2048; ++y) {
        for (int x = 0; x < 16 && len < 2048; ++x) {
            if (pixelMap[y * 16 + x] > 128) {  // 閾値128以上で塗りつぶし
                buffer[len++] = {
                    x / 15.0f,    // [0, 1] に正規化
                    y / 15.0f
                };
            }
        }
    }
    
    // len フレーム分の軌跡を記録
    // 再生時は LfoEngine で線形補間
}
```

---

## 5. 再生時の動作

### Step OFF（スムーズ再生）

```
記録フレーム: [0.0, 0.3], [0.3, 0.5], [0.5, 0.8], ...

再生位置 t=0.0~1.0 に対して Hermite 補間:
  
  float exactIdx = t * len;
  int idx1 = (int)exactIdx % len;
  int idx2 = (idx1 + 1) % len;
  float frac = exactIdx - floor(exactIdx);
  
  x = hermite(x0, x1, x2, x3, frac);  // 滑らか
  y = hermite(y0, y1, y2, y3, frac);
```

**効果**: ピクセルのカクカクした軌跡が滑らかに繋がる

### Step ON（ステップ再生）

```
記録フレーム: [0.0, 0.3], [0.3, 0.5], ...

再生:
  int idx = (int)(t * len) % len;
  x = buffer[idx].x;
  y = buffer[idx].y;
```

**効果**: ピクセル1つ1つが確実に通過（ドット打ち）

---

## 6. UI実装の詳細

### XYGridComponent クラス（新規）

```cpp
class XYGridComponent : public juce::Component {
public:
    XYGridComponent(QuadMorphFilterAudioProcessor& p);
    
    void paint(juce::Graphics& g) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
    void clear();
    void getRecordingData(
        std::array<juce::Point<float>, 2048>& buf,
        int& len);

private:
    static constexpr int GRID_SIZE = 16;
    static constexpr int CELL_SIZE = 20;  // px
    
    uint8_t pixelMap[GRID_SIZE * GRID_SIZE] = {};
    bool isRecording = false;
    
    QuadMorphFilterAudioProcessor& processor;
};
```

### XYPadComponent への統合

```cpp
class XYPadComponent : public juce::Component {
private:
    // ...既存メンバ...
    
    std::unique_ptr<juce::DialogWindow> recordingDialog;
    std::unique_ptr<XYGridComponent> gridComponent;
    
    void startRecording() {
        auto options = juce::DialogWindow::DialogOptions()
            .withTitle("Recording Grid - LFO 1")
            .withSize(400, 450);
        
        gridComponent = std::make_unique<XYGridComponent>(processor);
        recordingDialog = std::make_unique<juce::DialogWindow>(
            "Recording",
            juce::Colours::darkgrey,
            true  // closeButton
        );
        recordingDialog->setContentOwned(gridComponent.get(), false);
        recordingDialog->runModalLoop();
    }
};
```

---

## 7. ロジック実装チェックリスト

### 実装項目

- [ ] XYGridComponent クラスの実装
- [ ] mouseDrag → pixelMap 塗りつぶし処理
- [ ] getRecordingData() でシーケンス化
- [ ] LfoEngine::processSingleLfo() の Recording 再生部に Hermite 補間を追加
- [ ] Step / Smooth の分岐実装
- [ ] ダイアログUI の配置（モーダル or 常時表示）
- [ ] Clear ボタン機能
- [ ] 初期化時の pixelMap ゼロクリア

---

## 8. パフォーマンス考慮

- **ピクセルマップサイズ**: 16×16 = 256 bytes（無視できるレベル）
- **描画コスト**: 毎フレーム 256 ピクセル描画（軽い）
- **補間コスト**: Hermite 補間は毎サンプル実行（計算量O(4) で無視できる）

---

## 9. 今後の拡張案（オプション）

### 案①: Auto-Trace（自動軌跡描画）

```
[Auto-Trace] ボタン → 最後に記録された LFO軌跡を自動的にグリッドに描画
（LFO1 の波形が Sine ならサイン波を自動描画）
```

### 案②: Import/Export

```
グリッドパターンを JSON 保存 → 再利用可能に
```

### 案③: Undo/Redo

```
最後のストローク取り消し（Ctrl+Z）
```

---

## 実装手順（推奨）

1. **段階1**: XYGridComponent 基本実装（描画 + mouseDrag）
2. **段階2**: getRecordingData() でシーケンス化
3. **段階3**: LfoEngine Recording 再生部に Hermite 補間を追加
4. **段階4**: UI統合（モーダルダイアログ or XYPad内埋め込み）
5. **段階5**: テスト・ブラッシュアップ

---

**いかがでしょうか？実装を始めますか？それとも調整が必要ですか？**
