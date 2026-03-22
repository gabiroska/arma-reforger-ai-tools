# AI Tools — Arma Reforger Mod

A collection of quality-of-life tools for singleplayer and cooperative sessions, focused on giving players better visibility and control over AI units in the game.

Arma Reforger offers solid AI systems, but many basic interaction features — like seeing your squad on the HUD or tracking allied groups on the map — are absent for AI-controlled units. This mod fills that gap with lightweight, non-invasive tools that work out of the box on any mission.

---

## Features

### Squad HUD

A panel displayed in the top-left corner of the screen listing all AI units under the player's direct command.

- Appears automatically when units are recruited, disappears when the group is empty
- Panel height adjusts dynamically to the number of units
- Shows a **"Units (N)"** title with a live count of surviving members
- Each unit entry has a green `|` marker while alive
- On death, the marker turns red and the entry fades out after ~3 seconds
- Units that leave the group while alive are removed immediately
- Resolves character names using the game's identity system (`GetFormattedFullName()`), with fallback to entity name

### Allied Group Map Markers

Displays a map marker for every allied AI group currently active in the mission, positioned at the centroid of its living members.

- One marker per group (not per unit), updated every 2 seconds
- **Player's commanded group** — faction color + `LIGHT` infantry icon, labeled *"AI Units"*
- **Other allied groups** — faction color + `INFANTRY` icon, labeled with their callsign (e.g. *Alpha-1-1*) or custom name set in the mission editor
- Faction color is resolved automatically: BLUFOR (blue), OPFOR (red), INDFOR (green)
- Markers are local-only — visible only to the local player, not broadcast over the network
- Works with groups placed directly in the mission editor, not just "playable" groups
- Markers are removed automatically when a group has no living members

---

## Compatibility

| Scenario | Status |
|---|---|
| Singleplayer | Fully supported |
| Cooperative (listen server / host) | Supported |
| Dedicated multiplayer server | Partial — map markers may not receive all agents depending on server replication |

> The mod uses `modded SCR_AIWorld` to track agents. In dedicated multiplayer, client-side agent callbacks may not fire for all remote agents.

---

## Installation

1. Subscribe to the mod on the Arma Reforger Workshop *(link coming soon)*
2. Enable it in the mod list before starting a session
3. No additional configuration required — all features activate automatically when you take control of a character

---

## Planned

- **Unit role icons** — display a role badge (rifleman, medic, engineer, etc.) next to each unit name in the HUD
- **Incapacitated state indicator** — show unconscious units in yellow instead of removing them immediately, since they can still be revived
- **Configurable HUD position and size** — let players move and resize the squad panel to avoid conflicts with other mods
- **Tactical state indicator** — show each unit's current behavior (patrolling, moving, in combat, in vehicle) alongside their name
- **Coalition mod compatibility** — ensure AI Tools works alongside Coalition Squad Interface (CSI) without UI conflicts

---

## License

MIT