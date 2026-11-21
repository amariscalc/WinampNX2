# WinampNX2

Play music from an SD card on Nintendo Switch 2. The classic skin pairs a green spectrum analyzer and seven-segment clock with transport controls, a ten-band equalizer and a docked playlist. The analyzer reads the PCM stream. All artwork is drawn by the application.

![WinampNX2 playing a local FLAC file](docs/player.png)

## Install

1. Download [WinampNX2.nro](dist/WinampNX2.nro) and copy it to `sdmc:/switch/WinampNX2/WinampNX2.nro`.
2. Put music in `sdmc:/music/` or any folder on the card.
3. Open WinampNX2 from a working Switch 2 homebrew menu. Press **A** on a track. Use **X** to browse other folders. Opening a file loads the playable files in its folder as the playlist, in filename order. The player opens the SD root if the music folder is missing.

The v0.1.0 binary targets **Switch 2** using ounce-devkit 20261117. The NRO was cross-compiled and its embedded metadata verified. Audio, display, touch and controller behavior on physical hardware are awaiting confirmation. A startup failure is logged to `sdmc:/switch/WinampNX2/startup-error.txt` when the application directory is writable.

## Audio

| Family | Decoders compiled in this build |
| --- | --- |
| Lossless | FLAC, ALAC, WavPack, Monkey’s Audio (APE), WMA Lossless and PCM |
| Lossy | MP3, AAC, Opus, Vorbis and WMA v1/v2 |
| Containers | FLAC, MP3, raw AAC, MP4/M4A, Ogg, WAV, AIFF/AIFC, CAF, ASF/WMA, WavPack, APE and Matroska/WebM audio |

Container support depends on the audio stream inside. Files are opened locally and decoded incrementally. The app reads title and artist tags when present and displays the active codec and source sample rate. It supports unencrypted files.

**Output** is stereo PCM at **48 kHz, 16-bit** through libounce AudioOut. Other sample rates are resampled, multichannel sources are mixed to stereo, and higher bit depths are reduced at the output. The default is unity volume, centered balance and EQ bypass. At those settings, automated desktop tests produced sample-identical PCM from lossless 48 kHz, 16-bit stereo fixtures. Lossy files retain the fidelity of the encoded source.

## Controls

| Action | Switch 2 | Desktop |
| --- | --- | --- |
| Select a row | D-pad up/down | Up/down |
| Play selection, pause or resume current track | A | Enter or Space |
| Browse SD / return to playlist | X | X |
| Parent folder, leave EQ, or stop from the playlist | B | Escape |
| Seek backward/forward 5 seconds | D-pad left/right in the playlist | Left/right |
| Previous / next track | L / R | [ / ] |
| Volume | ZL / ZR | Minus / equals |
| Toggle shuffle / repeat playlist | Click left / right stick | S / R |
| Focus equalizer | Y | Y |
| Select EQ band / change gain | Left/right, then up/down | Arrow keys |
| Toggle EQ while focused | A | Enter |
| Exit | + | Q |

Touch the transport buttons, playlist entries and folder entries directly. Drag the position, volume, balance or EQ sliders. Select **RESET** to return preamp and band gains to zero. With shuffle active, the next track is picked at random and may recur after another track.

![SD file browser with playback continuing](docs/files.png)

Both screenshots are 1280 × 720 captures of the desktop build using the same renderer as the NRO. The displayed files were generated for testing.

## Build for Switch 2

Install devkitPro devkitA64 and switch-tools, then install [ounce-devkit](https://github.com/ouncebrew/ounce-devkit/releases/tag/devkit-20261117). Set `DEVKITPRO` to that installation. Use a Linux shell with Bash, Make, curl, xz and sha256sum.

```sh
make -j4 switch
sha256sum build/WinampNX2.nro
```

The first build downloads the pinned FFmpeg 7.1.5 source, checks its SHA-256 and compiles an audio-only static library. Set `JOBS` to control the number of FFmpeg build jobs. To use an existing source archive, put it at `build/vendor/ffmpeg-7.1.5.tar.xz` before running Make. Compiler flags and the complete codec configuration are in [scripts/build-codecs.sh](scripts/build-codecs.sh). The [checksum](dist/SHA256SUMS) identifies the supplied binary.

## Desktop build and tests

Install a C++17 compiler, pkg-config, SDL2 development headers and the FFmpeg 7 development libraries for avformat, avcodec, swresample and avutil. The audio tests also use the FFmpeg command-line encoder.

```sh
make desktop
./build/WinampNX2 /path/to/music
bash tests/check-audio.sh
```

The tests exercise decoding, sample equality for lossless files, 96 kHz conversion, pause, seek, EQ controls, folder navigation and automatic track transitions using a desktop audio device in dummy mode. To capture the running desktop player and SD browser, use a folder whose first track is longer than two seconds.

```sh
./build/WinampNX2 /path/to/music --capture preview
```

## Credits

WinampNX2 is an independent homebrew project. Winamp and Nintendo Switch are trademarks of their respective owners. The classic-style graphics were drawn for this project. See [third-party notices](licenses/NOTICE.md) for the audio engine, runtime and font notices.
