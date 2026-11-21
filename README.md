<img src="resources/icon_256.png" width="96" align="right" alt="">

# Regemark Present

Presentation software for running church services. You build songs and slides, then send them to several screens at the same time: projectors and monitors through the graphics card, NDI over the network, and SDI through Blackmagic DeckLink cards.

## What it does (v0.2)

**Songs**
- Library with search (title, lyrics, author, CCLI #), looks you can apply to many songs, and a default look for new songs
- **SongSelect inside the app**: sign in once, click Download, and the song lands in your library (re-downloads don't duplicate)
- Import `.usr`, lyrics `.txt` and ChordPro via the menu or drag and drop, or paste copied lyrics
- **Arrangements**: named orders such as V1 C V2 C B C C, without duplicating slides

**Presentations (slide decks, separate from songs)**
- A ProPresenter-style editor: drag and resize elements on the slide with snap guides
- Text boxes with per-word formatting, plus **images** (logos, photos) and **shapes** (panels, bands, cards), with layer order, rounded corners and opacity
- **Media slides**: full-screen photos and videos (videos play with sound)
- **Import PowerPoint and PDF**: each slide becomes an image slide. This uses PowerPoint on Windows if it's installed, otherwise LibreOffice
- Per-slide transitions (cut, fade, slide, zoom) and per-element build animations

**Bible**
- Browse and download Bibles from the Zefania library inside the app (including the Afrikaans 1933/1953 and KJV), or import your own Zefania XML
- Type "John 3:16-18", "Joh 3:16" or "Johannes 3:16". Verses become slides with a reference line; show them straight away or add them to the playlist

**Running the service**
- Playlists with headers. At the end of an item, "next" moves to the next one
- **Backgrounds per slide that change as a song builds**, a media bin over your folders (e.g. Sunday Screens downloads), and crossfades
- **Media cues and announcement loops**: videos with sound, images that advance every N seconds, and looping
- **Timer**: a countdown for a set length or until a time, shown on the confidence screen. `{timer}`, `{clock}` and `{date}` work in any slide text, and there's a one-click "Service starts in" slide
- Clear all / background / text (F1 / F2 / F3) and keyboard control

**Screens and outputs**
- **Audience**, **Confidence** (current and next slide, clock, timer, notes) and **Stream overlay** (transparent lower third)
- Each screen has its own look: show or hide backgrounds and text, its own text position and style, and a band behind the text
- Send any screen to a monitor or projector, **NDI** (with alpha) and **DeckLink SDI** (fill + key) at the same time. "Identify displays" shows you which monitor is which
- Windows installer built by GitHub Actions

## Running a service

1. **Screens → Screen setup…** For each screen, tick where it should go: a monitor, NDI, or DeckLink.
2. Drag SongSelect downloads onto the window, or use **Import SongSelect files…** (Ctrl+I).
3. Create a playlist and add songs to it (double-click a song in the library).
4. Click a slide to put it live. Use Space or → to go forward and ← to go back.
5. Click **+** in the media bin to add your backgrounds folder. Double-click an item to send it live, or select slides and click **→ Selected slides** so each one changes the background as the song goes on.

## SongSelect

CCLI only opens the SongSelect API to approved partners. Until you get partner access, this app imports the files that SongSelect lets subscribers download:
- **USR**: the "Download → USR" option, which includes the metadata.
- **Text**: the "Download → Lyrics (TXT)" option.
- **ChordPro**: from a chord sheet download. Chords are removed.

Each song keeps its CCLI number, authors and copyright for CCLI reporting.

## NDI and DeckLink

- **NDI**: no build step needed. The app loads the NDI runtime when it starts. Install the free **NDI Tools** or **NDI Runtime** from ndi.video. Stream-overlay screens send BGRA with alpha, so OBS, vMix and TriCaster receive a keyable source.
- **DeckLink**: install Blackmagic **Desktop Video** on the playout machine. To build with DeckLink support, download the **Desktop Video SDK** from blackmagicdesign.com/developer, then configure with `-DDECKLINK_SDK_DIR=<sdk root>`. In CI, store the SDK zip somewhere private and set the `DECKLINK_SDK_URL` repository secret. Turn on **Fill + key** on a Stream screen to send fill on one SDI output and key on the other, for an ATEM or other switcher.

NDI® is a registered trademark of Vizrt NDI AB. DeckLink is a trademark of Blackmagic Design. SongSelect and CCLI are trademarks of CCLI.

## Building

Needs Qt 6.4 or later (Widgets and Multimedia), CMake 3.21 or later, and a C++17 compiler.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/RegemarkPresent
```

**Windows installer:** push to GitHub, and `.github/workflows/windows.yml` builds the app with MSVC and Qt 6.8, runs the self-test, runs windeployqt, and outputs `RegemarkPresent-x.y.z-Setup.exe`. Push a `v*` tag to attach the installer to a release.

Data locations:
- Library: `Documents/Regemark Present`. Each song is a `.rpres` file and each playlist a `.rplist` file, both readable JSON. Override this folder with `RP_LIBRARY_DIR`.
- Settings: `%APPDATA%/Regemark/Regemark Present/settings.json` on Windows, or `~/.config/Regemark/Regemark Present/settings.json` on Linux.

## Code layout

```
src/core     data model (presentations, slides, styles, screens), library, settings, rich text
src/render   ScreenRenderer (transitions, builds, background crossfades), text layout, MediaPool (shared video decoders)
src/show     ShowController: the single source of truth for what is live
src/output   OutputEngine (per-screen frame clock) and sinks: DisplayOutput, NdiOutput, DeckLinkOutput
src/import   SongSelect USR / TXT / ChordPro importers, PDF / PowerPoint deck import
src/bible    Zefania Bibles: loader, references (English + Afrikaans), online library, Bible tab
src/ui       main window, slide editor, screen setup, media bin
tests        headless self-test that also writes sample frames as PNGs
```

See [docs/ROADMAP.md](docs/ROADMAP.md) for what's next.
