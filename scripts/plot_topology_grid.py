#!/usr/bin/env python3
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
