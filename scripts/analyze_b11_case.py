# =============================================================================
# STANDARDOWY OPIS SKRYPTU: analyze_b11_case.py
#
# DZIAŁANIE
# Analizuje przypadek B11 z 385 ED: PDR, skuteczność dzienną, utracone ramki,
# liczniki TS011 oraz sparowane różnice między trzema wariantami z 95% PU.
#
# WEJŚCIE I PARAMETRY
# --output: plik JSON; domyślnie scripts/B11_case_385.json.
# Stała ROOT = C:/Omnet/simulation_results/review_chapter1 (brak opcji CLI zmiany).
# Wymagane trzy warianty *_350, run_00..29, każdy: exec_results.txt,
# results.sca i exec_results_topology.txt. Dokładnie 350+35 ED, 30 dni,
# 26 dni statystyki, identyczne współrzędne urządzeń w sparowanych przebiegach.
#
# PRZYGOTOWANIE DANYCH Z SYMULATORA
# Zbuduj symulator release; konfiguracja wejściowa: simulations/omnetpp_review.ini.
# Z katalogu repozytorium uruchom (900 symulacji; polecenie w jednej linii):
#   python scripts/generate_review_chapter1_results.py --groups b11_comparison --runs 30 --output C:/Omnet/simulation_results/review_chapter1 --figures tmp/b11_figures
# Zachowaj variants/b11_comparison__<wariant>_<centralne_ED>/runs/run_00..29/
# z plikami exec_results.txt, exec_results_topology.txt i results.sca,
# chapter1_summary.csv oraz katalog reproducibility. Są trzy warianty:
# ep4312388, ts011, ts011_unlimited; centralne_ED = 50..500 co 50,
# łącznie z wyspami = 55..550 co 55. Przebieg trwa 30 dni, warmup 4 dni.
# Dla wyników historycznych użyj zgodnej wersji źródeł i konfiguracji;
# nowa symulacja zmienionego modelu nie odtwarza automatycznie starego B11.
# Szczegóły i kolejność analiz: scripts/README.md, sekcja „Dane B11”.
#
# URUCHOMIENIE
# python scripts/analyze_b11_case.py --output tmp/B11_case_385.json
#
# PRZYKŁADOWY WYNIK
# JSON na stdout i w tmp/B11_case_385.json; np. pola case_ed: 385,
# replicates: 30, quality_days: 26, series, paired_changes.
# Wartości metryk zależą od dostarczonych wyników; brak pliku graficznego.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, biblioteka standardowa. Katalog rodzica --output musi istnieć.
# Analiza odbywa się PRZED parsowaniem argumentów, więc nawet --help czyta dane.
# Znaczniki kolizji nie są rozłącznymi przyczynami fizycznymi utraty ramek.
# =============================================================================

"""Read-only analysis of existing B11 runs; write auditable case diagnostics."""
import json
import re
import statistics as stats
from pathlib import Path

ROOT = Path(r"C:\Omnet\simulation_results\review_chapter1")
PATTERNS = {
    "tx_ul": r"^Packets transmitted UL = (\d+)",
    "rx_ul": r"^Packets delivered UL = (\d+)",
    "lost_ul_with_collision": r"^Collisions uplink = (\d+)",
    "device_days": r"^Days device-days total \(UL\)\[%\] = (\d+)",
    "successful_device_days": r"^Days device-days successful \(UL\)\[%\] = (\d+)",
}
SCALARS = {
    "forwarded_30d": "TS011 uplinks forwarded",
    "forwarding_limit_drops_30d": "TS011 forwarding-limit drops",
    "payload_limit_drops_30d": "TS011 payload-limit drops",
    "join_filter_drops_30d": "TS011 Join-Request filter drops",
    "wor_tx_30d": "TS011 WOR transmitted",
    "wor_ack_rx_30d": "TS011 WOR ACK received",
    "sync_losses_30d": "TS011 synchronization losses",
}


def main():
    result = {"case_ed": 385, "central_ed": 350, "weak_ed": 35,
        "replicates": 30, "quality_days": 26, "scalar_counter_days": 30,
        "caveat": "collision tags are not exclusive physical causes; relay counters count receptions/forwards, not unique end-to-end losses",
        "series": {}, "paired_changes": {}}
    raw = {}
    coordinates = {}
    for series, key in (("EP4312388", "ep4312388_350"), ("TS011", "ts011_350"),
                         ("TS011 bez limitu", "ts011_unlimited_350")):
        rows = []
        for run in range(30):
            folder = ROOT / "variants" / ("b11_comparison__"+key) / "runs" / f"run_{run:02d}"
            text = (folder / "exec_results.txt").read_text(encoding="utf-8", errors="replace")
            row = {name: int(re.search(pattern, text, re.M).group(1)) for name, pattern in PATTERNS.items()}
            row["lost_ul"] = row["tx_ul"]-row["rx_ul"]
            row["lost_ul_without_collision"] = row["lost_ul"]-row["lost_ul_with_collision"]
            row["failed_device_days"] = row["device_days"]-row["successful_device_days"]
            row["pdr_pct"] = 100*row["rx_ul"]/row["tx_ul"]
            row["daily_pct"] = 100*row["successful_device_days"]/row["device_days"]
            row["collision_tagged_loss_pct"] = 100*row["lost_ul_with_collision"]/row["tx_ul"]
            row["other_loss_pct"] = 100*row["lost_ul_without_collision"]/row["tx_ul"]
            scalar_text = (folder / "results.sca").read_text(encoding="utf-8", errors="replace")
            for name, label in SCALARS.items():
                row[name] = sum(float(v) for v in re.findall(r'^scalar\s+\S+\s+"'+re.escape(label)+r'"\s+(\S+)', scalar_text, re.M))
            topology = (folder / "exec_results_topology.txt").read_text(encoding="utf-8", errors="replace")
            lines = topology.split("idx,x,y,avg_daily_success_UL[%]\n", 1)[1].strip().splitlines()
            devices = [line.split(",") for line in lines if len(line.split(",")) == 4]
            assert len(devices) == 385
            xy = [d[:3] for d in devices]
            if run in coordinates:
                assert coordinates[run] == xy, f"Topology mismatch: {series} run {run}"
            else:
                coordinates[run] = xy
            # Exact day counts can be recovered from two-decimal percentages (1 day = 3.846%).
            successes = [round(float(d[3])*26/100) for d in devices]
            row["central_failed_device_days"] = sum(26-n for n in successes[:350])
            row["weak_failed_device_days"] = sum(26-n for n in successes[350:])
            assert row["central_failed_device_days"]+row["weak_failed_device_days"] == row["failed_device_days"]
            rows.append(row)
        raw[series] = rows
        result["series"][series] = {name: stats.fmean(r[name] for r in rows) for name in rows[0]}
    for metric in ("daily_pct", "pdr_pct"):
        for label, hi, lo in (("limit_effect", "TS011 bez limitu", "TS011"),
                              ("residual_gap", "EP4312388", "TS011 bez limitu")):
            delta = [a[metric]-b[metric] for a, b in zip(raw[hi], raw[lo])]
            mean = stats.fmean(delta)
            half = 2.045*stats.stdev(delta)/(len(delta)**.5)
            result["paired_changes"][label+"_"+metric] = {"mean_pp": mean, "ci95": [mean-half, mean+half]}
    print(json.dumps(result, indent=2, ensure_ascii=False))
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(__file__).parent / "B11_case_385.json")
    args = parser.parse_args()
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
