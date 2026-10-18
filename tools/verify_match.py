#!/usr/bin/env python3
"""Independent check: does  MIDI + one-shots  rebuild the loop?   (numpy + scipy)
   python tools/verify_match.py loop.wav  "Documents/DrumSlicer/<loop>"
Plays the .mid with the WAVs exactly like Drum Slicer (whole-sample start, gain = velocity/100,
Poly-Pressure one tick before a note = sub-tick delay) and prints the match."""
import sys, glob, os, struct, numpy as np
from scipy.io import wavfile
from scipy import signal

def load(p):
    sr, d = wavfile.read(p)
    x = d / 2.0**31 if d.dtype == np.int32 else d / 2.0**15 if d.dtype == np.int16 else d.astype(float)
    if x.ndim == 1: x = np.stack([x, x], 1)
    return sr, x[:, :2].astype(float)

def read24(p):
    raw = open(p, 'rb').read(); i = 12; sr = 44100; ch = 2; bits = 24; data = b''
    while i + 8 <= len(raw):
        cid = raw[i:i + 4]; ln = struct.unpack('<I', raw[i + 4:i + 8])[0]
        if cid == b'fmt ': ch, sr = struct.unpack('<HI', raw[i + 10:i + 16]); bits = struct.unpack('<H', raw[i + 22:i + 24])[0]
        if cid == b'data': data = raw[i + 8:i + 8 + ln]
        i += 8 + ln + (ln & 1)
    if bits != 24: return load(p)
    b = np.frombuffer(data, np.uint8).reshape(-1, 3).astype(np.int32)
    v = (b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16); v = np.where(v >= 1 << 23, v - (1 << 24), v) / 2.0**23
    v = v.reshape(-1, ch)
    return sr, (np.stack([v[:, 0], v[:, 0]], 1) if ch == 1 else v[:, :2])

def read_midi(path, sr):
    d = open(path, 'rb').read(); ppq = struct.unpack('>H', d[12:14])[0]; i = 14; ev = []; tempo = 500000
    while i < len(d):
        ln = struct.unpack('>I', d[i + 4:i + 8])[0]; p = i + 8; end = p + ln; t = 0; rs = 0
        while p < end:
            v = 0
            while True:
                c = d[p]; p += 1; v = (v << 7) | (c & 0x7F)
                if c < 0x80: break
            t += v; st = d[p]
            if st == 0xFF:
                typ = d[p + 1]; p += 2; l = 0
                while True:
                    c = d[p]; p += 1; l = (l << 7) | (c & 0x7F)
                    if c < 0x80: break
                if typ == 0x51: tempo = int.from_bytes(d[p:p + 3], 'big')
                p += l; continue
            if st & 0x80: rs = st; p += 1
            hi = rs & 0xF0
            if hi in (0xC0, 0xD0): a, b = d[p], 0; p += 1
            else: a, b = d[p], d[p + 1]; p += 2
            ev.append((t, hi, a, b))
        i = end
    spt = tempo / 1e6 / ppq * sr; fine = {}; out = []
    for t, hi, a, b in ev:
        if hi == 0xA0: fine[a] = (t, b)
        elif hi == 0x90 and b > 0:
            off = fine[a][1] / 127.0 if a in fine and t - fine[a][0] == 1 else 0.0
            fine.pop(a, None); out.append((t * spt + off * spt, a, b))
    return out

def bands(x, sr):
    f = np.fft.rfftfreq(2048, 1 / sr); cents = 25 * 2 ** (np.arange(30) / 3); cents = cents[cents < 18000]
    S = sum(np.abs(signal.stft(x[:, c], sr, nperseg=2048, noverlap=2048 - 441)[2])**2 for c in range(2))
    return cents, np.array([S[(f >= c / 2**(1 / 6)) & (f < c * 2**(1 / 6))].sum(0) for c in cents])

def main():
    loop, folder = sys.argv[1], sys.argv[2]
    sr, x = load(loop); name = os.path.splitext(os.path.basename(loop))[0]
    info = open(glob.glob(os.path.join(folder, '*_drumslicer.txt'))[0]).read().splitlines()
    shots = {}
    for l in info:
        if l.startswith('pad='):
            _, nm, note, _ = l[4:].split('|')
            shots[int(note)] = read24(os.path.join(folder, f"{name}_{nm.replace(' ', '')}.wav"))[1]
    notes = read_midi(glob.glob(os.path.join(folder, '*.mid'))[0], sr)
    y = np.zeros_like(x)
    for t, n, v in notes:
        s = shots.get(n)
        if s is None: continue
        i0 = int(np.floor(t + 0.5)); m = min(len(x) - i0, len(s))
        if m > 0: y[i0:i0 + m] += v / 100.0 * s[:m]
    print(f"notes {len(notes)}   null (waveform) {100 * (1 - ((x - y)**2).sum() / (x**2).sum()):.1f}%")
    c, Bx = bands(x, sr); _, By = bands(y, sr)
    for lo, hi, nm in [(20, 100, 'SUB'), (100, 300, 'LOW'), (300, 1000, 'LOW-MID'), (1000, 4000, 'MID'), (4000, 20000, 'HIGH')]:
        k = (c >= lo) & (c < hi)
        print(f"  {nm:8s} {100 * (1 - ((np.sqrt(Bx[k]) - np.sqrt(By[k]))**2).sum() / Bx[k].sum()):5.1f}%")

if __name__ == '__main__': main()
