// ==========================================
// ModelCapabilities.h
// モデルごとに有効なスロープ・タイプを定義
// ==========================================
#pragma once
#include <tuple>

// 戻り値: { maxSlopeIdx, LP, BP, HP, Notch }
// maxSlopeIdx: 0=1択のみ, 1=0-1, 2=0-1-2, 3=全て
static inline std::tuple<int, bool, bool, bool, bool> getModelCaps(int m)
{
    switch (m)
    {
        // ===== TB-303 Diode Ladder =====
        // Slope コンボを Accent コンボとして使用
        // 0=Accent:Off, 1=Accent:Low, 2=Accent:High
        // 物理特性: 18dB/oct 的挙動、LP のみ
    case 2:  return { 2, true,  false, false, false };

          // ===== Ladder系 =====
    case 1:  return { 1, true,  true,  true,  false }; // Moog: 12/24dB
    case 12: return { 1, true,  true,  true,  false }; // CEM3320: 12/24dB, LP/BP/HP (チップ能力フル活用)
    case 13: return { 1, true,  false, false, false }; // SSM2040: 12/24dB, LPのみ
    case 15: return { 1, true,  false, false, false }; // Jupiter: 12/24dB, LPのみ

           // ===== SVF系 (固定/制限あり) =====
    case 3:  return { 1, true,  true,  true,  true };  // SEM: 12/24dB (実機は2-pole固定; 24dBはダブルSEM拡張)
    case 5:  return { 0, false, true,  false, false }; // Formant (Vowel): BP固定（フォルマント特徴はBPのみ）
                                                        //   Slope固定12dB / Cutoff=母音位置 / Res=フォルマントQ
    case 10: return { 0, true,  true,  true,  true };  // Reverb FDN: 12dBのみ
    case 21: return { 2, true,  false, false, false }; // Vactrol LPG: LP, Slope=Attackタイム(Fast/Med/Slow)
                                                        //   Cutoff=CV(開口量) / Res=Decayタイム
    case 22: return { 2, true,  true,  true,  true };  // Modal Resonator: Slope=Q(Low/Mid/High)
                                                        //   Cutoff=基音 / Res=非調波性(Inharmonicity)
    case 24: return { 0, true,  true,  true,  true };  // Bode: 12dBのみ
    case 25: return { 0, true,  true,  true,  true };  // Z-Plane: 12dBのみ
    case 27: return { 3, true,  false, false, false }; // Nyquist Anti-alias: LP, Slope=Stage数(2/4/6/8段)
                                                        //   Cutoff=カットオフ / Res=Q(段のキャラクター)

           // ===== 特殊 =====
    case 14: return { 3, true,  false, true,  false }; // CS-80: LP/HPのみ
    case 23: return { 3, true,  false, true,  false }; // Waveguide: LP/HP
    case 26: return { 3, true,  false, true,  false }; // Phased Array: LP/HP

           // ===== デフォルト: 全オプション有効 =====
    default: return { 3, true,  true,  true,  true };
    }
}