# =============================================================================
# STANDARDOWY OPIS SKRYPTU: test_topology_io.py
#
# DZIAŁANIE
# Testy unittest parsera: dopisywane przebiegi, stałe węzły, indeksy i zakresy,
# niezgodna liczba ED oraz pomijanie niekompletnego bloku w trybie lenient.
#
# WEJŚCIE I PARAMETRY
# Brak własnych opcji CLI; standardowe opcje unittest, np. -v, -q, -f.
# Stała SAMPLE zawiera 2 przebiegi po 2 ED i 2 stałe węzły.
# Testy tworzą tymczasowy exec_results_topology.txt i importują topology_io.
#
# PRZYGOTOWANIE DANYCH Z SYMULATORA
# Symulator nie jest wymagany. Syntetyczny eksport jest zapisany w SAMPLE
# i zapisywany do TemporaryDirectory na czas każdego testu.
# Dla danych rzeczywistych uruchom scenariusz Chapter7_T02 zgodnie
# z scripts/README.md i użyj eksportu Keeper w plot_topology.py.
#
# URUCHOMIENIE
# python scripts/tests/test_topology_io.py -v
#
# PRZYKŁADOWY WYNIK
# Raport unittest na stderr: cztery nazwy testów, „Ran 4 tests in ...s”, „OK”.
# Przykładowa średnia SAMPLE: 83.335%. Brak trwałych plików i PNG.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, biblioteka standardowa; nie wymaga Matplotlib ani danych B11.
# =============================================================================

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import textwrap
import unittest


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from topology_io import parse_topology, select_runs  # noqa: E402


SAMPLE = textwrap.dedent(
    """
    # Fixed nodes (Gateways/Relays/IPBridge)
    # Written on: Sat Sep 20 17:45:16 2025

    role,idx,x,y
    GW,0,750.00,450.00
    RELAY,0,300.00,250.00

    Sat Sep 20 17:45:16 2025

    ED             = 2
    Simulated days = 30
    idx,x,y,avg_daily_success_UL[%]
    0,500.00,400.00,100.00
    -1,200.00,300.00,66.67

    Sat Sep 20 17:45:20 2025

    ED             = 2
    Simulated days = 30
    idx,x,y,avg_daily_success_UL[%]
    0,510.00,410.00,96.67
    -1,210.00,310.00,70.00
    """
).strip()


class TopologyParserTests(unittest.TestCase):
    def test_parses_appended_runs_and_fixed_nodes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "exec_results_topology.txt"
            path.write_text(SAMPLE, encoding="utf-8")
            topology = parse_topology(path)

        self.assertEqual(len(topology.runs), 2)
        self.assertEqual(len(topology.devices), 4)
        self.assertEqual(len(topology.fixed_nodes), 2)
        self.assertEqual(topology.runs[0].simulated_days, 30)
        self.assertTrue(topology.runs[0].devices[1].is_weak_island_node)
        self.assertAlmostEqual(topology.runs[0].mean_daily_success_percent, 83.335)

    def test_selects_lists_and_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "exec_results_topology.txt"
            path.write_text(SAMPLE, encoding="utf-8")
            topology = parse_topology(path)

        self.assertEqual([run.run_number for run in select_runs(topology, "0-1")], [0, 1])
        self.assertEqual([run.run_number for run in select_runs(topology, "1,0")], [1, 0])

    def test_rejects_wrong_declared_count(self) -> None:
        bad = SAMPLE.replace("ED             = 2", "ED             = 3", 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "exec_results_topology.txt"
            path.write_text(bad, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "declares 3 EDs"):
                parse_topology(path)

    def test_lenient_mode_skips_incomplete_block_and_continues(self) -> None:
        bad = SAMPLE.replace("ED             = 2", "ED             = 3", 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "exec_results_topology.txt"
            path.write_text(bad, encoding="utf-8")
            topology = parse_topology(path, strict=False)

        self.assertEqual(len(topology.runs), 1)
        self.assertEqual(topology.runs[0].devices[0].x, 510.0)


if __name__ == "__main__":
    unittest.main()
