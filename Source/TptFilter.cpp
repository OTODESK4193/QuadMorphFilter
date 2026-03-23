// ==========================================
// TptFilter.cpp
// ==========================================
#include "TptFilter.h"
#include <cmath>

TptFilter::TptFilter()
{
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    cutoff.setCurrentAndTargetValue(1000.0f);
    resonance.setCurrentAndTargetValue(0.707f);
}

void TptFilter::prepare(double newSampleRate, int /*samplesPerBlock*/, int numChannels)
{
    sampleRate = newSampleRate;
    maxChannels = juce::jmin(numChannels, 2);
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    reset();
}

void TptFilter::reset()
{
    for (int stage = 0; stage < 8; ++stage) {
        for (int ch = 0; ch < 2; ++ch) {
            s1[stage][ch] = 0.0f; s2[stage][ch] = 0.0f;
            for (int pole = 0; pole < 4; ++pole) zdfState[stage][ch][pole] = 0.0f;
            combWriteIdx[stage][ch] = 0;
            for (int i = 0; i < 4096; ++i) combBuffer[stage][ch][i] = 0.0f;
        }
    }
    for (int stage = 0; stage < 16; ++stage) {
        for (int ch = 0; ch < 2; ++ch) ap_s[stage][ch] = 0.0f;
    }
    for (int ch = 0; ch < 2; ++ch) {
        rmsIn[ch] = 0.0f; rmsOut[ch] = 0.0f; agcGain[ch] = 1.0f;
        srrPhase[ch] = 0.0f; srrHeld[ch] = 0.0f; ap_out_prev[ch] = 0.0f;
        for (int f = 0; f < 3; ++f) { form_s1[f][ch] = 0.0f; form_s2[f][ch] = 0.0f; }
    }
}

void TptFilter::setModel(int newModel) { filterModel = newModel; }
void TptFilter::setCutoff(float newCutoff) { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff)); }
void TptFilter::setResonance(float newResonance) { resonance.setTargetValue(juce::jmax(0.1f, newResonance)); }
void TptFilter::setType(int newType) { filterType = newType; }

void TptFilter::setSlope(int index)
{
    slopeIdx = index;
    if (filterModel == 5) {
        currentStages = 1;
    }
    else if (filterModel == 8) { // 【追加】Phaser用拡張段数 (2, 4, 8, 16段)
        currentStages = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 8 : 16;
    }
    else if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 6 || filterModel == 7 || filterModel == 9) {
        currentStages = (index == 0) ? 1 : (index == 1) ? 2 : (index == 2) ? 4 : 8;
    }
    else {
        currentStages = (index == 0) ? 1 : (index == 1) ? 1 : (index == 2) ? 2 : 4;
    }
}

void TptFilter::updateCoefficients()
{
    float currentCutoff = cutoff.getNextValue();
    float currentRes = resonance.getNextValue();

    if (filterModel == 5) {
        float v = juce::jlimit(0.0f, 4.0f, juce::jmap(std::log10(currentCutoff), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
        int idx = (int)v; float frac = v - idx;
        if (idx >= 4) { idx = 3; frac = 1.0f; }
        float f1_map[5] = { 730.f, 270.f, 300.f, 530.f, 400.f }; float f2_map[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f }; float f3_map[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
        float f_arr[3] = { f1_map[idx] + (f1_map[idx + 1] - f1_map[idx]) * frac, f2_map[idx] + (f2_map[idx + 1] - f2_map[idx]) * frac, f3_map[idx] + (f3_map[idx + 1] - f3_map[idx]) * frac };
        for (int f = 0; f < 3; ++f) {
            float wd = juce::MathConstants<float>::pi * f_arr[f] / (float)sampleRate; form_g[f] = std::tan(wd); form_R[f] = 1.0f / (2.0f * currentRes); form_h[f] = 1.0f / (1.0f + 2.0f * form_R[f] * form_g[f] + form_g[f] * form_g[f]);
        }
    }
    else {
        float wd = juce::MathConstants<float>::pi * currentCutoff / (float)sampleRate;
        g = std::tan(wd); ladderG = g / (1.0f + g);
        if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 6 || filterModel == 7 || filterModel == 9) {
            float adjustedRes = currentRes;
            if (filterModel != 7 && currentStages > 1) adjustedRes = currentRes * std::pow(0.6f, std::log2((float)currentStages));
            R = 1.0f / (2.0f * adjustedRes); h = 1.0f / (1.0f + 2.0f * R * g + g * g);
        }
        else if (filterModel == 1) {
            ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, 4.0f); if (currentStages > 1) ladderRes *= std::pow(0.7f, std::log2((float)currentStages));
        }
        else if (filterModel == 2) {
            ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, 15.0f); if (currentStages > 1) ladderRes *= std::pow(0.7f, std::log2((float)currentStages));
        }
    }
}

