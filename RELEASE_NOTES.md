# Nuvio Max'd 1.0.0.19 — detail actions and controller hints

- Options on a movie/series detail page opens Title actions: add/remove library membership, movie watched/unwatched, series/season watched actions and trailers.
- Library changes sync to the Nuvio profile and the supported selected Library tracker (Trakt/MDBList). SIMKL library remains read-only here; its history is not removed by a library action. Watched marks use the selected Watch Progress service.
- Bulk actions show a confirmation/count and affect listed released episodes, with bounded scanning and no silent truncation. Offline changes are retained separately from playback progress.
- Triangle's episode watched action is now shown in the hints. Cast/person screens use controller icons, with Circle/R2 on the cast row.
- New action labels are translated in all eight menu languages. Existing USB import, profiles, playback and menu-language support are retained.

Real-account sync and PS5 behavior for the new actions need confirmation. Resume positions remain separate from manual watched marks. Fully close Max'd before updating, preserve saved data and do not overwrite a mounted FFPFSC. Title PPSA99288; contentVersion 07.000.019.
