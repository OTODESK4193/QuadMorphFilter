// ==========================================
// ModelCapabilities.h
// モデルごとに有効なスロープ・タイプを定義
// ==========================================
#pragma once
#include <tuple>

// 戻り値: { maxSlopeIdx, LP, BP, HP, Notch }
// maxSlopeIdx: 0=12dBのみ, 1=12/24dB, 2=12/24/48dB, 3=全て
static inline std::tuple<int, bool, bool, bool, bool> getModelCaps(int m)
{
    switch (m)
    {
        // ===== Ladder系 =====
    case 1:  return { 1, true,  true,  true,  false }; // Moog: 12/24, LP/BP/HP
    case 2:  return { 1, true,  false, false, false };  // TB-303: 12/24, LPのみ
    case 12: return { 1, true,  true,  true,  false };  // Prophet: 12/24, LP/BP/HP
    case 13: return { 1, true,  false, false, false };  // SSM2040: 12/24, LPのみ
    case 15: return { 1, true,  false, false, false };  // Jupiter: 12/24, LPのみ

           // ===== SVF系 (固定/制限あり) =====
    case 5:  return { 0, true,  true,  true,  true };   // Formant: 12dBのみ
    case 10: return { 0, true,  true,  true,  true };   // Reverb FDN: 12dBのみ
    case 21: return { 0, true,  false, false, false };  // Vactrol LPG: 12dB, LPのみ
    case 22: return { 0, true,  true,  true,  true };   // Modal: 12dBのみ
    case 24: return { 0, true,  true,  true,  true };   // Bode: 12dBのみ
    case 25: return { 0, true,  true,  true,  true };   // Z-Plane: 12dBのみ
    case 27: return { 0, true,  false, false, false };  // Nyquist: 12dB, LPのみ

           // ===== 特殊 =====
    case 14: return { 3, true,  false, true,  false };  // CS-80: LP/HPのみ
    case 23: return { 3, true,  false, true,  false };  // Waveguide: LP/HP
    case 26: return { 3, true,  false, true,  false };  // Phased Array: LP/HP

           // ===== デフォルト: 全オプション有効 =====
           // 0(Clean SVF), 3(SEM), 4(Bitcrush), 6(Comb),
           // 7(MS-20), 8(AllPass), 9(Wavefolder), 11(KiloAP),
           // 16(Wasp), 17-20(Classical)
    default: return { 3, true,  true,  true,  true };
    }
}