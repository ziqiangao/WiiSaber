# Beatmap light parsing

`BeatmapLights.cpp` owns JSON tokenization, palette loading, event ordering, and
per-frame light playback. Version-specific event layouts are handled in
`BeatmapParserV3.cpp` and `BeatmapParserV4.cpp`, which emit the same scheduled
event records consumed by the playback code.

Supported lighting inputs:

- V3 legacy/basic events (`_events` or `basicBeatmapEvents`) and
  `colorBoostBeatmapEvents`.
- V4 `basicEvents` plus `basicEventsData`, and `colorBoostEvents` plus
  `colorBoostEventsData`. If the beatmap file declares version 4, the loader
  also resolves its `lightshowDataFilename` from the adjacent `Info.dat` and
  reads those events from the separate lightshow file.
- Environment colors from the selected `Info.dat` color scheme, including V4
  eight-digit hex colors and the two boost colors. Missing or disabled color
  overrides use the environment callback defaults.

Both versions currently feed basic color lanes and Float FX lanes into the
shared callback API. GLS event box groups are a separate format path and are
not yet converted into GLS callbacks by these parsers.

The schema handling follows the [BSMG beatmap format](https://bsmg.wiki/mapping/map-format/beatmap.html),
[lightshow format](https://bsmg.wiki/mapping/map-format/lightshow.html), and
[Info format](https://bsmg.wiki/mapping/map-format/info.html) references.
