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
