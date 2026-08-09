# -*- coding: utf-8 -*-
"""
QuadMorphFilter FACTORY プリセット生成器
  カテゴリごとに「音楽的に成立する組み合わせ」の候補プールを定義し、
  決定論的に 200 件を組み立てて C++ の静的テーブルを書き出す。
  ランダムではなくキュレーションされたプールからの巡回なので、
  同じ入力からは常に同じプリセットが出る。
"""
import random

# ---- モデル能力（ModelCapabilities.h と同一。type/slope の妥当性検証に使う）----
CAPS = {
    2:(2,1,0,0,0), 1:(1,1,1,1,0), 12:(1,1,1,1,0), 13:(1,1,0,0,0), 15:(1,1,0,0,0),
    3:(1,1,1,1,1), 5:(0,0,1,0,0), 10:(3,1,1,1,1), 21:(2,1,0,0,0), 22:(2,1,1,1,1),
    24:(0,1,1,0,0), 25:(0,1,1,1,1), 14:(0,1,0,1,0), 23:(3,1,1,0,0), 26:(3,1,0,1,0),
    8:(3,1,1,0,0), 27:(3,1,0,0,0),
}
def caps(m): return CAPS.get(m, (3,1,1,1,1))
def ok_type(m,t):
    c = caps(m); return c[1+t] == 1
def fix_type(m,t):
    if ok_type(m,t): return t
    for cand in (0,1,2,3):
        if ok_type(m,cand): return cand
    return 0
def fix_slope(m,s): return max(0, min(s, caps(m)[0]))

MODELS = {
 'clean':0,'moog':1,'303':2,'sem':3,'crush':4,'vowel':5,'comb':6,'ms20':7,'phaser':8,
 'fold':9,'fdn':10,'phase':11,'cem':12,'ssm':13,'cs80':14,'jup':15,'wasp':16,
 'butter':17,'cheby':18,'bessel':19,'ellip':20,'lpg':21,'modal':22,'wg':23,'bode':24,
 'zplane':25,'array':26,'nyq':27 }
M = MODELS

# ============================================================================
#  カテゴリ定義
#   models : 4 スロットに使うモデル候補（4個組のリスト）
#   cut    : カットオフの候補（Hz）
#   res    : レゾナンス候補
#   lfo    : LFO 設定候補（下の LFOS から選ぶキー）
#   names  : プリセット名の素材
# ============================================================================
LFOS = {
 # key: (lfo1en, wave, sync, rateSyncIdx, rateFree, min, max, spread)
 #
 # rateSyncIdx: 0="8/1"(32拍) 1="4/1"(16拍) 2="2/1"(8拍) 3="1/1"(4拍=1小節)
 #              4="1/2"(2拍) 5="1/4"(1拍) 6="1/8" 7="1/16"
 #
 # 【調整履歴】初版は 1/8〜1/16 を多用して忙しすぎたため、
 # 全体を 2 段階（＝4倍）遅くした。フィルターのモーフは
 # 1〜8 小節かけて動くくらいがちょうどよく、リズミックな
 # ゲート用途でも 1/4〜1/8 で十分に刻んで聞こえる。
 'off'      : (0, 0, 1, 5, 1.0, 0.0, 1.0, 0.0),
 'slowsine' : (1, 0, 1, 0, 0.05, 0.15, 0.85, 0.0),   # 8小節
 'barsine'  : (1, 0, 1, 1, 0.12, 0.10, 0.90, 0.0),   # 4小節
 'halfsine' : (1, 0, 1, 2, 0.25, 0.20, 0.80, 0.0),   # 2小節
 'qtrsaw'   : (1, 1, 1, 3, 0.5,  0.00, 1.00, 0.0),   # 1小節
 '8thpulse' : (1, 2, 1, 5, 1.0,  0.00, 1.00, 0.0),   # 1拍
 '16thstep' : (1, 1, 1, 6, 2.0,  0.10, 0.95, 0.0),   # 8分
 'rand1'    : (1, 3, 1, 3, 0.5,  0.05, 0.95, 0.0),
 'smooth'   : (1, 7, 1, 1, 0.12, 0.20, 0.80, 0.0),
 'spread90' : (1, 0, 1, 1, 0.12, 0.10, 0.90, 90.0),
 'spread120': (1, 0, 1, 0, 0.06, 0.05, 0.95, 120.0),
 'liss'     : (1,11, 1, 0, 0.06, 0.10, 0.90, 0.0),
 'spiro'    : (1, 8, 1, 0, 0.04, 0.10, 0.90, 0.0),
 'billiard' : (1,16, 0, 3, 0.05, 0.05, 0.95, 0.0),
 'torus'    : (1,10, 1, 0, 0.05, 0.15, 0.85, 0.0),
 'rose'     : (1,14, 1, 1, 0.08, 0.10, 0.90, 0.0),
}

