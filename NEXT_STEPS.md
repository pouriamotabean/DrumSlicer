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

## Then
- Pouria uses v2.0 for a few days, notes every place he hesitates -> v2.1.
- Later project: "Audio manager" VST (Splice-like browser for his own archive; splicedd UI; key/BPM/sync/drag).
