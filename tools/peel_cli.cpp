// Command line test harness for Source/DrumPeel.h (no JUCE needed):
//   g++ -O2 -std=c++17 -o peel_cli tools/peel_cli.cpp
//   ./peel_cli loop.wav outdir
#include "../Source/DrumPeel.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>

static bool readWav(const char* path, dp::Buf& x, double& sr)
{
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 44 || std::string((char*) d.data(), 4) != "RIFF") return false;
    size_t p = 12; int ch = 0, bits = 0, fmt = 0; const uint8_t* data = nullptr; size_t dlen = 0;
    auto u16 = [&](size_t i) { return (int) (d[i] | d[i + 1] << 8); };
    auto u32 = [&](size_t i) { return (uint32_t) (d[i] | d[i + 1] << 8 | d[i + 2] << 16 | (uint32_t) d[i + 3] << 24); };
    while (p + 8 <= d.size())
    {
        std::string id((char*) &d[p], 4); uint32_t len = u32(p + 4);
        if (id == "fmt ") { fmt = u16(p + 8); ch = u16(p + 10); sr = u32(p + 12); bits = u16(p + 22); if (fmt == 0xFFFE) fmt = u16(p + 32); }
        if (id == "data") { data = &d[p + 8]; dlen = std::min<size_t>(len, d.size() - p - 8); }
        p += 8 + len + (len & 1);
    }
    if (!data || ch < 1) return false;
    const int bps = bits / 8; const size_t n = dlen / (size_t) (bps * ch);
    x.resize((int) n);
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            const uint8_t* s = data + (i * ch + std::min(c, ch - 1)) * bps; double v = 0;
            if (fmt == 3 && bits == 32) { float fv; std::memcpy(&fv, s, 4); v = fv; }
            else if (bits == 16) v = (int16_t) (s[0] | s[1] << 8) / 32768.0;
            else if (bits == 24) v = ((int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8) / 8388608.0;
            else if (bits == 32) v = (int32_t) (s[0] | s[1] << 8 | s[2] << 16 | (uint32_t) s[3] << 24) / 2147483648.0;
            x.c[c][i] = v;
        }
    return true;
}

static void writeWav24(const std::string& path, const dp::Buf& y, double sr)
{
    std::ofstream f(path, std::ios::binary);
    const uint32_t n = (uint32_t) y.n, dlen = n * 6;
    auto w32 = [&](uint32_t v) { f.put((char) v); f.put((char) (v >> 8)); f.put((char) (v >> 16)); f.put((char) (v >> 24)); };
    auto w16 = [&](uint16_t v) { f.put((char) v); f.put((char) (v >> 8)); };
    f.write("RIFF", 4); w32(36 + dlen); f.write("WAVEfmt ", 8); w32(16); w16(1); w16(2); w32((uint32_t) sr); w32((uint32_t) sr * 6); w16(6); w16(24);
    f.write("data", 4); w32(dlen);
    for (uint32_t i = 0; i < n; ++i) for (int c = 0; c < 2; ++c)
    {
        const double v = std::clamp(y.c[c][i], -1.0, 1.0 - 1.0 / 8388608.0); const int32_t q = (int32_t) std::lround(v * 8388608.0);
        f.put((char) q); f.put((char) (q >> 8)); f.put((char) (q >> 16));
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) { std::cerr << "usage: peel_cli loop.wav outdir\n"; return 1; }
    dp::Buf x; double sr = 44100;
    if (!readWav(argv[1], x, sr)) { std::cerr << "cannot read wav\n"; return 1; }
    const std::string out = argv[2];
    std::string name = argv[1]; name = name.substr(name.find_last_of("/\\") + 1); name = name.substr(0, name.find_last_of('.'));
    auto t0 = std::chrono::steady_clock::now();
    const double winSec = std::getenv("DP_WIN") ? std::atof(std::getenv("DP_WIN")) : 40.0;
    auto r = dp::analyseLong(x, sr, 0.03, [](float p, const std::string& s) { std::cout << "[" << (int) (p * 100) << "%] " << s << std::endl; }, nullptr, winSec);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    for (auto& p : r.pads)
    {
        std::string nm = p.name; nm.erase(std::remove(nm.begin(), nm.end(), ' '), nm.end());
        writeWav24(out + "/" + name + "_" + nm + ".wav", p.audio, sr);
        writeWav24(out + "/" + name + "_" + nm + "_best.wav", p.best, sr);
    }
    {   auto tm = dp::makeMidiFile(r, r.bpm, 480, false, 0.0, true, 1.0);
        std::ofstream(out + "/" + name + "_tight.mid", std::ios::binary).write((const char*) tm.data(), (std::streamsize) tm.size()); }
    {   // same info file as the plugin writes (read by tools/verify_match.py)
        std::ofstream inf(out + "/" + name + "_drumslicer.txt");
        for (size_t i = 0; i < r.pads.size(); ++i) inf << "pad=" << i << "|" << r.pads[i].name << "|" << r.pads[i].note << "|" << r.pads[i].hits << "\n";
        for (auto& n : r.notes) inf << "hit=" << (long long) n.time << "|" << n.pad << "|" << n.velocity << "\n";
    }
    auto mid = dp::makeMidiFile(r, r.bpm);
    std::ofstream(out + "/" + name + ".mid", std::ios::binary).write((const char*) mid.data(), (std::streamsize) mid.size());
    dp::Buf y; dp::Peeler::render(r.pads, r.notes, y, x.n); writeWav24(out + "/" + name + "_rebuilt.wav", y, sr);
    std::printf("time %.1fs  bpm %.3f  notes %zu  pads %zu\nnull %.2f%%  hearing", secs, r.bpm, r.notes.size(), r.pads.size(), r.nullMatch * 100);
    for (double h : r.hearing) std::printf(" %.2f%%", h * 100);
    std::printf("\n");
    for (auto& p : r.pads) std::printf("  %-8s note %d  hits %d  learned %d ms  best hit @%.2fs %d ms\n", p.name.c_str(), p.note, p.hits, (int) (p.audio.n * 1000 / sr), p.bestTime / sr, (int) (p.best.n * 1000 / sr));
    {   std::vector<dp::PadOut> bp = r.pads; for (auto& p : bp) p.audio = p.best;
        dp::Buf yb; dp::Peeler::render(bp, r.notes, yb, x.n); writeWav24(out + "/" + name + "_rebuilt_best.wav", yb, sr); }
    return 0;
}
