# slice-sequencer

A 16-step sequencer for the [Electrosmith Daisy Patch](https://www.electro-smith.com/daisy/patch), optimized for sequencing sliced samples (primarily drum breaks). A quick disclaimer, I'm not a C++ expert, so coding agents were heavily relied upon while writing this. But I've been using it in my own patches for a while now, and it's incredibly good at re-arranging drum patterns.

## Overview

Unlike traditional 16-step sequencers, this does not output notes on a scale. On each step, it will instead output a value between 0-5v that corresponds to either a "slice" from a sequence or a position in an unsliced sample. It works well in conjuction with modules like the morphagene that have a built-in way to slice samples, but could also work with other samplers by using the output to determine position in a sample and the gate output to trigger a restart on that sample.

On each clock pulse the module advances one step and outputs:

- **CV Out 1** — slice / note value (see Settings → Slice Out)
- **CV Out 2** — note length
- **Gate Out** — trigger on note starts
- **Audio Out 1 / 2** — optional kick / snare hit gates

CV knobs 1–3 apply non-destructive Shift, Transpose, or Repeat to a subsequence. CV 4 selects among the original sequence and up to 15 saved patterns. The OLED shows the current baseline (top) and the CV-modified sequence (bottom), with the playhead on the bottom row.

### I/O summary

| Patch jack / control | Role |
|----------------------|------|
| Gate In 1 | Clock (advance one step) |
| Gate In 2 | Reset (return to step 1; applies on next clock if received alone) |
| Ctrl 1–3 | CV modifiers (configured in CV1–CV3 menus) |
| Ctrl 4 | Pattern select (knob disabled while a CV menu is open) |
| CV Out 1 | Slice CV |
| CV Out 2 | Length CV |
| Gate Out | Slice-start gate |
| Audio Out 1 / 2 | Kick / snare hits |
| Encoder | Navigate menus; press to select / confirm |
| SD card | Stores sequences (`seq/*.seq`), last-loaded sequence, and global settings |

## Main menu

Turn the encoder to cycle menu items. The current item is shown on the top line. Below it:

1. Baseline sequence (original or selected pattern, no live CV mods)
2. Separator containing the name of the currently loaded sequence
3. Modified sequence with the current step highlighted

Press the encoder to enter a submenu.

## Submenus

### Edit Sequence

Edits the **original** sequence only (not patterns). Defaults to a note on each step, in order from 0-9, then A-F.

- **Total Steps** — sequence length from 2–16. Shrinking/expanding keeps a snapshot of note lengths where possible.
- **Assign** — `Length` or `Hits`.
  - **Length:** on the sequence row, press to select a note start, then turn to lengthen/shorten that note (steals/returns steps from/to the end).
  - **Hits:** assigns `None`, kick, or snare to a note (drives audio outs); useful for doubling kick and snare parts on a drum sample.
- **Back** — return to the main menu.

This effectively determines how to slice a sample. For example, the Amen Break might look like: `0-1-2-34567890AB`, with notes 0, 1, and 2 representing the first two kicks and snare as 8th notes, with the remaining notes as 16ths.

### CV1 / CV2 / CV3

Each maps to Ctrl 1–3. While any of these menus is open, pattern selection from Ctrl 4 is locked.

- **Type** — `Disabled` (default; ignore this CV), `Shift`, `Transpose`, or `Repeat`
- **Position** — starting note index of the affected subsequence
- **Size** — length of the affected region in steps (1–8)
- **Back** — save and unlock pattern select

Press the encoder on a field to edit it; press again to confirm. The masked sequence row shows which notes fall in the segment.

Mods are non-destructive: the baseline stays intact, and the bottom row / outputs reflect Shift → Transpose → Repeat applied in that order. Knob travel around the center is bipolar (below center one direction, above the other).

### Patterns

Manage variations of the current sequence. Patterns are the same length as the original; Ctrl 4 divides 0–1 across (original + patterns), with the lowest band always selecting the original.

- **Add Pattern** — bake the current CV-modified sequence as a new pattern (max 15). CV assignments are copied with positions remapped; knob center then restores this baked baseline. Preview shows the modified sequence.
- **Delete Pattern** — delete the currently selected pattern (no-op on the original). Preview shows the unmodified baseline.
- **Order Patterns** — list original + patterns. Select a pattern and press to move it up/down; press again to lock the new order. The original sequence cannot be moved.
- **Back** — main menu

Pattern changes from Ctrl 4 update the display immediately but take effect on playback at the **next** clock step. Mid-note switches can hold the previous note across a `-` in the new pattern; otherwise silence until the next note start on the new pattern.

Each pattern has its own CV1–3 settings. Edit Sequence always shows the original sequence.

### Save

Name the sequence (`a–z`, `0–9`, `_`) and write it to the SD card under `seq/<name>.seq`, including patterns and per-pattern CV data. Also remembers this as the last-loaded sequence.

### Load

Scroll saved names, or press **Back** to return to the main menu. Press to load a sequence and restore it as last-loaded. On boot, the last-loaded sequence is restored automatically when present.

### New

Confirm to clear the current sequence, patterns, and last-loaded pointer, and start a fresh default sequence.

### Settings

Global options (stored in `seq/settings.bin`, independent of which sequence is loaded). Not much here now, but I may decide to add to this in the future.

- **Slice Out**
  - **Note** (default) — Sets the CV Out 1 to output the slice value (A division of 0-5v representing each note).
  - **Step** — Sets the CV Out 1 to output the step value of each new note (first step is always 0; longer notes jump further in the CV range).
- **Back** — main menu

Press on **Slice Out** to toggle Note/Step; press again to confirm.

## Building

### Requirements

- [Daisy Toolchain](https://github.com/electro-smith/DaisyWiki/wiki/1.-Setting-Up-Your-Development-Environment) (`arm-none-eabi-gcc`, `make`, `dfu-util`)
- This repo with `libDaisy` and `DaisySP` present (use `git submodule` to pull in these dependencies)

Build the libraries once if needed:

```bash
make -C libDaisy
make -C DaisySP
```

Then from the project root:

```bash
make
```

The firmware is configured with `APP_TYPE = BOOT_QSPI` because the binary does not fit in the Patch’s 128 KB internal flash. Output lands in `build/`.

## Flashing (Daisy Patch)

USB must be connected to the Patch’s **Daisy Seed** USB port.

### One-time: install the Daisy bootloader

1. Enter STM32 DFU mode: hold **BOOT**, tap **RESET**, then release **BOOT**.
2. From the project root:

```bash
make program-boot
```

You only need this again if something overwrites internal flash (for example flashing a normal `BOOT_NONE` app).

### Every firmware update

1. Build: `make` (or `make clean && make` after big changes).
2. Reset the Patch so the bootloader runs (onboard LED **pulses**).
3. During that window, optionally press **BOOT** to stay in DFU, then:

```bash
make program-dfu
```

Use the **Daisy bootloader** DFU (pulsing LED), not the Boot+Reset STM32 ROM DFU used for `program-boot`. Writing a QSPI app while in ROM DFU fails with errors such as “Last page … is not writeable”.

After a successful `program-dfu`, the Patch should reboot into slice-sequencer.

## SD card notes

- Format the card so FatFS can mount it (typical FAT32).
- Sequences live in `seq/*.seq`.
- Global Slice Out setting is in `seq/settings.bin`.
- Without a card, the sequencer still runs; save/load/settings persistence will not.
