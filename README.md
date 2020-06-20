# CBPSSE
Modified version of [CBP Physics for Skyrim SE](https://github.com/cbpphysics/CBPSSE) by [polygonhell](https://github.com/polygonhell)

## Changes
* Decouple simulation speed from framerate
* Use a real-time, high-resolution monotonic timer in physics calculations
* Track all loaded actors instead of just those in the player cell
* Monitor for config updates using [change notifications](https://docs.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications) instead of reloading periodically
* Version independent (requires [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444))