CATS = []

def cat(name, models, cuts, ress, lfo1s, lfo2s, lfo3s, names, extra=None):
    CATS.append(dict(name=name, models=models, cuts=cuts, ress=ress,
                     lfo1=lfo1s, lfo2=lfo2s, lfo3=lfo3s, names=names,
                     extra=extra or {}))

cat("Classic Analog",
    [[M['moog'],M['cem'],M['ssm'],M['jup']],
     [M['moog'],M['sem'],M['cs80'],M['cem']],
     [M['jup'],M['ssm'],M['moog'],M['sem']],
     [M['cem'],M['cs80'],M['sem'],M['moog']]],
    [180,320,520,850,1400,2200,3600],
    [1.2,2.0,3.0,4.2,5.5],
    ['off','slowsine','barsine','halfsine','smooth'],
    ['off','barsine','halfsine'], ['off','slowsine'],
    ["Vintage Warmth","Ladder Cream","Poly Bloom","Silk Sweep","Analog Bed",
     "Curtis Glow","Prophet Pad","Jupiter Rise","SEM Air","Fat Stack",
     "Tape Era","Console Heat","Moog Round","Velvet Low","Studio Sheen",
     "Old Desk","Warm Motion","Classic Morph","Soft Ceramic","Amber Keys"])

cat("Acid & Squelch",
    [[M['303'],M['303'],M['moog'],M['ms20']],
     [M['303'],M['ms20'],M['303'],M['wasp']],
     [M['303'],M['ssm'],M['303'],M['moog']],
     [M['303'],M['wasp'],M['ms20'],M['303']]],
    [140,240,420,700,1200,1900],
    [5.5,6.8,8.0,9.0,10.0],
    ['qtrsaw','8thpulse','16thstep','rand1','barsine'],
    ['qtrsaw','8thpulse','16thstep'], ['off','8thpulse'],
    ["Acid Line","303 Burn","Squelch Bass","Rubber Band","Neon Slide",
     "Wet Acid","Screamer","TB Ghost","Rolling Acid","Sharp Tongue",
     "Warehouse","Acid Drips","Resin","Sour Lead","Bite Mark",
     "Modular Acid","Slippery","Overdriven","Late Night","Acid Morph"])

cat("Vocal & Formant",
    [[M['vowel'],M['vowel'],M['modal'],M['sem']],
     [M['vowel'],M['moog'],M['vowel'],M['cs80']],
     [M['vowel'],M['zplane'],M['vowel'],M['modal']],
     [M['vowel'],M['vowel'],M['cem'],M['zplane']]],
    [300,480,700,950,1300,1800],
    [2.5,3.5,4.5,6.0,7.5],
    ['slowsine','barsine','halfsine','liss','smooth'],
    ['barsine','halfsine','liss'], ['off','slowsine','barsine'],
    ["Talking Filter","Vowel Drift","Choir Gate","Whisper Pad","Robot Voice",
     "Ah To Ooh","Formant Walk","Throat Sing","Speaking Bass","Mouth Harp",
     "Vocoder Bed","Human Touch","Diphthong","Soft Consonant","Chant",
     "Larynx","Vowel Morph","Breath Pad","Syllable","Vox Machine"])

cat("Rhythmic & Gated",
    [[M['clean'],M['moog'],M['crush'],M['comb']],
     [M['moog'],M['clean'],M['ms20'],M['crush']],
     [M['sem'],M['clean'],M['moog'],M['nyq']],
     [M['clean'],M['cem'],M['crush'],M['clean']]],
    [220,400,700,1200,2000,3200],
    [1.5,2.5,4.0,6.0,8.0],
    ['8thpulse','16thstep','qtrsaw','rand1','spread90'],
    ['8thpulse','16thstep','qtrsaw'], ['16thstep','8thpulse','off'],
    ["Gate Pump","Sixteenth","Stutter Bed","Trance Gate","Chop Shop",
     "Pulse Train","Tick Tock","Hard Gate","Groove Cut","Offbeat",
     "Ratchet","Machine Gun","Half Time","Triplet Push","Sidechain Feel",
     "Percussive","Rhythmic Morph","Shuffle Gate","Broken Beat","Clockwork"])

