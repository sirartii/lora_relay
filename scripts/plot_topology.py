#!/usr/bin/env python3
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
