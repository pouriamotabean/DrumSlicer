# score.py <loopsdir> <resultsdir> <libdir> : how near the engine comes to the truth
import sys, os, json, glob, wave
import numpy as np
L, R, LIB = sys.argv[1], sys.argv[2], sys.argv[3]
SR = 44100
CAT = lambda n: 'kick' if n.startswith('KICK') else 'snare' if n.startswith(('SNARE', 'CLAP', 'RIM')) else 'hat' if ('HAT' in n or n.startswith(('SHAKER', 'CRASH', 'RIDE', 'CYM'))) else 'perc'
TCAT = {'kick': 'kick', 'snare': 'snare', 'chat': 'hat', 'ohat': 'hat', 'perc': 'perc'}

def load(f):
    w = wave.open(f); ch = w.getnchannels(); sw = w.getsampwidth()
    b = np.frombuffer(w.readframes(w.getnframes()), np.uint8)
    if sw == 3: b = b.reshape(-1, 3); a = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)); a = np.where(a >= 1 << 23, a - (1 << 24), a) / float(1 << 23)
    elif sw == 2: a = b.view(np.int16) / 32768.0
    else: a = b.view(np.int32) / 2.0 ** 31
    return a.reshape(-1, ch).mean(1).astype(np.float32)

edges = 40 * (16000 / 40) ** (np.arange(25) / 24)
def picture(a):
    on = int(np.argmax(np.abs(a) > 0.1 * np.abs(a).max())); a = a[max(0, on - 64):]
    a = np.pad(a, (0, max(0, int(0.4 * SR) - len(a))))
    F = []
    for k in range(10):
        s = a[int(k * 0.04 * SR):int((k + 1) * 0.04 * SR)] * np.hanning(int(0.04 * SR))
        p = np.abs(np.fft.rfft(s, 4096)) ** 2; f = np.fft.rfftfreq(4096, 1 / SR)
        F.append([p[(f >= edges[i]) & (f < edges[i + 1])].sum() + 1e-12 for i in range(24)])
    F = 10 * np.log10(np.array(F)); F -= F.max(); return np.maximum(F, -50)

tot = {'rec': [], 'fp': 0, 'n': 0, 'name': [], 'snd': {}, 'bpm': 0}
for jf in sorted(glob.glob(L + '/*.json')):
    nm = os.path.basename(jf)[:-5]; T = json.load(open(jf))
    txt = glob.glob(os.path.join(R, nm, '*_drumslicer.txt'))
    if not txt: print(nm, 'NO RESULT'); continue
    pads, hits = {}, []
    for l in open(txt[0]):
        k, v = l.strip().split('=', 1); x = v.split('|')
        if k == 'pad': pads[int(x[0])] = x[1]
        elif k == 'hit': hits.append((int(x[0]) / SR, int(x[1])))
    log = open(os.path.join(R, nm, 'log.txt')).read()
    bpm = float([l for l in log.splitlines() if l.startswith('time')][0].split()[3])
    tol = 0.02
    # which instrument each pad is: the truth instrument most of its notes sit on
    votes = {p: {} for p in pads}
    for t, p in hits:
        for h in T['hits']:
            if abs(h['t'] - t) < tol: votes[p][h['inst']] = votes[p].get(h['inst'], 0) + 1
    padInst = {p: (max(v, key=lambda i: v[i] + (0.5 if TCAT.get(i) == CAT(pads[p]) else 0)) if v else None) for p, v in votes.items()}   # a tie: what its name says
    rec = {}; fp = 0
    for h in T['hits']:
        ok = any(abs(t - h['t']) < tol and padInst[p] == h['inst'] for t, p in hits)
        rec.setdefault(h['inst'], []).append(ok)
    for t, p in hits:
        if not any(abs(h['t'] - t) < tol and h['inst'] == padInst[p] for h in T['hits']): fp += 1
    names = [(pads[p], padInst[p]) for p in pads]
    nameOk = [CAT(n) == TCAT.get(i, '?') for n, i in names if i]
    # the sound: each instrument's pad (most notes) against the real sample
    snd = {}
    for inst, f in T['kit'].items():
        if inst == 'clap' or inst == 'ghost': continue
        cand = [p for p in pads if padInst[p] == inst]
        if not cand: snd[inst] = None; continue
        p = max(cand, key=lambda q: votes[q].get(inst, 0))
        pw = glob.glob(os.path.join(R, nm, '*_' + pads[p] + '.wav'))
        if not pw: continue
        truth = load(os.path.join(LIB, f))
        if T['layered'] and inst == 'snare':
            c = load(os.path.join(LIB, T['kit']['clap'])); n2 = max(len(truth), len(c)); truth = np.pad(truth, (0, n2 - len(truth))) + 0.8 * np.pad(c, (0, n2 - len(c)))
        snd[inst] = float(np.abs(picture(load(pw[0])) - picture(truth)).mean())
    bpmOk = abs(bpm - T['bpm']) < 0.01 * T['bpm']
    tot['bpm'] += bpmOk; tot['fp'] += fp; tot['n'] += len(hits); tot['name'] += nameOk
    for i, v in rec.items(): tot['rec'] += v
    for i, v in snd.items(): tot['snd'].setdefault(i, []).append(v)
    print('%-18s bpm %6.1f%s  recall %s  extra %d  names %d/%d  sound dB %s' % (nm, bpm, '' if bpmOk else '(!%d)' % T['bpm'],
          ' '.join('%s %d/%d' % (i, sum(v), len(v)) for i, v in rec.items()), fp, sum(nameOk), len(nameOk),
          ' '.join('%s %s' % (i, '--' if v is None else '%.1f' % v) for i, v in snd.items())))
print('TOTAL recall %.1f%%  extra %d/%d notes  names %d/%d  tempo %d/%d  sound dB: %s' % (100 * np.mean(tot['rec']), tot['fp'], tot['n'],
      sum(tot['name']), len(tot['name']), tot['bpm'], len(glob.glob(L + '/*.json')),
      ' '.join('%s %.1f (missing %d)' % (i, np.mean([x for x in v if x is not None] or [0]), sum(x is None for x in v)) for i, v in tot['snd'].items())))
