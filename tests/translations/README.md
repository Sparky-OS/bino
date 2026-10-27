# Anaglyph input translations

`python3 tests/translations/check.py` checks the seven input labels in their Gui
and Mode contexts, finished translations and placeholders. English stays in the
source. `check.py RECEIPTS` also checks JSON receipts obtained by observing the
installed application's normal QTranslator loader in each required locale.

The new Chinese catalogs retain every existing Chinese translation. Their seven
new labels distinguish Simplified and Traditional Chinese explicitly. Existing
unrelated Chinese translations are preserved verbatim, including Simplified
wording in the Traditional catalog; this bounded correction does not claim a
complete Traditional Chinese translation of the older interface. The generic
Chinese and Georgian catalogs remain unchanged. Languages with no prior catalog
translate these seven labels and fall back to English for other text.

These labels describe encoded input channels, in the same order as the English
source. They do not add display output modes or promise original colour recovery.
