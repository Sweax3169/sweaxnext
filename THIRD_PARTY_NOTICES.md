# Third-party notices

SweaxNext is free software under the GNU General Public License v3 (see [LICENSE](LICENSE)).
It is built on the following code, all of which is compatible with that licence.

## JUCE 7.0.12

<https://github.com/juce-framework/JUCE> · © Raw Material Software Limited

JUCE 7 is offered under the JUCE 7 licence or, for open-source projects, the
[GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.html). SweaxNext uses JUCE under GPLv3.
The `juce_audio_basics`, `juce_audio_devices`, `juce_core` and `juce_events` modules are ISC licensed.
The build fetches the unmodified `7.0.12` tag; no JUCE file is copied into this repository.

JUCE bundles code from other projects, which is compiled into the plug-in where the modules use it:

| Code | Where in JUCE | Licence |
|---|---|---|
| zlib | `juce_core/zip/zlib` | zlib licence |
| libpng | `juce_graphics/image_formats/pnglib` | libpng licence |
| IJG JPEG library | `juce_graphics/image_formats/jpglib` | IJG licence (this software is based in part on the work of the Independent JPEG Group) |
| FLAC | `juce_audio_formats/codecs/flac` | BSD-3-Clause (Xiph.Org Foundation) |
| Ogg Vorbis 1.3.7 | `juce_audio_formats/codecs/oggvorbis` | BSD-3-Clause (Xiph.Org Foundation) |
| LV2, lilv, serd, sord, sratom | `juce_audio_processors/format_types/LV2_SDK` | ISC |

## Steinberg VST3 SDK

Bundled inside JUCE (`juce_audio_processors/format_types/VST3_SDK`). The SDK is dual licensed under the
Steinberg VST3 License or the GNU General Public License v3; SweaxNext uses it under **GPLv3**.
Parts of the SDK (`base`, `pluginterfaces`, `public.sdk`) carry Steinberg's BSD-style licence text in their own
`LICENSE.txt` files.

VST is a registered trademark of Steinberg Media Technologies GmbH. SweaxNext is not affiliated with or
endorsed by Steinberg.

## Fonts and demo material in this repository

- The web page and README art use **Bricolage Grotesque** (© The Bricolage Grotesque Project Authors) and
  **Hanken Grotesk** (© The Hanken Grotesk Project Authors), both under the
  [SIL Open Font License 1.1](https://openfontlicense.org). The licence texts are in `docs/fonts/`.
- The demo vocal was synthesised with [Piper](https://github.com/rhasspy/piper) (MIT) using the Turkish voice
  `tr_TR-fahrettin-medium`, trained on a CC0 dataset. The lyrics and the beat were written and generated for this
  repository (`Tools/demo/demo_uret.py`). No commercial recording, stem or reference track is included.

## Artist-named styles

Twelve styles in the style menu carry the names of Turkish rap artists (for example "Ezhel tarzı",
*Ezhel style*). They are descriptions of the vocal sound heard on those artists' released records, written as
processing recipes. The artists, their labels and their engineers have nothing to do with SweaxNext and have not
endorsed it. The names are used only to describe a sound.
