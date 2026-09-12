#!/usr/bin/env python3
"""Recursively render every exec_results_topology.txt below a directory."""

from __future__ import annotations

import argparse
from pathlib import Path
import re

import matplotlib.pyplot as plt

from topology_io import parse_topology
from topology_plotting import (
    add_success_colorbar,
    apply_canvas,
    configure_style,
    draw_topology,
    save_figure,
)


def safe_name(value: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "_", value.strip())
    return cleaned.strip("._") or "topology"


def main() -> int:
    parser = argparse.ArgumentParser(description="Wsadowo rysuje wszystkie pliki topologii.")
    parser.add_argument("root", type=Path, help="katalog przeszukiwany rekurencyjnie")
    parser.add_argument("-o", "--output-dir", type=Path, default=Path("topology_plots"))
    parser.add_argument("--pattern", default="exec_results_topology.txt")
    parser.add_argument("--cmap", default="viridis")
    parser.add_argument("--point-size", type=float, default=20.0)
    parser.add_argument("--dpi", type=int, default=180)
    parser.add_argument("--canvas", nargs=4, type=float, metavar=("XMIN", "XMAX", "YMIN", "YMAX"))
    parser.add_argument("--lenient", action="store_true")
    args = parser.parse_args()

    root = args.root.expanduser().resolve()
    files = sorted(path for path in root.rglob(args.pattern) if path.is_file())
    if not files:
        parser.error(f"nie znaleziono plików {args.pattern!r} pod {root}")

    configure_style()
    output_dir = args.output_dir.expanduser().resolve()
    used_names: dict[str, int] = {}
    for source in files:
        topology = parse_topology(source, strict=not args.lenient)
        relative_parent = source.parent.relative_to(root)
        stem = safe_name("__".join(relative_parent.parts) or source.stem)
        used_names[stem] = used_names.get(stem, 0) + 1
        suffix = "" if used_names[stem] == 1 else f"_{used_names[stem]}"
        destination = output_dir / f"{stem}{suffix}.png"

        figure, ax = plt.subplots(figsize=(11.2, 7.2), constrained_layout=True)
        draw_topology(
            ax,
            topology.devices,
            topology.fixed_nodes,
            cmap=args.cmap,
            point_size=args.point_size,
            alpha=0.84,
        )
        apply_canvas(ax, args.canvas)
        add_success_colorbar(figure, ax, cmap=args.cmap)
        ax.set_title(f"{relative_parent or source.parent.name} — {len(topology.runs)} przebiegów")
        save_figure(figure, destination, dpi=args.dpi)
        print(f"{source} -> {destination}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
