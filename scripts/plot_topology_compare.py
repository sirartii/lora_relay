#!/usr/bin/env python3
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