void TptFilter::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    float* writePointers[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch) writePointers[ch] = buffer.getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i) {
        updateCoefficients();
        for (int ch = 0; ch < numChannels; ++ch) writePointers[ch][i] = processSample(ch, writePointers[ch][i]);
    }
}

float TptFilter::processSample(int channel, float x)
{
    float comp = 1.0f;
    if (filterModel == 0 || filterModel == 4 || filterModel == 5 || filterModel == 6 || filterModel == 9) comp = 1.0f + resonance.getCurrentValue() * 0.1f;
    else if (filterModel == 1) comp = 1.0f + 0.5f * ladderRes;
    else if (filterModel == 2) comp = 1.0f + 0.2f * ladderRes;
    else if (filterModel == 7) comp = 1.0f + resonance.getCurrentValue() * 0.15f; // MS-20
    x *= comp;

    if (filterModel == 4) {
        float targetSR = cutoff.getCurrentValue(); float phaseInc = targetSR / sampleRate; srrPhase[channel] += phaseInc;
        if (srrPhase[channel] >= 1.0f) {
            srrPhase[channel] -= std::floor(srrPhase[channel]); float bits = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 16.0f, 2.0f);
            float steps = std::pow(2.0f, bits); srrHeld[channel] = std::round(x * steps) / steps;
        }
        x = srrHeld[channel];
    }

    float envCoefIn = 0.005f;
    rmsIn[channel] = (1.0f - envCoefIn) * rmsIn[channel] + envCoefIn * (x * x);
    float out = x;

    if (filterModel == 0 || filterModel == 4) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp; s2[stage][channel] = g * bp + lp;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 1) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float s1_ = zdfState[stage][channel][0]; float s2_ = zdfState[stage][channel][1]; float s3_ = zdfState[stage][channel][2]; float s4_ = zdfState[stage][channel][3];
            float S1 = s1_ / (1.0f + g); float S2 = s2_ / (1.0f + g); float S3 = s3_ / (1.0f + g); float S4 = s4_ / (1.0f + g);
            float sigma = ladderG * ladderG * ladderG * S1 + ladderG * ladderG * S2 + ladderG * S3 + S4;
            float u = std::tanh((out - ladderRes * sigma) / (1.0f + ladderRes * ladderG * ladderG * ladderG * ladderG));
            float v1 = (u - s1_) * ladderG; float y1 = v1 + s1_; zdfState[stage][channel][0] = s1_ + 2.0f * v1;
            float v2 = (y1 - s2_) * ladderG; float y2 = v2 + s2_; zdfState[stage][channel][1] = s2_ + 2.0f * v2;
            float v3 = (y2 - s3_) * ladderG; float y3 = v3 + s3_; zdfState[stage][channel][2] = s3_ + 2.0f * v3;
            float v4 = (y3 - s4_) * ladderG; float y4 = v4 + s4_; zdfState[stage][channel][3] = s4_ + 2.0f * v4;
            if (slopeIdx == 0) { if (filterType == 0) out = y2; else if (filterType == 1) out = 2.0f * (y1 - y2); else if (filterType == 2) out = out - 2.0f * y1 + y2; else out = y2 + (out - 2.0f * y1 + y2); }
            else { if (filterType == 0) out = y4; else if (filterType == 1) out = 4.0f * (y2 - y4); else if (filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4; else out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4); }
        }
    }
    else if (filterModel == 2) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float fb = std::tanh(out - ladderRes * zdfState[stage][channel][3]);
            float v1 = (fb - zdfState[stage][channel][0]) * ladderG; float y1 = v1 + zdfState[stage][channel][0]; zdfState[stage][channel][0] = y1 + v1; y1 = std::tanh(y1);
            float v2 = (y1 - zdfState[stage][channel][1]) * ladderG; float y2 = v2 + zdfState[stage][channel][1]; zdfState[stage][channel][1] = y2 + v2; y2 = std::tanh(y2);
            float v3 = (y2 - zdfState[stage][channel][2]) * ladderG; float y3 = v3 + zdfState[stage][channel][2]; zdfState[stage][channel][2] = y3 + v3; y3 = std::tanh(y3);
            float v4 = (y3 - zdfState[stage][channel][3]) * ladderG; float y4 = v4 + zdfState[stage][channel][3]; zdfState[stage][channel][3] = y4 + v4;
            if (y4 > 0.0f) y4 *= 1.1f; y4 = std::tanh(y4);
            if (slopeIdx == 0) { if (filterType == 0) out = y2; else if (filterType == 1) out = y1 - y2; else if (filterType == 2) out = out - y1; else out = y2 + (out - y1); }
            else { if (filterType == 0) out = y4; else if (filterType == 1) out = y2 - y4; else if (filterType == 2) out = out - y2; else out = y4 + (out - y2); }
        }
    }
    else if (filterModel == 3) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float drivenOut = std::tanh(out * 1.2f);
            float hp = (drivenOut - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = std::tanh(g * hp + bp); s2[stage][channel] = std::tanh(g * bp + lp);
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 5) {
        float out_formant = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
        for (int stage = 0; stage < currentStages; ++stage) {
            float stage_in = out; out_formant = 0.0f;
            for (int f = 0; f < 3; ++f) {
                float hp = (stage_in - (2.0f * form_R[f] + form_g[f]) * form_s1[f][channel] - form_s2[f][channel]) * form_h[f];
                float bp = form_g[f] * hp + form_s1[f][channel]; float lp = form_g[f] * bp + form_s2[f][channel];
                form_s1[f][channel] = form_g[f] * hp + bp; form_s2[f][channel] = form_g[f] * bp + lp;
                float bandOut = bp; if (filterType == 0) bandOut = lp; else if (filterType == 1) bandOut = bp; else if (filterType == 2) bandOut = hp; else bandOut = lp + hp;
                out_formant += bandOut * gains[f];
            }
            out = out_formant;
        }
    }
    else if (filterModel == 6) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float delaySamples = sampleRate / juce::jlimit(20.0f, 20000.0f, cutoff.getCurrentValue());
            int dInt = (int)delaySamples; float dFrac = delaySamples - dInt;
            int readIdx1 = combWriteIdx[stage][channel] - dInt; if (readIdx1 < 0) readIdx1 += 4096;
            int readIdx2 = readIdx1 - 1; if (readIdx2 < 0) readIdx2 += 4096;
            float delayed = combBuffer[stage][channel][readIdx1] + dFrac * (combBuffer[stage][channel][readIdx2] - combBuffer[stage][channel][readIdx1]);
            float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f); if (filterType == 1 || filterType == 3) fb = -fb;
            float combOut = 0.0f;
            if (filterType == 0 || filterType == 1) {
                float toBuffer = std::tanh(out + delayed * fb); combBuffer[stage][channel][combWriteIdx[stage][channel]] = toBuffer; combOut = toBuffer;
            }
            else {
                combBuffer[stage][channel][combWriteIdx[stage][channel]] = out; combOut = out + delayed * fb;
            }
            combWriteIdx[stage][channel] = (combWriteIdx[stage][channel] + 1) % 4096;
            out = combOut;
        }
    }
    else if (filterModel == 7) { // 【追加】MS-20 (Screaming)
        float ms_res = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 2.5f);
        float r_ms = 1.0f / (2.0f * ms_res + 0.1f);
        float h_ms = 1.0f / (1.0f + 2.0f * r_ms * g + g * g);
        for (int stage = 0; stage < currentStages; ++stage) {
            float clip_s2 = s2[stage][channel];
            if (clip_s2 > 0.0f) clip_s2 *= 1.8f; // 強烈な非対称クリップ
            clip_s2 = std::tanh(clip_s2);
            float hp = (out - (2.0f * r_ms + g) * s1[stage][channel] - clip_s2) * h_ms;
            float bp = g * hp + s1[stage][channel];
            float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = std::tanh(g * hp + bp);
            s2[stage][channel] = std::tanh(g * bp + lp);
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 8) { // 【追加】All-Pass Phaser
        float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f);
        if (filterType == 1 || filterType == 3) fb = -fb; // ノッチの位相切替
        float in_ap = out + fb * ap_out_prev[channel];
        in_ap = std::tanh(in_ap); // フィードバックサチュレーション

        for (int stage = 0; stage < currentStages; ++stage) {
            float v = (in_ap - ap_s[stage][channel]) * ladderG;
            float lp = v + ap_s[stage][channel];
            ap_s[stage][channel] = lp + v;
            in_ap = 2.0f * lp - in_ap; // 1-Pole All-Pass
        }
        ap_out_prev[channel] = in_ap;
        out = (out + in_ap) * 0.5f; // Dry/Wet ミックスでノッチ生成
    }
    else if (filterModel == 9) { // 【追加】Wavefolder
        float fold_gain = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 1.0f, 10.0f);
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel];
            float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp;
            s2[stage][channel] = g * bp + lp;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
        out = std::sin(out * fold_gain); // Buchlaスタイル Sine Folder
    }

    float envCoefOut = 0.005f;
    rmsOut[channel] = (1.0f - envCoefOut) * rmsOut[channel] + envCoefOut * (out * out);

    float targetGain = 1.0f;
    if (rmsOut[channel] > 1e-8f) {
        targetGain = std::sqrt((rmsIn[channel] + 1e-8f) / (rmsOut[channel] + 1e-8f));
        targetGain = juce::jlimit(0.1f, 15.0f, targetGain);
    }
    agcGain[channel] = (1.0f - 0.0005f) * agcGain[channel] + 0.0005f * targetGain;

    return out * agcGain[channel];
}

