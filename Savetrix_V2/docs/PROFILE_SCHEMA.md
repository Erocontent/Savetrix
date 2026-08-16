# Savetrix profile schema V2

`schemaVersion: 2`

V2 keeps every field from V1 and adds `quests`.

Each quest snapshot contains:

```json
{
  "form": {
    "plugin": "Skyrim.esm",
    "localFormID": 0,
    "editorID": "MQ105",
    "name": "The Way of the Voice"
  },
  "category": "main",
  "stage": 200,
  "completed": true,
  "active": false,
  "running": false,
  "restorable": true
}
```

## Campaign categories

- `main`
- `college`
- `thieves`
- `dark_brotherhood`
- `companions`
- `dawnguard`
- `dragonborn`
- `civil_war`

## Safety policy

V2 exports official `Skyrim.esm`, `Dawnguard.esm` and `Dragonborn.esm` campaign-like quests.

Automatic restoration is intentionally narrower:

- completed quests only;
- quest must match the built-in storyline whitelist;
- quest must not allow repeated stages;
- Civil War is never auto-restored in V2;
- `MQ101`, `MQ102`, `MQ302` and `MQPaarthurnax` are exported but blocked from automatic replay;
- a destination quest that is already completed is left alone;
- a destination quest whose current stage is ahead of the exported stage is never regressed.

V2 does not claim to reproduce aliases, every Papyrus variable, NPC placement, cell state or every decision-specific world mutation. The quest layer is a conservative milestone replayer, not a save-file clone.
