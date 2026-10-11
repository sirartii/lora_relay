#!/usr/bin/env python3
# =============================================================================
# STANDARDOWY OPIS SKRYPTU: plot_topology_grid.py
#
# DZIAŁANIE
# Rysuje osobny panel dla każdego wybranego przebiegu, ze wspólnymi osiami
# i skalą kolorów skuteczności 0..100%.
#
# WEJŚCIE I PARAMETRY
# input: wymagany plik eksportu Keeper z wybranymi pełnymi przebiegami.
# -o/--output: plik, domyślnie topology_runs.png.
# --runs: all, indeksy lub zakresy od 0; domyślnie 0-5 (wymaga >= 6 przebiegów).
# --columns: liczba kolumn >= 1, domyślnie 3; warunek sprawdzany w skrypcie.
# --point-size: pt², zalecane > 0, domyślnie 23.
# --no-labels: ukrywa etykiety GW/RL/IPB; domyślnie wyłączone.
#
# --cmap: nazwa palety Matplotlib, domyślnie viridis.
# --dpi: dodatnia liczba całkowita, domyślnie 180.
# --canvas: cztery liczby XMIN XMAX YMIN YMAX w metrach; zalecane
# XMIN < XMAX i YMIN < YMAX; bez opcji zakres wynika z punktów.
# --lenient: domyślnie wyłączone; pomija niekompletne bloki, a także
# wyłącza kontrolę zakresu procentów. Nie naprawia błędnych pomiarów.
#
# PRZYGOTOWANIE DANYCH Z SYMULATORA
# Najpierw zbuduj symulator w trybie release (instrukcja: scripts/README.md).
# Z katalogu repozytorium wygeneruj sześć pełnych przebiegów:
#   python scripts/generate_chapter7_results.py --sections table --scenarios t02 --runs 6 --days 30 --output tmp/docs_demo
# Plik wejściowy powstanie pod ścieżką:
#   tmp/docs_demo/scenarios/02_1GW_SF12_N1_12h/exec_results_topology.txt
# W przykładach PowerShell przypisz tę ścieżkę do zmiennej $topology.
# Eksport Keeper zawiera sekcje role,idx,x,y oraz
# idx,x,y,avg_daily_success_UL[%]; wartości skuteczności są procentami 0..100.
#
# URUCHOMIENIE
# python scripts/plot_topology_grid.py $topology --runs 0-5 --columns 3 -o tmp/topology_grid.png
#
# PRZYKŁADOWY WYNIK
# tmp/topology_grid.png: sześć paneli z numerami przebiegów i średnią skutecznością.
# Stdout: ścieżka zapisanego pliku. Podgląd: plot_topology_grid_example.png.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, Matplotlib. Nie wybieraj indeksów spoza pliku wejściowego.
# =============================================================================

"""Draw selected simulation runs as a shared-scale subplot grid."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt

from topology_io import parse_topology, select_runs
from topology_plotting import (
    add_success_colorbar,
    apply_canvas,
    configure_style,
    draw_topology,
    save_figure,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Tworzy siatkę wybranych przebiegów topologii.")
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=Path("topology_runs.png"))
    parser.add_argument("--runs", default="0-5", help="np. 0-5 albo 0,4,9,14")
    parser.add_argument("--columns", type=int, default=3)
    parser.add_argument("--cmap", default="viridis")
    parser.add_argument("--point-size", type=float, default=23.0)
    parser.add_argument("--dpi", type=int, default=180)
    parser.add_argument("--canvas", nargs=4, type=float, metavar=("XMIN", "XMAX", "YMIN", "YMAX"))
    parser.add_argument("--no-labels", action="store_true")
    parser.add_argument("--lenient", action="store_true")
    args = parser.parse_args()

    topology = parse_topology(args.input, strict=not args.lenient)
    runs = select_runs(topology, args.runs)
    if args.columns < 1:
        parser.error("--columns musi być dodatnie")

    configure_style()
    rows = math.ceil(len(runs) / args.columns)
    figure, axes = plt.subplots(
        rows,
        args.columns,
        figsize=(5.0 * args.columns, 4.0 * rows),
        squeeze=False,
        sharex=True,
        sharey=True,
        constrained_layout=True,
    )

    for ax, run in zip(axes.flat, runs):
        draw_topology(
            ax,
            run.devices,
            run.fixed_nodes,
            cmap=args.cmap,
            point_size=args.point_size,
            label_fixed_nodes=not args.no_labels,
        )
        apply_canvas(ax, args.canvas)
        ax.set_title(
            f"Przebieg {run.run_number + 1} — średnia {run.mean_daily_success_percent:.2f}%"
        )

    for ax in list(axes.flat)[len(runs) :]:
        ax.set_visible(False)

    add_success_colorbar(figure, list(axes.flat)[: len(runs)], cmap=args.cmap)
    destination = save_figure(figure, args.output, dpi=args.dpi)
    print(destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