float TptFilter::getMagnitudeForFrequency(float frequency) const
{
    float fc = cutoff.getCurrentValue();
    float res = resonance.getCurrentValue();
    float w = frequency / juce::jlimit(20.0f, 20000.0f, fc);
    float w2 = w * w;
    float mag = 1.0f;

    if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 9) {
        float adjustedRes = res;
        if (currentStages > 1) adjustedRes = res * std::pow(0.6f, std::log2((float)currentStages));
        float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, currentStages);
    }
    else if (filterModel == 5) {
        float v = juce::jlimit(0.0f, 4.0f, juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
        int idx = (int)v; float frac = v - idx; if (idx >= 4) { idx = 3; frac = 1.0f; }
        float f1_m[5] = { 730.f, 270.f, 300.f, 530.f, 400.f }; float f2_m[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f }; float f3_m[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
        float f_arr[3] = { f1_m[idx] + (f1_m[idx + 1] - f1_m[idx]) * frac, f2_m[idx] + (f2_m[idx + 1] - f2_m[idx]) * frac, f3_m[idx] + (f3_m[idx + 1] - f3_m[idx]) * frac };
        float mag_sum = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
        for (int f = 0; f < 3; ++f) {
            float w_f = frequency / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
            float d_f = 1.0f / juce::jlimit(0.1f, 10.0f, res);
            float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
            if (filterType == 1) m_f *= w_f; else if (filterType == 2) m_f *= w_f * w_f; else if (filterType == 3) m_f *= std::abs(1.0f - w_f * w_f);
            mag_sum += m_f * gains[f];
        }
        return mag_sum;
    }
    else if (filterModel == 6) {
        float delaySamples = sampleRate / juce::jlimit(20.0f, 20000.0f, fc);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f); if (filterType == 1 || filterType == 3) fb = -fb;
        float wD = 2.0f * juce::MathConstants<float>::pi * frequency * (delaySamples / sampleRate);
        float m = (filterType == 0 || filterType == 1) ? (1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD))) : std::sqrt(1.0f + fb * fb + 2.0f * fb * std::cos(wD));
        return std::pow(m, currentStages);
    }
    else if (filterModel == 7) { // MS-20
        float ms_res = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 2.5f);
        float d = 1.0f / (ms_res + 0.1f);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, currentStages);
    }
    else if (filterModel == 8) { // All-Pass Phaser
        float phi = -2.0f * std::atan(frequency / juce::jlimit(20.0f, 20000.0f, fc));
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        if (filterType == 1 || filterType == 3) fb = -fb;
        float c_ap = std::cos(currentStages * phi);
        float s_ap = std::sin(currentStages * phi);
        float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
        float real_yap = (c_ap - fb) / den2;
        float imag_yap = s_ap / den2;
        float real_out = 0.5f * (1.0f + real_yap);
        float imag_out = 0.5f * imag_yap;
        return std::sqrt(real_out * real_out + imag_out * imag_out);
    }
    else {
        float r_scale = (filterModel == 1) ? 4.0f : 15.0f;
        float r_val = juce::jmap(res, 0.1f, 10.0f, 0.0f, r_scale);
        if (currentStages > 1) r_val *= std::pow(0.7f, std::log2((float)currentStages));
        float real_p = std::pow(1.0f - w2, 2.0f) - ((filterModel == 1) ? 4.0f : 3.5f) * w2 + r_val;
        float imag_p = ((filterModel == 1) ? 4.0f : 3.5f) * w * (1.0f - w2);
        mag = 1.0f / std::sqrt(real_p * real_p + imag_p * imag_p);
        if (slopeIdx == 0) {
            if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        }
        else {
            if (filterModel == 1) {
                if (filterType == 1) mag *= w2; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w2);
            }
            else {
                if (filterType == 1) mag *= w2 * w; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w);
            }
        }
        return std::pow(mag, currentStages);
    }
}