cat("Ambient & Space",
    [[M['fdn'],M['wg'],M['modal'],M['clean']],
     [M['fdn'],M['fdn'],M['moog'],M['wg']],
     [M['wg'],M['modal'],M['fdn'],M['sem']],
     [M['fdn'],M['zplane'],M['wg'],M['cs80']]],
    [180,300,500,800,1400,2400],
    [3.0,4.5,6.0,7.5,9.0],
    ['slowsine','spiro','torus','smooth','liss'],
    ['slowsine','smooth','spiro'], ['off','slowsine','torus'],
    ["Deep Cavern","Slow Bloom","Drone Field","Endless Hall","Frozen Lake",
     "Distant Bells","Nebula","Cathedral","Ghost Room","Wind Tunnel",
     "Ice Sheet","Suspended","Long Tail","Sub Aquatic","Vapour",
     "Glacier","Space Morph","Aurora","Weightless","Far Away"])

cat("Metallic & Resonant",
    [[M['comb'],M['modal'],M['wg'],M['bode']],
     [M['modal'],M['comb'],M['modal'],M['wg']],
     [M['wg'],M['comb'],M['bode'],M['modal']],
     [M['comb'],M['wg'],M['modal'],M['comb']]],
    [110,180,260,380,560,820,1200],
    [4.0,5.5,7.0,8.5,9.5],
    ['barsine','rand1','spread120','rose','8thpulse'],
    ['barsine','rand1','spread120'], ['off','barsine','rose'],
    ["Struck Metal","Tuned Pipe","Gamelan","Steel Drum","Wire Resonance",
     "Bell Tower","Anvil","Prepared Piano","Sheet Metal","Harmonic Rod",
     "Kalimba","Tension Wire","Ring Mod Feel","Clang","Cymbal Bed",
     "Metal Morph","Sympathetic","Overtone","Struck Glass","Resonator"])

cat("Lo-Fi & Digital",
    [[M['crush'],M['nyq'],M['clean'],M['cheby']],
     [M['crush'],M['crush'],M['butter'],M['nyq']],
     [M['nyq'],M['crush'],M['ellip'],M['clean']],
     [M['crush'],M['bessel'],M['nyq'],M['cheby']]],
    [400,700,1100,1800,3000,5000],
    [1.0,2.0,3.5,5.0,7.0],
    ['16thstep','rand1','8thpulse','off','qtrsaw'],
    ['rand1','16thstep','off'], ['off','rand1'],
    ["Bit Dust","Old Sampler","12 Bit","Broken Radio","Data Loss",
     "Aliasing","Cassette","Downsampled","Digital Grit","Telephone",
     "Crunch Bed","Early Digital","Sample Hold","Noise Floor","Chip Tone",
     "Lo-Fi Morph","Tape Hiss","Compander","Dither","Glitch Bed"])

cat("Motion & Morph",
    [[M['moog'],M['vowel'],M['comb'],M['fdn']],
     [M['sem'],M['modal'],M['303'],M['wg']],
     [M['cem'],M['crush'],M['vowel'],M['phaser']],
     [M['clean'],M['zplane'],M['modal'],M['ms20']]],
    [250,450,750,1200,2000,3400],
    [2.0,3.5,5.0,6.5,8.0],
    ['spread90','spread120','liss','torus','rose','spiro'],
    ['barsine','liss','spread90'], ['halfsine','rose','torus'],
    ["Four Corners","Constant Drift","Slow Rotation","Quadrant","Orbiting",
     "Shape Shifter","Wandering","Morph Engine","Circular","Figure Eight",
     "Pendulum","Cross Fade","Evolving","Never Same","Rotary",
     "Spiral In","Phase Walk","Kaleidoscope","Tidal","Continuum"])

