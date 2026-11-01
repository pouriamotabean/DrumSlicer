# gen.py <libdir> <outdir> <n> <seed> : synthetic drum loops from one-shots, with the truth
import sys, os, json, random, wave, glob
import numpy as np
lib, out, n, seed = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
os.makedirs(out, exist_ok=True)
rng = random.Random(seed)
SR = 44100

def load(f):
    w = wave.open(f); a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, 2).astype(np.float32) / 32768
    return a

files = sorted(glob.glob(lib + '/*.wav'))
def pick(cat, sub=None, excl=()):
    c = [f for f in files if os.path.basename(f).startswith('OLIVER_' + cat) and (sub is None or sub in f) and not any(e in f for e in excl)]
    return rng.choice(c)

# styles: (name, steps per bar = 16), lanes: list of (inst, steps, vel range)
def pattern(style):
    P = {}
    if style == 'four':        # house / pop
        P['kick'] = [0, 4, 8, 12]; P['snare'] = [4, 12]; P['chat'] = [2, 6, 10, 14]; P['ohat'] = []
    elif style == 'boom':      # boom bap
        P['kick'] = [0, 7, 10]; P['snare'] = [4, 12]; P['chat'] = list(range(0, 16, 2))
    elif style == 'trap':
        P['kick'] = [0, 6, 11]; P['snare'] = [8]; P['chat'] = list(range(16)); P['ohat'] = [14]
    elif style == 'rnb':
        P['kick'] = [0, 3, 9]; P['snare'] = [4, 12]; P['chat'] = list(range(0, 16, 2)) + [7, 15]; P['perc'] = [6, 13]
    elif style == 'break':
        P['kick'] = [0, 2, 10]; P['snare'] = [4, 12, 7, 15]; P['chat'] = list(range(0, 16, 2)); P['ghost'] = [7, 15]
    else:                      # sparse
        P['kick'] = [0, 10]; P['snare'] = [8]; P['chat'] = [4, 12]
    return P

styles = ['four', 'boom', 'trap', 'rnb', 'break', 'sparse']
for k in range(n):
    style = styles[k % len(styles)]
    bpm = rng.choice([70, 85, 90, 95, 100, 110, 120, 128, 140]) if style != 'trap' else rng.choice([70, 75, 140, 145])
    bars = rng.choice([2, 4])
    kit = {'kick': pick('kick'), 'snare': pick('snare'), 'chat': pick('hihat', 'closed')}
    layered = rng.random() < 0.35
    if layered: kit['clap'] = pick('clap')
    P = pattern(style)
    if P.get('ohat') is not None:
        oh = [f for f in files if 'hihat' in f and 'open' in f]
        if oh: kit['ohat'] = rng.choice(oh)
        else: P.pop('ohat')
    if 'perc' in P: kit['perc'] = pick('percussion')
    if 'ghost' in P: kit['ghost'] = kit['snare']
    step = 60.0 / bpm / 4 * SR
    L = int(round(bars * 16 * step))
    mix = np.zeros((L + SR * 8, 2), np.float32)
    hits = []
    shots = {i: load(f)[:SR * 3] for i, f in kit.items()}
    for b in range(bars):
        for inst, steps in P.items():
            if inst not in kit: continue
            for s in steps:
                if style == 'trap' and inst == 'chat' and s % 2 and rng.random() < 0.3: continue
                if inst == 'chat' and s in P.get('ohat', []): continue          # an open hat is not closed at once
                v = {'kick': (0.85, 1.0), 'snare': (0.85, 1.0), 'chat': (0.45, 0.8), 'ohat': (0.5, 0.7), 'perc': (0.5, 0.8), 'ghost': (0.2, 0.3)}[inst]
                vel = rng.uniform(*v)
                if inst == 'chat' and s % 4 == 0: vel = min(1.0, vel * 1.25)
                t = int(round((b * 16 + s) * step + (rng.uniform(-0.004, 0.004) * SR if style in ('rnb', 'break') else 0)))
                t = max(0, t)
                sh = shots[inst]; mix[t:t + len(sh)] += vel * sh
                hits.append({'t': t / SR, 'inst': 'snare' if inst == 'ghost' else inst, 'vel': round(vel, 3)})
                if inst == 'snare' and layered:
                    c = shots['clap']; mix[t:t + len(c)] += vel * 0.8 * c
    # wrap the tail into the start (a loop), then a gentle level
    tail = mix[L:]; mix = mix[:L]; mix[:len(tail)] += tail[:L]
    pk = np.abs(mix).max(); mix *= 0.89 / max(pk, 1e-9)
    name = 'gt%02d_%s_%d' % (k, style, bpm)
    with wave.open(os.path.join(out, name + '.wav'), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(mix, -1, 1) * 32767).astype(np.int16).tobytes())
    json.dump({'bpm': bpm, 'bars': bars, 'style': style, 'layered': layered,
               'kit': {i: os.path.basename(f) for i, f in kit.items()}, 'hits': sorted(hits, key=lambda h: h['t'])},
              open(os.path.join(out, name + '.json'), 'w'), indent=0)
    print(name, 'clap+snare' if layered else '', {i: os.path.basename(f)[7:30] for i, f in kit.items()})
