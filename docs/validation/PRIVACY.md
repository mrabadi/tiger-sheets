# Public validation evidence

Public copies of the validation records normalize personal source paths to
`/workspace/tiger-sheets` and redact the host identity and machine configuration.
Test outcomes, formulas, calculated values, timings, process resource usage and
executable hashes retain their recorded meaning. These redacted files are not
byte-for-byte copies of the original local logs. The original records remain in
a private local archive outside version control.

Measurements do not qualify the reference hardware configuration. Host-specific
details have been omitted, so the public records cannot establish hardware
equivalence.

Screenshot artifacts are excluded from version control. Tests check interactions,
formulas, values, formats and saved workbook contents without writing screenshots.
Rendering checks that directly verify chart geometry, visible focus or PDF output
still inspect their relevant rendered results in memory or temporary files.
