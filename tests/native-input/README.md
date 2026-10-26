# Native input regression

`fixtures.py DIRECTORY [NATIVE_MV_HEVC]` creates synthetic address-coded RGB
anaglyph controls, a custom st3d declaration on a wide raster, and optionally
an MV-HEVC variant with container SAR 2:1. No film bytes or file-name guesses
supply stereo identity. RGB reference equations are independent of the library.

`check.py CASE FIXTURES COLOUR-PAIR [--swap]` checks both reconstructed eyes at
source resolution, with the existing tolerance of one 8-bit channel value.
The original encoded channels survive; other channels use the shared approximate
reconstruction. This cannot recover lost full-colour views or undo an arbitrary
anaglyph encoder, clipping, or baked-in cross-talk.

`lifecycle.cpp` runs against installed Qt Multimedia and independently decoded
raw eye planes. It checks all sampled YUV values, pair size, source aspect,
view identity, timestamp order, seek, replacement, track changes, cancellation,
EOF and a two-unrelated-tracks control. Run with both `native` and `default`.
The latter retains the ordinary consumer's mono SAR behavior.

`check-multiview.py CASE REFERENCES [--swap]` converts each independent decoded
YUV420P eye using the player's existing centre-sited chroma interpretation and
the BT.709 limited-range equations. It checks all rendered channels, including
the seam, at tolerance one. This RGB conversion assertion is separate from the
exact raw-plane equality in the lifecycle/shared-pair tests; it does not claim
recovery of pre-encoding RGB or test other chroma-siting interpretations.
The source fixture declares BT.709; other transfer/range/precision controls stay
in the retained frame-path suite. Swapping expected eyes must fail.

`declared.cpp` prevents custom, unknown and not-yet-assembled native declarations
from falling through to a shape guess. Actual decoded pair metadata can promote
a native source to SBS. Truly unmarked files retain the shared shape policy.

`case.sh` and `session.sh` are bounded, private KWin test sessions using the
existing native-buffer recorder. They do not change the retained strip/fullscreen
harness or its proxy. Supply STRIP_BINARY, STRIP_CLIP and optional STRIP_INPUT or
STRIP_SWAP; CASE_DIR and the private HOME/runtime directories belong to the case.
Use a fresh container/session and match installed ELF hashes to the archives
before interpreting a packaged result.