cat("FX & Special",
    [[M['phaser'],M['bode'],M['phase'],M['array']],
     [M['fold'],M['phaser'],M['bode'],M['clean']],
     [M['array'],M['phase'],M['fold'],M['phaser']],
     [M['bode'],M['fold'],M['array'],M['phase']]],
    [200,400,700,1100,1800,2800],
    [2.0,3.5,5.0,6.5,8.5],
    ['barsine','halfsine','rand1','liss','8thpulse'],
    ['barsine','halfsine','rand1'], ['off','barsine','liss'],
    ["Wide Phaser","Frequency Shift","Fold Over","Stereo Spread","Barber Pole",
     "Ring Halo","Shimmer Shift","Comb Phase","Wide Motion","Metal Fold",
     "Doppler","Detune Field","Chorus Ghost","Shepard","Inverse",
     "FX Morph","Alien Voice","Sideband","Warp Drive","Dimension"])

cat("Bass & Sub",
    [[M['moog'],M['303'],M['ssm'],M['jup']],
     [M['303'],M['moog'],M['cem'],M['ssm']],
     [M['ssm'],M['jup'],M['moog'],M['sem']],
     [M['moog'],M['cem'],M['303'],M['wasp']]],
    [70,110,160,230,340,500,760],
    [1.5,2.5,3.5,5.0,6.5],
    ['off','barsine','8thpulse','slowsine','16thstep'],
    ['off','8thpulse','barsine'], ['off','barsine'],
    ["Sub Weight","Reese Bed","Deep Growl","Round Bottom","Solid State",
     "Bass Morph","Cellar","Rumble","Pluck Bass","Warm Sub",
     "Distorted Low","Fifth Below","Tight Low","Wobble","Foundation",
     "Dub Bass","Analog Sub","Chest Punch","Low Motion","Anchor"])

