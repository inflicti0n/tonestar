# Third-party notices

The following works are included or linked. Their licenses still apply.

## JUCE 8.0.15

- Copyright (C) Raw Material Software Limited
- License: GNU Affero General Public License v3.0
- Source: fetched at configure time from https://github.com/juce-framework/JUCE
- Used as the application framework (audio, UI, DSP). ToneStar is an AGPL
  build, so the JUCE splash screen is disabled (`JUCE_DISPLAY_SPLASH_SCREEN=0`).

## Steinberg ASIO

- Copyright Steinberg Media Technologies GmbH
- `JUCE_ASIO=1` includes Steinberg ASIO headers via JUCE.
- Steinberg offers ASIO under a proprietary developer agreement **or** under
  GPLv3. This repository uses the GPL-compatible path so the headers can ship
  with an AGPL program.
- ASIO is a trademark and software of Steinberg Media Technologies GmbH.
  Do not use Steinberg logos or imply endorsement.

## Gaegu

- Copyright 2018 The Gaegu Project Authors
- License: SIL Open Font License 1.1
- Full text: [`assets/fonts/OFL.txt`](../assets/fonts/OFL.txt)
- Embedded in the binary. OFL fonts may be bundled with software; they may
  not be sold by themselves.

## Space Grotesk

- Copyright 2020 The Space Grotesk Project Authors
- License: SIL Open Font License 1.1
- Full text: [`assets/fonts/SpaceGrotesk-OFL.txt`](../assets/fonts/SpaceGrotesk-OFL.txt)
- Same OFL embedding rules as Gaegu.

## SADIE II HRTF (University of York)

- Copyright 2018, University of York
- License: Apache License 2.0
- Compact extract of SADIE II KEMAR D2 at azimuth 0°, elevation −15°,
  measured at 1.2 m (Audio Lab, University of York: Cal Armstrong,
  Lewis Thresh, Gavin Kearney).
- Files: [`assets/hrtf/`](../assets/hrtf/)
- NOTICE: [`assets/hrtf/NOTICE`](../assets/hrtf/NOTICE)
- Full license: [`assets/hrtf/LICENSE.txt`](../assets/hrtf/LICENSE.txt)
- Source: https://doi.org/10.5281/zenodo.10886409
- Paper: https://doi.org/10.3390/app8112029

## Cab impulse response

- Jester Dyne Brutal IR Pack, Cookie Monster
  (Celestion Vintage 30 + Shure SM57, 48 kHz)
- License: CC0 / public-domain dedication as stated on the product page
- Vendor note: [`assets/irs/LICENSE`](../assets/irs/LICENSE)
- Source: https://www.jester-dyne-productions.com/brutal-ir-pack/
- ToneStar ships a trimmed (~32 ms) mono fold as cone colour only.
  Celestion and Shure names describe the measured hardware. They are not
  endorsements.

## Discord Rich Presence

- ToneStar speaks Discord's public local RPC (named pipe / UNIX socket
  `discord-ipc-0` … `9`) to set Rich Presence. No Discord SDK or binary
  is bundled.
- Protocol: https://discord.com/developers/docs/rich-presence/how-to
- Discord is a trademark of Discord Inc. This is not an official Discord
  product and does not imply endorsement.

ToneStar is licensed under the GNU Affero General Public License v3.0
(see [`LICENSE`](../LICENSE)). Copyright 2026 ToneStar authors.
