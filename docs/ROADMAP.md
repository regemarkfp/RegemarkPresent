# Roadmap

## Milestone 2: editor and rendering
- **GPU compositor.** Frames are currently drawn on the CPU with QPainter, which is fine for two or three 1080p30 screens. Moving to Qt RHI (Direct3D 11 on Windows) would allow 4K, 60 fps and many outputs, and would pass video frames to NDI and DeckLink without copying them.
- **Canvas slide editor:** drag and resize text boxes on the preview, snap guides, and multi-select.
- More object types: shapes, images, logos, scrolling tickers, countdown timers, and a CCLI footer that fills itself from the song details.
- Arrangements, e.g. V1 C V2 C B C C, without duplicating slides.
- Themes: named looks you can apply to many songs at once.
- Gapless video loops. They currently use QMediaPlayer's loop, which can show a single dropped frame where the clip restarts.

## Milestone 3: service workflow
- Integration with WorshipTools / Planning Center: pull the week's service plan into a playlist.
- SongSelect API, once CCLI approves partner access.
- Props and messages layers, e.g. "Parking lot: car with lights on".
- Audio cues and playback of video clips with sound, routed to a chosen audio device.
- Stage display layouts: free-form editor, multiple layouts, timers and countdown to service start.
- Remote control from a phone or tablet over the LAN.

## Milestone 4: hardening
- Tests on real hardware: DeckLink Duo 2, Mini Monitor 4K, and an NDI → OBS round trip.
- Auto-save and crash recovery; library backup and sync.
- Code-signed installer and auto-update.