# ============================================================================
#  組み立て
# ============================================================================
def build():
    out = []
    for ci, c in enumerate(CATS):
        rnd = random.Random(1000 + ci)     # カテゴリごとに固定シード = 再現性あり
        for i, nm in enumerate(c['names']):
            models = c['models'][i % len(c['models'])]
            p = {}

            # ---- 4 フィルター ----
            # 3〜4 個を有効にし、必ず A は有効
            n_on = 3 if (i % 4 == 1) else 4
            for k in range(4):
                s = "ABCD"[k]
                m = models[k]
                on = 1 if k < n_on else 0
                cut = c['cuts'][(i + k * 2) % len(c['cuts'])]
                # 4 本のカットオフをばらけさせて帯域を分担させる
                cut = cut * (0.72 + 0.38 * k)
                cut = max(20.0, min(20000.0, cut))
                res = c['ress'][(i + k) % len(c['ress'])]
                t = fix_type(m, [0,0,1,2][k] if k != 0 else 0)
                sl = fix_slope(m, [1,1,2,1][k])
                p[f"enable{s}"] = on
                p[f"model{s}"] = m
                p[f"type{s}"] = t
                p[f"slope{s}"] = sl
                p[f"cutoff{s}"] = round(cut, 1)
                p[f"res{s}"] = round(res, 2)
                # LFO2/3 の割り当て（0=Off,1=+X,2=+Y,3=-X,4=-Y）
                # 割り当ては後段でまとめて決める（LFO の ON/OFF と必ず一致させるため）
                p[f"lfoCutSrc{s}"] = 0
                p[f"lfoResSrc{s}"] = 0

            # ---- MORPH ----
            p["posX"] = round(0.2 + 0.6 * ((i % 5) / 4.0), 3)
            p["posY"] = round(0.2 + 0.6 * (((i // 5) % 4) / 3.0), 3)
            p["morphBlend"] = [0,0,2,3,1][i % 5]
            p["cutoffAlgo"] = [0,1,2][i % 3]
            p["xyDepth"] = [0,0,0,15,25][i % 5]

            # ---- LFO1 (Morph) ----
            k1 = c['lfo1'][i % len(c['lfo1'])]
            e,w,sy,rs,rf,mn,mx,sp = LFOS[k1]
            p.update({"lfo1en":e,"lfo1wave":w,"lfo1sync":sy,"lfo1rateSync":rs,
                      "lfo1rateFree":rf,"lfo1min":mn,"lfo1max":mx,"lfo1spread":sp,
                      "lfo1phase":[0,90,180][i%3],"lfo1fade":[0,0,250][i%3],
                      "lfo1step":1 if k1=='16thstep' else 0})

            # ---- LFO2 (Cutoff) ----
            #
            # 【整合性】LFO の ON/OFF と、各フィルターの割り当てボタンは
            # 必ず一致させる。片方だけ立っていると
            #   ・LFO は動いているのに音が変わらない
            #   ・ボタンは点灯しているのに変調が来ない
            # という「壊れて見える」プリセットになるため。
            k2 = c['lfo2'][i % len(c['lfo2'])]
            e,w,sy,rs,rf,mn,mx,sp = LFOS[k2]
            p.update({"lfo2en":e,"lfo2wave":w,"lfo2sync":sy,"lfo2rateSync":rs,
                      "lfo2rateFree":rf,"lfo2min":mn,"lfo2max":mx,"lfo2spread":sp})

            if e:
                # 有効フィルターの中から 2 本以上に、別々の軸を割り当てる
                targets = [k for k in range(4) if p[f"enable{'ABCD'[k]}"] == 1]
                axes = [1, 2, 3, 4]            # +X / +Y / -X / -Y
                for n, k in enumerate(targets):
                    if n == 0 or (n + i) % 2 == 0:
                        p[f"lfoCutSrc{'ABCD'[k]}"] = axes[(i + n) % 4]

            # ---- LFO3 (Reso) ----
            k3 = c['lfo3'][i % len(c['lfo3'])]
            e,w,sy,rs,rf,mn,mx,sp = LFOS[k3]
            p.update({"lfo3en":e,"lfo3wave":w,"lfo3sync":sy,"lfo3rateSync":rs,
                      "lfo3rateFree":rf,"lfo3min":mn,"lfo3max":mx,"lfo3spread":sp})

            if e:
                # Res 変調は掛けすぎると不安定になるので 1〜2 本に絞る
                targets = [k for k in range(4) if p[f"enable{'ABCD'[k]}"] == 1]
                for n, k in enumerate(targets):
                    if n == 0 or (n + i) % 3 == 0:
                        p[f"lfoResSrc{'ABCD'[k]}"] = [2, 4, 1, 3][(i + n) % 4]

            # ---- LFO4 (Rate Mod) : 5 件に 1 件だけ ----
            use4 = (i % 5 == 3)
            p.update({"lfo4en":1 if use4 else 0,"lfo4wave":0,"lfo4sync":1,
                      "lfo4rateSync":0,"lfo4depth":1.0 if use4 else 0.0,
                      "lfo4assignA":1,"lfo4assignB":0,"lfo4assignC":0})

            # ---- LFO5 (Dry/Wet) : 6 件に 1 件 ----
            use5 = (i % 6 == 4)
            p.update({"lfo5en":1 if use5 else 0,"lfo5wave":0,"lfo5sync":1,
                      "lfo5rateSync":2,"lfo5min":45.0,"lfo5max":100.0})

            # ---- Envelope Follower : 7 件に 1 件 ----
            useE = (i % 7 == 5)
            p.update({"envFollowen":1 if useE else 0,"envFollowdepth":55.0 if useE else 50.0,
                      "envFollowattack":12.0,"envFollowrelease":140.0,"envFollowinvert":0})

            # ---- 出力 ----
            p["dryWet"] = [100,100,100,85,92][i % 5]
            p["masterGain"] = round([0.0,-1.0,-2.0,0.5,-0.5][i % 5], 1)
            p["limiterCeiling"] = -0.1
            p["osMode"] = 1

            s = ";".join(f"{k}={v}" for k, v in p.items())
            out.append((c['name'], nm, s))
    return out

items = build()
assert len(items) == 200, len(items)

esc = lambda s: s.replace('\\','\\\\').replace('"','\\"')
lines = []
for cat_, nm, ps in items:
    lines.append(f'    {{ "{esc(cat_)}", "{esc(nm)}", "{esc(ps)}" }},')

body = "\n".join(lines)
print(f"生成: {len(items)} 件 / {len(set(c for c,_,_ in items))} カテゴリ")
for c in dict.fromkeys(c for c,_,_ in items):
    print(f"  {c}: {sum(1 for x in items if x[0]==c)} 件")
open('/sessions/cool-serene-planck/mnt/outputs/gen/table.inc','w',encoding='utf-8').write(body)
print("平均パラメータ文字列長:", sum(len(x[2]) for x in items)//len(items))
