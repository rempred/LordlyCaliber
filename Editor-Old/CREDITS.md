# Credits

LordlyCaliber is a fan-made mod editor for *Ogre Battle 64: Person of Lordly
Caliber*. It would not exist without the work below.

## Editor

- **rempred** — project coordination, test cases, emulator verification, and release judgment.
- **Claude** — AI-assisted reverse engineering, implementation, documentation, and maintenance.
- **OpenAI Codex** — AI-assisted reverse engineering, implementation, documentation, and maintenance.

## Reference data and assets

- **Zargata** — creator and maintainer of the
  [Ogre Battle 64 Archive](https://www.ogrebattle64archive.com/) — community
  reference site for game data and lore.
- **Joshua Lindquist** — [OgreBattle64.net](https://ogrebattle64.net/) —
  primary source of the item sprites in [resources/Item Icons/](resources/Item%20Icons/)
  and the shop-category icons in [resources/Item Icons/](resources/Item%20Icons/).
- **GrantWChapman** — ripped the imported OB64 portrait resource bundle used
  for the class and squad portrait PNGs in
  [resources/portraits/](resources/portraits/), and supplied the
  class-definition offset/framing correction that led to the same-class header
  migration for unit size, base HP, and HP growth.
- **[VGMaps.com](https://www.vgmaps.com/) (The Video Game Atlas)** — source of the
  scenario map art bundled in [resources/maps/vgmaps/](resources/maps/vgmaps/)
  and shown in the Scenario tab's map view. The maps were ripped and contributed
  by the VGMaps community.
- **Cralex** — author of the *Ogre Battle 64 Person of Lordly Caliber Gameshark
  Class Hacking Guide*, which supplied the authoritative 154-class mapping
  used throughout the editor (Classes tab, character struct decode).
- **Thomas Olson** — item list contributed to Cralex's GameShark guide.
- **dancing elf** — gender-code research contributed to Cralex's GameShark guide.
- **ShaneZell** — additional reference material.

## Game

Item icons in [resources/](resources/) and all game data referenced by the
editor remain © 1999 Quest Corporation, published in North America by Atlus
and worldwide on N64 by Nintendo. This editor is an unofficial, non-commercial
fan tool and is not affiliated with or endorsed by Quest, Atlus, Nintendo, or
any rights holder. No ROM or copyrighted code is included; users must supply
their own legally-obtained copy of the game.

## Third-party software (bundled)

- **[fflate](https://github.com/101arrowz/fflate)** by Arjun Barrett — used to
  decompress and re-compress RetroArch RZIP `.state` save files in the Save
  Game Editor tab. Vendored at [vendor/fflate.min.js](vendor/fflate.min.js).
  MIT license — see [vendor/LICENSE-fflate](vendor/LICENSE-fflate).

---

Contributions, corrections, and additional credits are welcome via pull
request.
