#!/usr/bin/env python3
# =============================================================================
# STANDARDOWY OPIS SKRYPTU: plot_topology_compare.py
#
# DZIAŁANIE
# Porównuje skumulowane topologie kilku scenariuszy na wspólnej skali 0..100%.
# Każdy panel obejmuje wszystkie przebiegi z jednego pliku.
#
# WEJŚCIE I PARAMETRY
# inputs: co najmniej jedna ścieżka albo para "Etykieta=ścieżka"; do porównania
# podaj >= 2 pliki. Bez etykiety używana jest nazwa katalogu lub pliku.
# -o/--output: plik, domyślnie topology_comparison.png.
# --columns: liczba kolumn >= 1, domyślnie 2; sprawdzana w skrypcie.
# --point-size: pt², zalecane > 0, domyślnie 18.
# --no-labels: ukrywa etykiety stałych węzłów; domyślnie wyłączone.
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
# Drugą topologię wygeneruj tym samym poleceniem dla --scenarios t07.
# W PowerShell przypisz do $topology2 ścieżkę:
#   tmp/docs_demo/scenarios/07_2GW_SF12_N1_12h/exec_results_topology.txt
#
# URUCHOMIENIE
# python scripts/plot_topology_compare.py "1 GW=$topology" "2 GW=$topology2" -o tmp/topology_compare.png
#
# PRZYKŁADOWY WYNIK
# tmp/topology_compare.png: panele obu scenariuszy ze średnią skutecznością.
# Stdout: ścieżka pliku. Podgląd: plot_topology_compare_example.png.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, Matplotlib. Dla porównania użyj zgodnych czasów symulacji
# i definicji metryk; skrypt nie sprawdza zgodności konfiguracji scenariuszy.
# =============================================================================

"""Compare aggregate topologies from several scenarios side by side."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt

from topology_io import parse_topology
from topology_plotting import (
    add_success_colorbar,
    apply_canvas,
    configure_style,
    draw_topology,
    save_figure,
)


def parse_input(value: str) -> tuple[str, Path]:
    if "=" in value:
        label, raw_path = value.split("=", 1)
        return label.strip(), Path(raw_path.strip())
    path = Path(value)
    return path.parent.name or path.stem, path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Porównuje skumulowane topologie kilku scenariuszy."
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help='pliki albo pary "Etykieta=C:\\...\\exec_results_topology.txt"',
    )
    parser.add_argument("-o", "--output", type=Path, default=Path("topology_comparison.png"))
    parser.add_argument("--columns", type=int, default=2)
    parser.add_argument("--cmap", default="viridis")
    parser.add_argument("--point-size", type=float, default=18.0)
    parser.add_argument("--dpi", type=int, default=180)
    parser.add_argument("--canvas", nargs=4, type=float, metavar=("XMIN", "XMAX", "YMIN", "YMAX"))
    parser.add_argument("--no-labels", action="store_true")
    parser.add_argument("--lenient", action="store_true")
    args = parser.parse_args()

    scenarios = [(label, parse_topology(path, strict=not args.lenient)) for label, path in map(parse_input, args.inputs)]
    if args.columns < 1:
        parser.error("--columns musi być dodatnie")

    configure_style()
    rows = math.ceil(len(scenarios) / args.columns)
    figure, axes = plt.subplots(
        rows,
        args.columns,
        figsize=(6.0 * args.columns, 4.6 * rows),
        squeeze=False,
        sharex=True,
        sharey=True,
        constrained_layout=True,
    )

    for ax, (label, topology) in zip(axes.flat, scenarios):
        devices = topology.devices
        draw_topology(
            ax,
            devices,
            topology.fixed_nodes,
            cmap=args.cmap,
            point_size=args.point_size,
            alpha=0.82,
            label_fixed_nodes=not args.no_labels,
        )
        apply_canvas(ax, args.canvas)
        mean = sum(node.daily_success_percent for node in devices) / len(devices)
        ax.set_title(f"{label}\n{len(topology.runs)} przebiegów, średnia {mean:.2f}%")

    for ax in list(axes.flat)[len(scenarios) :]:
        ax.set_visible(False)

    add_success_colorbar(figure, list(axes.flat)[: len(scenarios)], cmap=args.cmap)
    destination = save_figure(figure, args.output, dpi=args.dpi)
    print(destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
