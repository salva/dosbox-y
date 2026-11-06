# VGA live text consuming extents — 2026-11-06

The original glyph producer now checks the complete destination extent before
writes: 16-pixel prefix minus panning, optional extra panned block and 8/9 dots,
in units of its byte or packed32 template. Raw snapshot owns a separate output
allocation and is intentionally not checked against TempLine. SuperMegazeux
separately checks expanded packed32 storage and the even-tail historical nibble.
It retains the upstream width<4 no-expansion branch and existing recurrence.
An impossible live extent terminates the operation/session with E_Exit rather
than overwriting memory. No producer runs during import, no clock or mode replay.

This ports selected74b5's concrete complete-producer-extent safety to the owning
original primitive, without its optional context/duplicate record design.
`tests/original-foundation/vga-text-bounds.py` extracts both actual guards and
checks tight valid/invalid 8/9-dot, panning extra block/17, large blocks, odd SMZX,
null storage, small width and independent raw snapshot under ASan/UBSan.
It is a boundary test, not guest pixel or recurrence-format qualification.

`proc-e93a8a9181a2` && exit0: initial guards, both configured builds, four combined
fresh FILE/INLINE stopped-save/held-restore/explicit Run/root42 E2Es and real
mid-DAC continuation. Final compatibility refinement preserves width<4;
`proc-92922daebd1f` && exit0 reruns guard sanitizer, both builds, all four combined
E2Es (`e2e-milestone-l5t8whww`) and diff check. Final Lua SHA256
`a02117a8de1691857c1f87a13356ffe3bfa508dd9fe927ec005a898a6bfdf729`, native
`144c859698be30ba05f4153097aa16f893e39bdf902560acfea5f412a2ab1a57`.
Build config/toolchain unchanged from INT10-APERTURE-FILELPT-CONTINUATION.md;
card work logs `text-bounds-{lua,native}.log`. Codec layout remains engine20.

Still unresolved: raw TempLine native-channel history is not established to
match selected logical producer history on a differently packed destination.
Selected `VGA_LiveTextFeedback` tracks source shifts/nibble/tail and uniform-fill
history across text, BIOS overlay and debug XOR writers. Original serializes
scratch but not that packing provenance; applied DAC RGB conversion alone is not
proof of equivalence. This is remaining implementation/qualification, not a
reason to delete ordinary renderer/backend functionality or silently require
identical binary/config/SDL packing.
