import json
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "sample" / "profile.example.json"
data = json.loads(p.read_text(encoding="utf-8"))
assert data["schemaVersion"] == 2
assert data["modVersion"].startswith("2.")
assert len(data["skills"]) == 18
assert data["stats"]["level"] >= 1
for skill in data["skills"]:
    assert {"level", "xp", "levelThreshold", "legendaryLevel"} <= skill.keys()
for quest in data.get("quests", []):
    assert {"form", "category", "stage", "completed", "restorable"} <= quest.keys()
print("profile.example.json V2: OK")
