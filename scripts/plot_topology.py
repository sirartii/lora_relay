#!/usr/bin/env python3
# =============================================================================
# STANDARDOWY OPIS SKRYPTU: plot_topology.py
#
# DZIAŁANIE
# Rysuje jeden przebieg lub skumulowaną topologię wielu przebiegów.
# Kolor ED przedstawia dzienną skuteczność UL; GW/RL/IPB mają osobne symbole.
#
# WEJŚCIE I PARAMETRY
# input: wymagany plik tekstowy eksportu Keeper, co najmniej jeden pełny przebieg.
# -o/--output: plik graficzny, domyślnie topology.png (format wg rozszerzenia).
# --runs: all (domyślnie), indeks, lista 0,2,5 lub zakres 0-5; indeksy od 0
# do liczby przebiegów minus 1, końce zakresu włącznie.
# --title: dowolny tekst; domyślnie tytuł tworzony automatycznie.
# --point-size: rozmiar znacznika w pt², zalecane > 0, domyślnie 24.
# --alpha: przezroczystość 0..1, domyślnie 0.88.
# --no-labels: ukrywa etykiety stałych węzłów; domyślnie są widoczne.
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
# python scripts/plot_topology.py $topology --runs 0 --canvas 0 1400 0 900 -o tmp/topology.png
#
# PRZYKŁADOWY WYNIK
# Plik tmp/topology.png oraz jego bezwzględna ścieżka na stdout.
# Podgląd: plot_topology_example.png obok skryptu.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, Matplotlib. Polecenia wykonuj z katalogu repozytorium.
# Zakresy opisane jako zalecane nie są dodatkowo sprawdzane przez argparse.
# =============================================================================

"""Draw one run or an aggregate of runs from exec_results_topology.txt."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt

from topology_io import parse_topology, select_runs, unique_fixed_nodes
from topology_plotting import (
    add_success_colorbar,
    apply_canvas,
    configure_style,
    draw_topology,
    flatten,
    save_figure,
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Rysuje topologię LoRa z pliku exec_results_topology.txt."
    )
    parser.add_argument("input", type=Path, help="plik exec_results_topology.txt")
    parser.add_argument("-o", "--output", type=Path, default=Path("topology.png"))
    parser.add_argument(
        "--runs",
        default="all",
        help="all, pojedynczy numer, lista 0,2,5 albo zakres 0-9 (domyślnie: all)",
    )
    parser.add_argument("--title", help="własny tytuł wykresu")
    parser.add_argument("--cmap", default="viridis", help="paleta Matplotlib")
    parser.add_argument("--point-size", type=float, default=24.0)
    parser.add_argument("--alpha", type=float, default=0.88)
    parser.add_argument("--dpi", type=int, default=180)
    parser.add_argument(
        "--canvas",
        nargs=4,
        type=float,
        metavar=("XMIN", "XMAX", "YMIN", "YMAX"),
        help="stały zakres osi, np. --canvas 0 1400 0 900",
    )
    parser.add_argument("--no-labels", action="store_true", help="ukrywa etykiety GW/RL/IPB")
    parser.add_argument("--lenient", action="store_true", help="pomija niepełne końcowe bloki")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    topology = parse_topology(args.input, strict=not args.lenient)
    runs = select_runs(topology, args.runs)
    devices = flatten(run.devices for run in runs)
    fixed = unique_fixed_nodes(run.fixed_nodes for run in runs)

    configure_style()
    figure, ax = plt.subplots(figsize=(11.2, 7.2), constrained_layout=True)
    draw_topology(
        ax,
        devices,
        fixed,
        cmap=args.cmap,
        point_size=args.point_size,
        alpha=args.alpha,
        label_fixed_nodes=not args.no_labels,
    )
    apply_canvas(ax, args.canvas)
    add_success_colorbar(figure, ax, cmap=args.cmap)

    if args.title:
        title = args.title
    elif len(runs) == 1:
        title = f"Topologia — przebieg {runs[0].run_number + 1}/{len(topology.runs)}"
    else:
        title = f"Topologia skumulowana — {len(runs)} przebiegów, {len(devices)} ED"
    ax.set_title(title)

    destination = save_figure(figure, args.output, dpi=args.dpi)
    print(destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
