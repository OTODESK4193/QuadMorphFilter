# Quad Morph Filter — User Manual English Edition

**Version: v1.0.0**  
**Last Updated: June 6, 2026**  
**Developer: OTODESK**

---

## Table of Contents

1. [Introduction](#introduction)
2. [Critical Safety Warning](#critical-safety-warning)
3. [Installation](#installation)
4. [Basic Operation](#basic-operation)
5. [Filter System](#filter-system)
6. [Morphing Features](#morphing-features)
7. [LFO System](#lfo-system)
8. [Advanced Features](#advanced-features)
9. [Parameter Reference](#parameter-reference)
10. [Use Cases & Techniques](#use-cases--techniques)
11. [Troubleshooting](#troubleshooting)
12. [FAQ](#faq)
13. [Technical Specifications](#technical-specifications)
14. [License](#license)

---

## Introduction

**Quad Morph Filter** is a high-performance VST3 plugin featuring **28 meticulously modeled filter algorithms** that morph together in real-time using an intuitive XY pad interface. Whether you're exploring analog warmth (Moog Ladder, TB-303), digital precision (Butterworth, Elliptic), or experimental territory (Bode Frequency Shifter, Z-Plane morphing), Quad Morph Filter opens boundless sonic possibilities.

Blend up to **4 different filters simultaneously** with equal-power blending algorithms, modulate them with 19 LFO waveforms, record custom XY patterns by hand, and achieve professional results with extreme real-time stability.

### Key Features

- **28 Premium Filter Models** — Ladder, SVF, Analog Emulation, Digital Precision, Spectral, and Experimental architectures
- **Real-Time XY Morphing Engine** — Equal-power blending of up to 4 filters with zero artifacts
- **19 LFO Waveforms** — From classic sine to chaotic attractors
- **Real-Time Frequency Response Visualizer** — 1024-point FFT display showing live filter changes
- **XY Pattern Recording** — Draw custom LFO patterns by hand on the morph pad
- **Ableton Live Optimized** — Strict real-time safety, zero heap allocation on audio thread

---

## Critical Safety Warning

### ⚠️ Hearing Protection

**This plugin can generate LOUD audio output. Hearing protection is YOUR responsibility.**

- Always monitor output levels carefully when using speakers or headphones
- **Start with LOW volume and gradually increase**
- **NEVER wear headphones at maximum volume**
- Take regular breaks during extended listening sessions
- Prolonged exposure to loud sound (≥85 dB SPL) causes permanent hearing loss
- By using this plugin, you accept FULL responsibility for your hearing safety and equipment protection

### Operating Cautions

- Initialize with master volume set low to protect against unexpected peaks
- High resonance settings (Res > 8.0) can produce sharp peaks at high frequencies
- Enable oversampling (2× minimum) when using Moog Ladder or TB-303 models with high Res values

---

## Installation

### VST3 Plugin Installation

1. Download the latest `QuadMorphFilter.vst3` from the [Releases](https://github.com/OTODESK4193/QuadMorphFilter/releases/latest) page
2. Copy the `.vst3` folder to your VST3 plugin directory:
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. Rescan plugins in your DAW (Ableton Live, etc.)

### Standalone Application

A standalone executable is also provided. Simply run `Quad-Morph Filter.exe` — no DAW required.

### System Requirements

- **Windows 10 / 11 (64-bit)**
- **AVX2-capable CPU**
- **Tested DAW: Ableton Live 11 / 12**
- **Other DAWs (Reaper, Studio One, etc.) are untested**

---

## Basic Operation

### Interface Overview

```
┌─────────────────────────────────────────────┐
│ Filter Panel                                │
│  ├─ Filter A/B/C/D (Enable, Model, Type)  │
│  ├─ Cutoff / Resonance Sliders             │
│  └─ LFO Cut/Res Buttons                    │
├─────────────────────────────────────────────┤
│ XY Morph Pad                                │
│  ├─ Morph Area (A/B/C/D at corners)       │
│  ├─ LFO Tabs (LFO1/2/3 Selection)         │
│  └─ Recording Grid Overlay                 │
├─────────────────────────────────────────────┤
│ Visualizer & E-Button                       │
│  ├─ Real-Time Frequency Response Graph     │
│  └─ E-Button (Background Color Randomizer) │
├─────────────────────────────────────────────┤
│ LFO Control Section                         │
│  ├─ LFO1/2/3 Parameters                   │
│  └─ LFO4/5 Specialized Controls            │
├─────────────────────────────────────────────┤
│ Master Controls                             │
│  ├─ Output Gain / Dry-Wet Mix             │
│  ├─ Oversampling / Limiter Ceiling        │
│  └─ Envelope Follower                      │
└─────────────────────────────────────────────┘
```

### Mouse Operations

| Action | Target | Effect |
|---|---|---|
| **Drag** | XY Pad | Change morph position |
| **Left Click** | E-Button | Randomize background color |
| **Right Click** | E-Button | Reset to default color |
| **Slider Drag** | Parameter | Smooth value change |
| **Ctrl + Click** | Slider | Reset to default value |

---

## Filter System

### 4-Filter Architecture

Quad Morph Filter operates **4 independent filters (A, B, C, D)** simultaneously, blending them in real-time to create complex frequency responses.

Each filter controls:

- **Enable** — On/Off toggle
- **Model** — 28 models (0–27)
- **Type** — LP / BP / HP / Notch
- **Slope** — 12 / 24 / 48 / 96 dB/oct (model-dependent)
- **Cutoff** — 20 Hz to 20 kHz (log scale)
- **Res/Ctrl** — 0.1 to 10.0 (resonance emphasis)

### Filter Model Categories

#### Ladder Filters (5 Models)

Iconic 4-pole analog ladder designs. Strong resonance character.

- **Model 1: Moog Ladder** — Most famous analog filter. Warm, legendary sound
- **Model 2: TB-303 Diode** — Defines acid bass. Accent On/Off/High mode
- **Model 12: CEM3320 (Prophet)** — Curtis classic. Soft saturation
- **Model 13: SSM 2040 (Oberheim)** — Oberheim iconic. Asymmetric clipping
- **Model 15: Jupiter (Roland)** — SH-101/Jupiter-8 recreation. Soft clip

#### SVF Filters (9 Models)

High-precision State Variable designs. Multiple types per model. Maximum flexibility.

- **Model 0: Clean SVF** — Most transparent, neutral, recommended starting point
- **Model 3: SEM (Oberheim)** — Modern Oberheim. Soft saturation
- **Model 5: Formant (Vowel)** — 3 parallel SVFs. Vowel formant mimicry
- **Model 6: Comb Filter** — Delay-line based. Metallic resonance
- **Model 9: Wavefolder** — Sine expansion with ADAA. Remarkably warm

#### Analog Emulation (4 Models)

Real circuit simulation. Complex, organic character.

- **Model 10: FDN Reverb** — 4-delay feedback network. Reverb capability
- **Model 21: Vactrol LPG** — LED-driven optical gate. Vintage warmth
- **Model 22: Modal Resonator** — 8 formant bands. Speech-like textures
- **Model 27: Nyquist Anti-alias** — High-frequency limiter. Aliasing suppression

#### Digital Precision (4 Models)

Mathematically perfect IIR designs. Electronic aesthetic.

- **Model 17: Butterworth** — Maximally flat passband. Neutral
- **Model 18: Chebyshev** — Passband ripple. Edge and presence
- **Model 19: Bessel** — Linear phase. Minimal ringing
- **Model 20: Elliptic** — Equiripple pass/stopband. Steep edge

#### Spectral (5 Models)

Frequency-domain processing. Advanced sound design.

- **Model 11: Kilo All-Pass** — 16-stage all-pass. Dense phase manipulation
- **Model 24: Bode Frequency Shifter** — Hilbert transform. ±5 kHz frequency shift
- **Model 25: Z-Plane 2D Morph** — 7-biquad pole placement. Z-plane morphing
- **Model 26: Phased Array** — Parallel all-pass. Stereo decoration

#### Experimental (1 Model)

Exotic sound processing.

- **Model 8: All-Pass Phaser** — 16-stage all-pass. Subtle phasing

### Cutoff Algorithms

Three methods to control cutoff frequency via XY position:

#### Absolute (Direct Frequency Mapping)

```
20 Hz ────────────────────── 20 kHz
Left (0)                Right (1)
```

Intuitive, predictable. Left = low frequencies, Right = high frequencies.

#### Relative (±Octave Range)

```
      Center (632 Hz)
Left: -4 oct    Right: +4 oct
```

Musical and symmetric. Center = base frequency, ±4 octaves range.

#### Zone (Asymmetric Scaling)

```
Left: 20Hz ──── 200Hz  |  Right: 200Hz ──── 20kHz
Narrow range          Wide range
```

Combines fine control with broad sweep.

---

## Morphing Features

### The XY Pad

A 2D interface controlling **blend weights** of 4 filters positioned at the corners:

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

- **Drag to morph** between filters
- **Watch the frequency response change** in real-time
- **Modulate with LFO1** for automatic sweeping

### Morph Blend Algorithms

Four mathematical approaches to blending 4 filters:

#### Equal Power

$$w_A^2 + w_B^2 + w_C^2 + w_D^2 = 1 \text{ (always)}$$

**Characteristic:** Constant perceived loudness. Acoustic energy preserved.

**Use:** Recommended for nearly all applications. Smooth, natural morphing.

#### Linear

$$w_i = \max(0, 1 - |d_i|)$$

**Characteristic:** Bilinear interpolation. Each filter 0.25 at center.

**Use:** Sharper transitions. Electronic character.

#### Smoothstep (S-Curve)

$$w_i = \text{smoothstep}(0, 1, 1 - |d_i|)$$

**Characteristic:** S-curve bias. Distant filters weighted more.

**Use:** Organic transitions. Ambient, pads.

#### Radial (Inverse Distance Squared)

$$w_i \propto 1 / d_i^2$$

**Characteristic:** Nearest filter dominates. Steep distance decay.

**Use:** Targeted morphing. Fast switches.

### LFO1 Morph Modulation

**LFO1** automatically sweeps the XY pad position through time:

```
LFO1 Wave    →   Morph Pad Motion
Sine         →   Smooth circular orbit
Sawtooth     →   Ramp sweep
Random       →   Unpredictable jumps
Spirograph   →   Complex trajectory
```

**Example:**
- LFO1: Sine, 0.5 Hz, Min=0, Max=1
- Morph pad traces a slow, smooth circle over 4-filter area

---

## LFO System

### LFO1, LFO2, LFO3 Roles

| LFO | Purpose | Modulates |
|---|---|---|
| **LFO1** | Morph Modulation | XY pad position (filter weights) |
| **LFO2** | Cutoff Modulation | Cutoff frequency of all 4 filters |
| **LFO3** | Resonance Modulation | Resonance of all 4 filters |

### 19 Waveforms

#### Basic (4)

- **Sine** — Smooth, musical, most natural
- **Sawtooth** — Bright ramp. Sweep character
- **Pulse** — Bright, step-like
- **Triangle** — Intermediate. Linear ramps

#### Random Family (4)

- **Random 1** — Step-wise jumps. Unpredictable
- **Random 2** — Smooth random. Organic
- **Noise** — White noise. Frenzied, chaotic
- **Smooth Noise** — Hermite-filtered. Smoother organic feel

#### Geometric Patterns (6)

Complex, often unpredictable trajectories. Perfect for experimental design.

- **Spirograph** — Epitrochoid curves
- **Torus Knot** — 3D knot unwrapping
- **Lissajous** — 3:2 frequency ellipse
- **Spiral** — Logarithmic spiral
- **Star** — Radial star rays
- **Rose** — Polar rose (Rhodonea)

#### Dynamic (3)

Physics-based modulation. Interactive and evolving.

- **Billiard** — Ball bouncing. Collision physics
- **Polygon** — N-sided polygon. Geometric steps
- **Attractor** — Lorenz / Henon chaos. Unpredictable

#### Special (1)

- **Recording** — Hand-drawn custom pattern

### Tempo Sync

**Sync = On:** LFO frequency locks to DAW tempo

```
Divisions:
8/1, 4/1, 2/1, 1/1  → Standard rhythmic values
1/2 ～ 1/64         → Fast subdivisions
1/1D～1/32D         → Dotted (1.5× length)
1/1T～1/32T         → Triplet (2/3× length)
```

**Sync = Off:** Freerun mode

```
0.01 ～ 20.0 Hz → Independent frequency
```

### LFO Min/Max Range

Control the upper and lower boundaries of LFO sweep:

```
Min=0.0, Max=1.0   → Full range
Min=0.3, Max=0.7   → Central region only
Min=0.9, Max=1.0   → Almost stationary (subtle)
```

### LFO4: Rate Modulation

Modulate the frequency of LFO1, LFO2, LFO3:

```
LFO4: Sine, 0.1 Hz, Depth=2.0
LFO1: Sine, 1.0 Hz, Assign LFO4
      ↓
LFO1 speed varies: 0.5 ～ 2.0 Hz
      ↓
Morph speed tremolo
```

Creates "living," breathing modulation.

### LFO5: Dry/Wet Range Modulation

Modulate the wet/dry mix ratio itself via LFO5:

```
Dry/Wet: locked at 50%
   ↓
LFO5: Sine, 1.0 Hz, Min=20%, Max=80%
   ↓
Wet signal oscillates: 20% ～ 80%
```

Creates depth sweep, reverb swells, and spatial motion.

### Recording Custom Patterns

**Draw a custom LFO waveform by hand on the XY pad:**

#### Steps

1. Select **LFO tab** (LFO1, LFO2, or LFO3)
2. Set **Wave = Recording**
3. **Drag on XY pad** to draw your pattern
4. **Release mouse** → automatically records

#### Recording Grid

- **16×16 grid** showing snap points
- **Max 256 samples** per pattern
- **Repeats smoothly** after recording ends

#### LFO Tab System

Prevents accidental overwriting during recording:

```
[LFO1] [LFO2] [LFO3]
 ★              ← Currently recording LFO
```

---

## Advanced Features

### Envelope Follower

Auto-modulate filter parameters based on input signal amplitude:

| Parameter | Role |
|---|---|
| **Enable** | On/Off |
| **Depth** | Modulation amount (0–100%) |
| **Invert** | Reverse envelope polarity |

**Example:** Detect drum hits, dynamically lower cutoff during impact.

### Oversampling

Suppress aliasing at high resonance settings:

| Mode | Rate | Latency | CPU Cost |
|---|---|---|---|
| **Off** | 1× | 0 ms | Minimum |
| **Auto** | 2–4× | Variable | Moderate |
| **2×** | 2× | ~5 ms | Moderate |
| **4×** | 4× | ~10 ms | High |

**Recommendation:**
- Moog Ladder, TB-303: **2× minimum**
- Others: **Auto** sufficient

### Limiter Ceiling

RMS-tracking output limiter at final stage:

```
Threshold = -0.1 dB  → Transparent
Threshold = -3.0 dB  → Noticeable limiting
```

Protection against resonance peaks.

---

## Parameter Reference

### Filters A/B/C/D (Shared Parameters)

#### Enable

```
Off → Excluded from blend
On  → Participates in morph
```

**Note:** With ≤1 enabled filter, morphing auto-disables.

#### Model (0–27)

28 filter models. See "Filter System" section.

#### Type

| Type | Description |
|---|---|
| **LP** | Lowpass — attenuate high frequencies |
| **BP** | Bandpass — pass only center frequency region |
| **HP** | Highpass — attenuate low frequencies |
| **Notch** | Notch — sharply attenuate specific frequency |

#### Slope (dB/oct)

```
12 dB/oct  → Gentle, smooth
24 dB/oct  → Balanced (most common)
48 dB/oct  → Steep
96 dB/oct  → Extreme (high CPU)
```

**Note:** Not all models support all slopes.

#### Cutoff (20 Hz ～ 20 kHz)

Logarithmic frequency. Modulated by LFO2.

#### Res / Ctrl (0.1 ～ 10.0)

Resonance peak emphasis.

```
0.1   → Dead (no resonance)
0.707 → Flat (recommended start)
5.0   → Strong resonance
10.0  → Extreme (dangerous)
```

Modulated by LFO3.

### XY Morph Pad

#### Base X, Base Y

Current pad position (0.0–1.0). Auto-modulated by LFO1.

#### Morph Blend

Algorithm selection. See "Morphing Features."

#### Cutoff Algo

Frequency mapping method. See "Filter System."

### LFO Controls

#### Enable

```
Off → Inactive
On  → Full operation
```

#### Wave (1–19)

Waveform selection. See "LFO System."

#### Step Mode

```
Off → Smooth continuous
On  → Quantized steps
```

#### Sync

```
Off → Freerun (0.01–20.0 Hz)
On  → DAW tempo-locked
```

#### Rate Sync (Sync = On)

Note divisions: 8/1 to 1/64, Dotted, Triplet.

#### Rate Free (Sync = Off)

Frequency in Hz (0.01–20.0).

#### Min / Max

Modulation range boundaries.

#### Phase (0°–360°)

LFO phase offset. Useful for offsetting multiple LFOs.

#### Fade In (0–10 s)

Fade-in time from plugin load. Avoids sudden jumps.

#### Spread (0°–360°)

Per-filter phase offset. Creates decorrelation.

### LFO4: Rate Modulation

#### Enable / Wave / Depth

Controls and modulation amount.

#### Assign LFO1/2/3

Which LFO(s) to modulate. Multiple allowed.

### LFO5: Dry/Wet Range

#### Enable / Wave / Min / Max

Wet mix oscillation control.

---

## Use Cases & Techniques

### Case 1: Dynamic Cutoff Sweep (Dubstep Bass)

**Goal:** Searing cutoff sweep. Acid synth character.

**Setup:**
- Filter A: Moog Ladder, LP, Cutoff 500 Hz
- Filter B: TB-303, LP, Cutoff 2 kHz
- LFO2: Sawtooth, Sync On, Rate = 1/8, Min=0, Max=1
- Morph Blend: Equal Power
- XY Pad: Center (0.5, 0.5)

**Result:** Smooth sweep from Moog to TB-303 every 8 beats. Classic acid.

### Case 2: Organic Pad (Evolving)

**Goal:** Slowly morphing, warm, timely pad.

**Setup:**
- Filter A: Clean SVF, LP, Cutoff 1000 Hz, Res 0.707
- Filter B: Formant (Vowel), LP, Cutoff 800 Hz, Res 1.5
- Filter C: CEM3320, LP, Cutoff 1200 Hz, Res 2.0
- Filter D: CS-80, LP, Cutoff 600 Hz, Res 1.0
- All Enable: On
- LFO1: Smooth Noise, Sync Off, Rate 0.1 Hz, Min=0.2, Max=0.8
- Morph Blend: Smoothstep
- LFO5: Sine, 0.05 Hz, Min=30%, Max=70%

**Result:** XY drifts organically. Wet mix slowly breathes. Lush pad.

### Case 3: Chaos Lead

**Goal:** Unpredictable, experimental, otherworldly.

**Setup:**
- Filters A/B/C/D: Random diverse models (Wavefolder, Bode Shifter, Kilo All-Pass, Modal Resonator)
- LFO1: Attractor (Lorenz), Sync Off, Rate 0.3 Hz, Phase=45°
- LFO2: Billiard, Sync On, Rate = 1/16
- LFO3: Lemniscate, Sync Off, Rate 0.7 Hz, Min=1, Max=8
- Morph Blend: Radial
- Envelope Follower: On, Depth 50%

**Result:** Living, chaotic sound. Never the same twice.

### Case 4: Custom Recording Pattern

**Goal:** Hand-drawn complete control.

**Steps:**
1. LFO1 Tab: Select LFO1
2. Wave: Set to Recording
3. Draw ∿ wave on XY pad
4. Release mouse → recorded
5. LFO1 Enable: On

**Result:** Your hand-drawn wave controls morph position.

---

## Troubleshooting

### Q: No Sound Output

**A:**
1. Check filter **Enable** — at least one On?
2. Check **Wet/Wet Mix** — Wet > -∞ dB?
3. Check **Limiter Ceiling** — not extremely low?
4. Check DAW **track output** — volume > -∞?

### Q: Extremely Loud Output

**A:**
1. **Remove headphones immediately**
2. Lower DAW master volume
3. Set plugin Master Gain to -36 dB
4. Set Limiter Ceiling to -0.1 dB
5. If Resonance high, reduce to 4.0 or below
6. Enable Oversampling 4×
7. Restart at low volume, gradually test

### Q: High CPU Usage

**A:**
1. Disable Oversampling (critical impact)
2. Reduce enabled filters
3. Switch to low-CPU models (Clean SVF)
4. Use simpler LFO waves (avoid Attractor, Spirograph)
5. Minimize FilterVisualizer

### Q: Plugin Not Found in Ableton Live

**A:**
1. Verify: `C:\Program Files\Common Files\VST3\QuadMorphFilter.vst3` exists
2. Restart Ableton Live
3. Rescan: Preferences → Plug-in Devices → Rescan

### Q: "Zipper Noise" on Parameter Automation

**A:**
SmoothedValue interpolation (τ=50ms) smooths changes, but extremely steep automation curves may exceed it.

**Solution:**
1. Soften automation curves
2. Focus on Wet/Dry Mix automation
3. Enable Oversampling

---

## FAQ

### Q: How do I choose between filter models?

**A:** Quick characteristics:
- **Warm:** Ladder (Moog, TB-303) > Analog > SVF > Digital
- **Resonant:** TB-303 > Moog > SEM > Clean SVF > Digital
- **Experimental:** Spectral (Bode, Z-Plane) > Experimental > Analog
- **Versatile:** Clean SVF (general use) > Formant (special) > Experimental (niche)

### Q: Which morph blend algorithm?

**A:**
- **Equal Power (recommended):** Natural, all genres
- **Linear:** Sharp transitions, electronic
- **Smoothstep:** Smooth, ambient, pads
- **Radial:** Target-focused, dynamic

### Q: Sync LFO to DAW tempo?

**A:**
1. **Sync = On**
2. Select **Rate Sync** (e.g., 1/4 = quarter note)
3. Auto-syncs to DAW tempo

### Q: Edit or delete Recording wave?

**A:**
Currently, Recording is overwrite-only. To delete:
1. Redraw with new pattern (overwrites)
2. Or select different Wave

### Q: When to use Envelope Follower Invert?

**A:**
- **Off (normal):** Signal up → Effect up (sidechain pumping)
- **On (inverted):** Signal up → Effect down (ducking)

**Example:** Reduce cutoff when drums hit.

---

## Technical Specifications

### DSP Architecture

```
Input
  ├─ Dry Buffer Cache
  ├─ LFO Processing (3 systems + LFO4/5)
  ├─ Filter A/B/C/D Parameter Dispatch
  ├─ Weight Mix Calculation (enabledCount check)
  ├─ Per-Sample Coefficient Update
  ├─ Parallel Filter Processing
  │  ├─ FilterA_SVF_SIMD (AVX2)
  │  ├─ TptFilter B
  │  ├─ TptFilter C
  │  └─ TptFilter D
  ├─ 4-Filter Weighted Sum
  ├─ Dry/Wet Smooth Crossfade (τ=50ms)
  ├─ Master Gain
  ├─ RMS-Tracking Limiter
  └─ Output
```

### Real-Time Safety

- **Zero Heap Allocation** on audio thread
- **ScopedNoDenormals** — suppress denormal spikes
- **SmoothedValue Automation** — no zipper noise
- **Lock-Free Parameter Dispatch** — thread-safe updates

### Performance

| Operation | CPU Cost | Latency |
|---|---|---|
| Clean SVF × 1 | ~0.5% | 0 ms |
| Moog Ladder × 1 | ~1.2% | 0 ms |
| Oversampling 2× | +1–2% | ~5 ms |
| Oversampling 4× | +3–4% | ~10 ms |
| Visualizer (16-tap) | ~1% | Variable |

### Verified Platforms

- **DAW:** Ableton Live 11 / 12
- **CPU:** Intel i7-9700K (reference)
- **RAM:** 8 GB
- **OS:** Windows 10 / 11 64-bit

---

## License

Quad Morph Filter is distributed under the **GPLv3 License**.

See README.md and LICENSE file for details.

---

## Appendix: Keyboard Shortcuts

Not currently supported. All operations via mouse.

---

**End of Manual. Happy Morphing! 🎵**
