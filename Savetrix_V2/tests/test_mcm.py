import configparser
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mcm_plugin", ROOT / "tools/generate_mcm_plugin.py")
plugin = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plugin)


class McmContractTests(unittest.TestCase):
    def test_initial_page_has_content(self):
        config = json.loads((ROOT / "Data/MCM/Config/Savetrix/config.json").read_text())
        self.assertTrue(config.get("content"), "MCM Helper draws MainPage on initial empty-page selection")

    def test_shipped_registration_matches_generator(self):
        data = (ROOT / "Data/Savetrix.esp").read_bytes()
        plugin.validate_plugin(data)
        self.assertEqual(data, plugin.build_plugin())

    def test_controls_match_ini_and_mod_name(self):
        config = json.loads((ROOT / "Data/MCM/Config/Savetrix/config.json").read_text())
        ini = configparser.ConfigParser()
        ini.read(ROOT / "Data/MCM/Config/Savetrix/settings.ini")
        self.assertEqual(config["modName"], "Savetrix")
        seen = set()
        for page in config["pages"]:
            for item in page["content"]:
                if item["type"] not in ("toggle", "keymap"):
                    continue
                key, section = item["id"].split(":")
                self.assertTrue(ini.has_option(section, key), item["id"])
                self.assertNotIn(item["id"], seen)
                seen.add(item["id"])
                expected = "ModSettingBool" if item["type"] == "toggle" else "ModSettingInt"
                self.assertEqual(item["valueOptions"]["sourceType"], expected)
        self.assertEqual(len(seen), 9)


if __name__ == "__main__":
    unittest.main()
