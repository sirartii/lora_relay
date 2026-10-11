# =============================================================================
# STANDARDOWY OPIS SKRYPTU: generate_b11_topology_figure.py
#
# DZIAŁANIE
# Rysuje zarejestrowaną topologię B11 dla 385 ED, rozdzielając 350 centralnych
# i wyspy 18+17. Sprawdza identyczność topologii trzech wariantów.
#
# WEJŚCIE I PARAMETRY
# --results: katalog wyników, domyślnie C:/Omnet/simulation_results/review_chapter1.
# --output: wymagany katalog PNG/PDF/JSON, tworzony automatycznie.
# --run: indeks przebiegu, domyślnie 0; dla standardowego B11: 0..29.
# Wejścia: reproducibility/simulations/omnetpp_review.ini oraz topologie
# trzech wariantów *_350. Muszą zawierać dokładnie 1 przebieg, 385 ED,
# 1 GW i 2 RL; indeksy w grupach: 0..349, 0..17, 0..16.
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
# python scripts/generate_b11_topology_figure.py --run 0 --output tmp/b11_topology
#
# PRZYKŁADOWY WYNIK
# B11_topologia_385.png/.pdf/.json; stdout: metadane, współrzędne GW/RL,
# group_counts=[350,18,17], źródła i SHA-256.
# Podgląd: generate_b11_topology_figure_example.png.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, Matplotlib i sąsiednie moduły topology_io/topology_plotting.
# Obrysy prostokątów oznaczają obszary losowania ED, nie granice zasięgu.
# Skrypt dodaje C:/Omnet/lora_relay/scripts do ścieżki importów.
# =============================================================================

"""Draw the recorded B11 topology, without simulating or inventing radio links."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys

os.environ.setdefault("MPLCONFIGDIR", str(Path(os.environ.get("TEMP", ".")) / "lora_review_matplotlib"))
sys.path.insert(0, str(Path(r"C:\Omnet\lora_relay\scripts")))
from topology_io import parse_topology
from topology_plotting import configure_style
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, default=Path(r"C:\Omnet\simulation_results\review_chapter1"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--run", type=int, default=0)
    args = parser.parse_args()
    config = args.results / "reproducibility/simulations/omnetpp_review.ini"
    parameters = dict(re.findall(r"^\*\*\.(\w+)\s*=\s*([-\d.]+)\s*$", config.read_text(encoding="utf-8"), re.M))
    sources = []
    reference = None
    for variant in ("ep4312388_350", "ts011_350", "ts011_unlimited_350"):
        source = args.results / "variants" / ("b11_comparison__" + variant) / "runs" / f"run_{args.run:02d}" / "exec_results_topology.txt"
        data = parse_topology(source)
        assert len(data.runs) == 1
        run = data.runs[0]
        coordinates = [(node.index, node.x, node.y) for node in run.devices]
        assert len(coordinates) == 385
        signature = (coordinates, run.fixed_nodes)
        if reference is None:
            reference, topology = signature, run
        else:
            assert signature == reference, f"Different topology: {variant}"
        sources.append({"file": str(source), "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})

    # Keeper exports central EDs, followed by endnode1 and endnode2.
    groups = (topology.devices[:350], topology.devices[350:368], topology.devices[368:])
    assert [[node.index for node in group] for group in groups] == [list(range(n)) for n in (350, 18, 17)]
    for number, group in enumerate(groups[1:], 1):
        x, y = (float(parameters[f"weak{number}{axis}"]) for axis in ("x", "y"))
        width, height = (float(parameters[f"weak{number}{axis}"]) for axis in ("Width", "Height"))
        assert all(x + 1 <= node.x <= x + width and y + 1 <= node.y <= y + height for node in group)

    configure_style()
    plt.rcParams.update({"font.size": 11, "axes.labelsize": 11, "pdf.fonttype": 42})
    fig, ax = plt.subplots(figsize=(7.4, 7.2))
    fig.subplots_adjust(left=.10, right=.98, bottom=.15, top=.83)
    for group, label, color, marker in zip(groups,
            ("ED centralne (350)", "ED obszaru I (18)", "ED obszaru II (17)"),
            ("#527f9f", "#008579", "#9855a2"), ("o", "D", "x")):
        ax.scatter([n.x for n in group], [n.y for n in group], s=18 if len(group) > 100 else 32,
                   marker=marker, color=color, alpha=.85, linewidths=.9, label=label, zorder=3)
    for number, color in ((1, "#008579"), (2, "#9855a2")):
        x, y = (float(parameters[f"weak{number}{axis}"]) for axis in ("x", "y"))
        width, height = (float(parameters[f"weak{number}{axis}"]) for axis in ("Width", "Height"))
        ax.add_patch(Rectangle((x, y), width, height, fill=False, color=color, linestyle=(0, (4, 3)), linewidth=1))
        ax.text(x + 5, y - 25 if number == 1 else y + height + 9, "Obszar " + ("I" if number == 1 else "II"), color=color, fontsize=10)

    roles = [(n.role, n.index) for n in topology.fixed_nodes]
    assert sorted(roles) == [("GW", 0), ("RELAY", 0), ("RELAY", 1)]
    for node in topology.fixed_nodes:
        gateway = node.role == "GW"
        ax.scatter(node.x, node.y, marker="s" if gateway else "^", s=115 if gateway else 130,
                   color="#cf3b37" if gateway else "#eb8b2d", edgecolors="#262626", linewidths=.8,
                   label="Brama GW (1)" if gateway else ("Retransmitery RL (2)" if node.index == 0 else "_nolegend_"), zorder=5)
        ax.annotate("GW" if gateway else f"RL{node.index + 1}", (node.x, node.y), xytext=(9, -15) if gateway else (10, -15),
                    textcoords="offset points", fontsize=10, fontweight="bold",
                    bbox={"facecolor": "white", "edgecolor": "none", "alpha": .9, "pad": 1.4}, zorder=6)

    ax.set(xlim=(0, 1150), ylim=(0, 900), xlabel="x [m]", ylabel="y [m]")
    ax.set_xticks(range(0, 1101, 200))
    ax.set_yticks(range(0, 901, 100))
    ax.set_aspect("equal")
    ax.legend(loc="lower center", bbox_to_anchor=(.5, 1.025), ncol=2, fontsize=10, frameon=False,
              columnspacing=1.7, handletextpad=.5)
    fig.suptitle("Topologia B11 — 385 urządzeń końcowych", y=.975, fontsize=14)
    fig.text(.54, .035, "Przerywane obrysy: obszary losowania ED, nie granice zasięgu radiowego.", ha="center", fontsize=8.5, color="#4b5563")
    args.output.mkdir(parents=True, exist_ok=True)
    for extension in ("pdf", "png"):
        fig.savefig(args.output / f"B11_topologia_385.{extension}", dpi=220, bbox_inches="tight")
    plt.close(fig)
    cx, cy, radius = (float(parameters[k]) for k in ("centralX", "centralY", "R"))
    outside = sum((node.x-cx)**2+(node.y-cy)**2 > radius**2 for node in groups[0])
    metadata = {"run": args.run, "devices": 385, "group_counts": [350,18,17], "same_topology_all_three_variants": True,
                "fixed_nodes": [{"role": n.role, "index": n.index, "x": n.x, "y": n.y} for n in topology.fixed_nodes],
                "sources": sources, "config": str(config), "central_ED_outside_R_circle": outside,
                "note": "Plot uses exported coordinates; x/y were drawn independently in Chapter7Network.ned. No coverage or disk boundary is assumed."}
    (args.output / "B11_topologia_385.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(metadata, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
