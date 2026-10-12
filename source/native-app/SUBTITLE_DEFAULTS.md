# Automatic subtitle defaults in 0.5.19

Previously, an addon response could arrive before the file opened. Automatic
selection chose that addon and became final; prepending the file's tracks later
preserved the early addon choice. The race also made same-language behavior depend
on whether audio metadata was available when the addon answered.

Automatic selection now waits until the embedded track menu has been built. It
searches matching in-file tracks first, in the user's preferred language order,
then a file-declared untagged default, then matching addon tracks. Full captions take priority over forced-only tracks;
the file's default disposition and non-SDH tracks break ties. A file-declared untagged default is the only untagged fallback.
It never guesses a foreign subtitle from its list position. The enabled automatic
subtitle setting applies even when the preferred subtitle language is also the
audio language. Explicit selections, including Off, stop automatic selection for
that playback session and are never undone by late addon responses.

Cached external subtitle text is retained in memory, so a quick Matroska input
reopen on the first seek can restore the selected libass track and its styling.
Nothing is written to disk or carried between unrelated playback sessions.

Validation: regression tests cover early addon arrival, manual choice/Off,
preferred-language fallback, known foreign/untagged tracks, full/default/forced
caption ranking, and external styling restoration. The production player lifecycle
fixture now includes an English embedded track with default disposition.
