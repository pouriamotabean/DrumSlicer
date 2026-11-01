# ReKit — where we are (v1.14.0) and what comes next

## Done and tested
- Engine: separation, MIDI (EXACT / SYNC / TIGHT), MATCH (EQ, shape, room, pitch, drop, drive, width, loudness), LAYER MATCH,
  SIMILAR (ranked after match), BUILD MY KIT (30 matched suggestions per pad, < >, lock, one file per pad).
- Pouria: "100% happy with the sound / engine, 30% with layout and workflow" -> v2.0 is a UI / workflow release.

## v2.0 — decided (mockup: Claude artifact "ReKit 2 UI", row "Flow C")
1. Main screen without drawers: source in one line, PATTERN (play, LOOP / YOUR KIT toggle, TAB switches, drag = MIDI),
   BUILD MY KIT, six pad cards, EXPORT ALL / SAVE KIT. The whole normal job works without opening a drawer.
2. Six pads (not eight). A 7th sound merges into the nearest pad. Old projects: no compatibility needed.
3. Pad card: light strip (dim / lit when selected / flashes on its hits while the pattern plays), name, hits, lock, M, S,
   sample name + folder between < >, % like the loop, ADVANCED, drag WAV. Click the sample name = browser.
4. ADVANCED drawers per section (one open at a time, Esc / click outside closes, the pattern stays visible and playing):
   source (sensitivity, snap, change part, re-analyse), pattern (MIDI mode exact / grid, grid strength, tempo info),
   pad (sound: the loop's / best real hit / yours + folder; shape; knobs; match parts; amount; layer), kit (volume, folders, export).
5. A dot on ADVANCED when anything inside is not at its default.
6. Fewer concepts: match always automatic (no MATCH button), A/B -> "the loop's sound" (best real hit only in ADVANCED),
   MIDI 2 modes (exact / on grid; sync to project tempo always on), TRY IN KIT gone (picking plays in the pattern),
   browser only "like the loop's sound", punch + sustain -> ATTACK.
7. Two numbers on top: EXTRACTION (loop's sounds + MIDI replay the loop) and YOUR KIT (the pattern with your samples vs the
   loop, same measure, live).
8. Graphics drawn in code (flat, dark graphite + amber); no ChatGPT images needed. Window ~1300 x 780.

## Added in the final mockup round (screens C1-C8)
9. Where each sound comes from (pad ADVANCED): THE LOOP'S = "from N hits", click = they light up on the loop's wave;
   BEST REAL HIT = "hit 4 at 0:03.27", play it raw from the loop; YOURS = file + folder, OPEN FOLDER, drag the original file;
   a layer partner shows its own folder. On the main screen the selected pad's hits are lit on the source line.
10. Two numbers on top: EXTRACTION (fixed after analysis) and YOUR KIT (live).
11. UNDO BUILD next to BUILD MY KIT (back to the kit as it was before the last build).
12. Layer: FIND A LAYER (automatic) or CHOOSE ONE... (opens the browser; LAYER WITH YOURS on any row).
13. Browser (click a pad's sample name): covers the pads, the pattern above keeps playing; SIMILAR / FOLDERS, filter;
    each row: % after match, USE (goes on the pad, matched, pad locks), LAYER WITH YOURS, open folder, drag original.
14. Source ADVANCED: the selection on the whole file with the flags, snap, sensitivity, RE-ANALYSE THIS PART.
    Pattern ADVANCED: MIDI timing EXACT / ON THE GRID, HOW TIGHT, loop / project tempo, grid. Kit settings: volume,
    sample folders (+ add, rescan, loop length limit), what EXPORT ALL gives, window size S / M / L.
15. Start: drop zone + LOAD + OPEN A KIT + recent; when a file is loaded: flags, ANALYSE THIS PART.
16. KITS: save as (name), list with play preview, OPEN, folder, DELETE.
17. TAB (loop / your kit) only works while the plugin window has the keyboard: the on-screen toggle stays the main control.
18. Window: designed at 1300 x 780, resizable (fixed aspect), sizes S / M / L - check it fits the Cubase screen first.

## Visual direction (decided 2026-10-23): "a device from the future", BLACK body (mockup "D2 · FUTURE · black")
- Teenage Engineering / Apple-like: minimal, monochrome, one accent (orange #FF5A1F) for live / main things only.
- Black device body, a big black display = the heart: kit name large, ring "your kit", pattern as dots of light
  (lit hits glow, the playing one orange), display pages kit / source / pattern / pad replace the ADVANCED drawers.
- Four knobs for the selected pad (like the loop [orange], volume, pitch, attack); loop / your kit switch.
- Round orange BUILD MY KIT key, play, undo. Eight clean pads (4 x 2): a shape glyph per sound (kick filled circle,
  snare ring + dot, hats dots, rim triangle, tom square, clap x, shaker lines) + name + sample + a thin bar that lights on hits.
- Dark only (long sessions). No wood, no amber, no light theme.
- Still to draw before code: the source / pattern / pad display pages, the pad hover state (< > suggestions, lock, M, S, %).
- Merge in the ChatGPT reviews: undo history, raw vs matched audition, compare at the same position, status labels,
  lock apart from M/S, archive health, simpler start (auto region + one ANALYSE), tempo x2 / /2, arrow keys.

## Then
- Pouria uses v2.0 for a few days, notes every place he hesitates -> v2.1.
- Later project: "Audio manager" VST (Splice-like browser for his own archive; splicedd UI; key/BPM/sync/drag).

## v2.0 access map (every 1.14 function has one fixed home; no menus, no drawers)

Rule: the display is the only place for details. Hover and right-click are shortcuts only;
everything they do is also on a display page. Pages are one level deep:
kit · source · pattern · pad (+ pad ···) · samples · kits.

| 1.14 function | v2.0 home |
|---|---|
| LOAD / drop loop | drop on the device; source page |
| selection flags, SNAP (ALT), ANALYSE / RE-ANALYSE | source page (drag on the wave, "snap to hits", "analyse this part") |
| SENS, BPM (÷2 ×2), GRID, MASTER | source page: the 4 knobs become tempo / sensitivity / grid / master |
| EXACT / SYNC / TIGHT | pattern page, 3-way switch |
| DRAG MIDI | hardware key "midi" next to play / undo (drag it) |
| DRAG WAV | drag the pad itself; "drag original" on the pad page's yours card |
| EXPORT | kits page: "export all · wav + midi" |
| KITS: save / open / delete / open folder | kits page |
| BUILD MY KIT | orange key |
| < n/N > suggestions | pad hover; "next" chips on the pad page |
| lock | lock icon always visible on a locked pad; toggle on hover and on the pad page |
| A learned / B best hit / C yours | the 3 source cards on the pad page (click = plays it) |
| "show its hits" (A) | card 1 lights its hits on the loop |
| open folder of the sample (C) | card 3: open folder · drag original |
| MATCH TO A / MATCH n% | "like the loop" knob (amount) + raw / matched switch |
| match parts EQ SHAPE ROOM PITCH LENGTH DROP SATURATION WIDTH | the 8 pills on the pad page (dashed = not needed) |
| MATCH AGAIN / REMOVE MATCH | pill "match again"; raw = match off |
| LAYER MATCH / LAYER AGAIN / REMOVE LAYER | layer line on the pad page: "+ add layer" or "change · remove" |
| BROWSE, FOLDERS / SIMILAR, + - UP folder, RESCAN, < n S, MORE, USE ON PAD, TRY IN KIT, OPEN FOLDER | samples page |
| envelope START / FADES + curves / END / GAIN, FIT/FULL | drag on the pad page's wave; "reset" text replaces the reset menu |
| VOL PITCH MATCH amount + ATTACK | knob bank 1 |
| PAN PUNCH SUSTAIN TONE | knob bank 2 (1 / 2 switch above the knobs) |
| pad type, move hits into another pad, clear sound, delete pad | pad ··· page (delete = hold) |
| empty pad: add a sound | click it = samples page filtered for that type; drop a file still works |
| m / s | pad hover (new in 2.0) |

## v2.0.0 — built (ReKit 2, its own plugin next to ReKit 1.14)
- Separate plugin: PRODUCT_NAME "ReKit 2", PLUGIN_CODE Rkt2, installer ReKit2-Setup (own AppId; ReKit.vst3 is left alone).
  1.14 stays on the branch `rekit-1.14`. Both share Documents/ReKit (kits, sample folders, the library).
- Engine unchanged. Small processor additions for the UI: undo (UndoManager on the parameters), m / s per pad,
  MIDI preview position, suggestion names / scores.
- New editor (PluginEditor.h): the black device, display pages kit / source / pattern / pad (+ ···) / samples / kits,
  no popup menus and no dialogs (hold to delete, "replace it?" on the save button), 4 knobs in 2 banks
  (source page: tempo / sensitivity / how tight / master), keys: build my kit, play, undo, midi (drag).
- Fonts embedded: Inter + Space Mono (OFL, Assets/fonts).
- Seen in the test (engine, not UI): after BUILD MY KIT a pad can show "88 -> 84" (matched lower than its sample
  as it was) - the never-worse rule of MATCH is not applied inside BUILD. To look at in 2.0.x.
- Honest limits: undo covers knobs and the sound's shape (not sample choices: < > and the pad page do those);
  the build animation shows the real % but the rows lock in together at the end (the engine reports one %).

## v2.1.0 — first round of ReKit 2 feedback
- Kit page = the sounds (a row per pad: sample, its wave, how near, < n/N >, open). Pattern page = the rhythm:
  every bar, wheel zoom, edit hits (click = add on the grid, drag = move, up/down = how hard, right-click / delete
  = remove), undoable; dots belong to where they are (no extra kick at the end of a bar).
- BUILD MY KIT builds from what you hear: pads that play your sample are kept, loop's / best-hit pads are built
  (from that sound). Lock = keep the loop's own sound. Hint over the key: how many it builds / keeps.
  Never worse inside BUILD too (a sample nearer as it is plays as it is).
- Files are what you hear: drag WAV, export, the kit's files and its preview include volume, pan, pitch.
- Pad page: loop | raw | matched (loop only listens, the pad keeps what it plays). Sound editor: fixed view.
- Samples: path back (< Kick  Samples), one "like my sample" switch (else: like the pad's card), keep on pad,
  "trying on ..." notice. Trim flags grab at their dot. Bigger display, smaller knobs / pads.

## v2.2.0
- Display full width; bottom: keys | 8 pads (128x84) | 8 knobs in 2 rows (no banks) + loop / your kit.
- BUILD MY KIT = the magic key again: every pad from scratch except locked ones (manual choices lock).
- Pattern: orange cursor (drag on the strip above the rows), play / stop and zoom - + on the page, play starts at the cursor.
- Pad page: "loop" lights while it sounds. Kit page: only real sounds + "add a sound".

## v2.3.0
- Scrollbar under zoomed views (pattern, source).
- ROOM knob (instead of attack): 0 = dry, loop = the measured room, 200 = twice as wet; the room is now the Suno Chain FDN reverb, sized for drums.
- Undo key also undoes BUILD MY KIT (after the knob / pattern steps that came after it).

## v2.3.1
- LOOPS page: your loop folders inside ReKit, click = listen as it is, load this loop / other file.
- ROOM above "loop" is a send after the shape: works on raw / unmatched sounds too, deeper (up to ~dry level), its tail rings past END.
- Next (2.4, engine): closed hats were labelled O-HAT (OLIVER_100 808 loop) -> label by decay; clap+snare layered (burst pattern) -> CLAP / snare+clap, bursts = one hit; kick doubles 12-34 ms; loudness per instrument band.

## v2.3.3
- FIX: reopening a project lost every sample of yours that had no layer (unembedResult cleared it; since 1.13). Tested: build, save, reopen = same samples, same sources, same matches.

## v2.9.2
- Window size 50 - 200 %: - / + next to the version (steps 50 60 70 80 90 100 110 125 150 175 200), the corner
  still drags freely. The size you used last opens next time (any project).

## v2.9.1
- "by pouria motabean" under the rekit logo.

## v2.9.0
- Matching kicks: the loop's kick carries the hats / snare on it as extra highs - one of yours with fewer highs is
  hardly further away. Truth test: the real kick ranked first in 5 of 6 loops (was 2); BUILD exact 4 / 6 (was 3).
- Matching an open hat cut by the next closed one: what comes after the cut does not count. Real open hat now
  ranked first in both trap loops; BUILD picks an open hat (was a closed one).
- Undo key glows orange for 4 s after BUILD MY KIT (you can go back).
- Old analyses: once per session the folders of loops not used for 30 days, past the 40 newest, go to the bin
  (Kits and the library index never). Projects keep their kits inside them.
- Engine "drumpeel-6": a loop opened from disk that was analysed by an older engine is analysed again (2.4 - 2.8
  improvements reach loops analysed before). Projects are unchanged.

## v2.8.0
- BUILD: snare + clap pairs. For a snare pad, every snare of yours with the claps that fit best, summed at 4 mix
  ratios; taken when clearly (1.5) nearer than one sample. Truth test: rimshot_street + clap jam_one found exactly.
- Open hats: in a hat pad, accented hits that still ring 40 - 70 ms on get an O-HAT pad (their re-found ringing
  removed); kept only where the loop is modelled at least as well. Trap loops: open hats 0/6 -> 6/6.
- 16 real loops: identical to 2.7. Truth bench: hits 81.4 -> 82.6 %, names 39 -> 41 of 48.
- Known: an open hat cut by the next closed hat is short - the library pick for it is then a closed hat.

## v2.7.0
- Truth bench (tools/gtbench): 12 loops made from the user's OLIVER one-shots, pattern and samples known.
- Kick: a woody / clicky kick was renamed by the low-end rule; now a kick is "a sound that raises the low end"
  (46 ms before / after, +6 dB) - kicks found: 100 % (was 2 loops with none).
- Kick + snare on the same beat (pop 2 and 4): the combined sound is split into the kick and a snare; four-on-the-
  floor loop: null 70 % -> 99 %, snare found.
- Percussion whose hits add only highs is a hat (live hats). Hat hits learned on the snare / kick pad move to the
  hat pad where that models the spot as well (the user's stem loop: snare 6 -> 4 hits, hats 11 -> 13, exact).
- Matching hats: the bands below 1 kHz hardly count (the loop's hat loses them to the leftovers cleanup): the
  trap loop now picks the real hat (was a snap).
- Library test (6 loops, exact sample found): hats 4/6 (rest the same series), kicks 3/6, snares 2/6.
- Tried and left off: a gentler hat cleanup (hurt 4 hat loops) and a tail gate (hurt real loops' room).

## v2.6.0
- BUILD loudness bug: when your sample plays as it is (matching would sound further away), its GAIN was reset to 0
  dB - it played at its own level (kick far too loud / quiet). Now it is still brought to the loop sound's loudness.
- BUILD layers: a snare pad also tries your nearest claps and snares as the layer (clap + snare of the loop);
  taken only where the pair is clearly nearer than one sample.
- Pattern velocity: over a hit the cursor changes and its velocity shows; drag up / down = velocity follows the
  mouse (1 per pixel, SHIFT fine), a meter beside the hit and the value at the mouse.

## v2.5.0
- Tempo: a loop is 1/2/4/8/16 bars - a tempo giving 12 or 6 bars is a triplet-grid error (ZEN 116: 174 -> 116).
- Tempo: a number in the file name ("_66_", "110bpm") that is the found tempo or 1/2, 2, 2/3, 3/2 of it wins.
- Hats: a soft hat (attack 13-20 ms) was named SHAKER; now a shaker only when the slow-attack sounds play dense
  (4+ a second). Synthwave 110 gets its C-HAT pad (was SHAKER); percussion loop keeps SHAKER.
- Matching hats: a candidate more metallic (less noise in the body) or harsher in the air than the loop's hat
  counts three / two times as far; softer / smoother hardly matters.

## v2.4.0
- Engine: a "kick" whose hits add nothing below 250 Hz (it learned its low end from the ringing of the real kick)
  is relabelled hat / percussion. Mumbai 93: null 13% -> 62%; zodiac 43% -> 67%; 808 loop 42% -> 46%;
  breakbeat now gets its hat pad and the right tempo (96, was 192).
- Engine: double notes of one drum within 30 ms are merged only where the model of that spot does not get worse
  (claps learned as two bursts keep both). Duplicates over 13 loops: 79 -> 25.
- Regression set: 13 loops (tools/peel_cli + run.sh). Known: Mumbai top band 49.6 -> 41.1 (rest much better),
  breakbeat null 57.3 -> 55.5 (hat pad found instead).

## v2.3.4
- First run: "where should ReKit keep its data?" (choose a folder = what is there now moves with it, or keep Documents/ReKit). Later: kits page > data folder. Location in <app data>/ReKit/location.txt.

## To do next (Pouria, 2026-10-23)
- PATTERN velocity: hovering a dot = the mouse cursor itself shows it can be moved (up / down = how hard, sideways = time) + the value next to it; select a dot, change it with the mouse.
- Engine 2.4 (with the regression harness: 10 old loops + OLIVER_100 808, nothing may get worse): hat class by decay (closed / open / crash), metallic hats penalised only when the loop is softer, clap+snare = one sound -> layered build, clap bursts = one hit, kick doubles, loudness per instrument band.
