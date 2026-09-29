import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "prepare_measured_multichannel.py"
REPO = SCRIPT.parents[1]


class MeasuredPreparationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.devices = self.root / "devices.csv"
        with self.devices.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=["device", "longitude", "latitude", "modal_sf"])
            writer.writeheader()
            writer.writerows([dict(device="TEST1", longitude=19, latitude=50, modal_sf=12),
                              dict(device="TEST2", longitude=19.001, latitude=50.001, modal_sf=11)])

    def prepare(self, parameters=None):
        command = [sys.executable, str(SCRIPT), "--devices", str(self.devices),
                   "--repo", str(REPO), "--output", str(self.root / "output")]
        if parameters is not None:
            path = self.root / "parameters.json"
            path.write_text(json.dumps(parameters), encoding="utf-8")
            command += ["--parameters", str(path)]
        return subprocess.run(command, capture_output=True, text=True)

    def test_draft_uniform_no_guessed_gateway(self):
        self.assertEqual(self.prepare().returncode, 0)
        manifest = json.loads((self.root / "output/scenario_manifest.json").read_text())
        ini = (self.root / "output/piekary_uniform.ini").read_text()
        self.assertEqual(manifest["data_channel_probabilities"], [0.125] * 8)
        self.assertEqual(manifest["status"], "DRAFT_MISSING_PARAMETERS")
        self.assertIn("*.gwX = ask", ini)
        self.assertIn("**.applicationPayloadLength = ask", ini)
        self.assertIn("*.endnode[1].**.forceSF = 11", ini)

    def test_complete_parameters(self):
        p = dict(gw_longitude=19, gw_latitude=50, payload_bytes=51, tx_power_dbm=14,
                 frames_per_window=2, ack_every=5, windows_per_day=2,
                 window_start1_minutes=0, window_duration1_minutes=720,
                 window_start2_minutes=720, window_duration2_minutes=720)
        self.assertEqual(self.prepare(p).returncode, 0)
        manifest = json.loads((self.root / "output/scenario_manifest.json").read_text())
        self.assertEqual(manifest["status"], "READY_FOR_ASSUMPTION_REVIEW")
        self.assertEqual(manifest["missing_parameters"], [])
        self.assertNotIn("= ask", (self.root / "output/piekary_uniform.ini").read_text())

    def test_reject_malformed_parameters(self):
        for p in [dict(gw_latitude=100), dict(ack_every=0), dict(frames_per_window=1.5),
                  dict(window_start1_minutes=1400, window_duration1_minutes=720), dict(channel_weight=0.2)]:
            with self.subTest(p=p):
                self.assertNotEqual(self.prepare(p).returncode, 0)


if __name__ == "__main__":
    unittest.main()
