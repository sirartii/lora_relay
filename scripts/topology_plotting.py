"""Shared Matplotlib helpers for LoRa topology figures."""

from __future__ import annotations

from collections.abc import Iterable, Sequence
from pathlib import Path
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.axes import Axes
from matplotlib.cm import ScalarMappable
from matplotlib.colors import Normalize

from topology_io import EndDevice, FixedNode


SUCCESS_NORMALISATION = Normalize(vmin=0.0, vmax=100.0)

ROLE_STYLE = {
    "GW": {"marker": "^", "color": "#d62728", "label": "Gateway", "size": 125},
    "RELAY": {"marker": "s", "color": "#ff7f0e", "label": "Relay", "size": 105},
    "IPB": {"marker": "D", "color": "#1f77b4", "label": "LoRa→IP", "size": 105},
}


def add_success_statistics(ax: Axes, devices: Sequence[EndDevice]) -> None:
    """Add the mean/median/quartile labels used in the thesis figures."""

    values = [node.daily_success_percent for node in devices]
    if not values:
        return
    if len(values) == 1:
        q1 = q3 = values[0]
    else:
        q1, _, q3 = statistics.quantiles(values, n=4, method="inclusive")
    labels = (
        f"Śr: {statistics.fmean(values):.2f}%",
        f"Me: {statistics.median(values):.2f}%",
        f"Q1: {q1:.2f}%",
        f"Q3: {q3:.2f}%",
    )
    for index, label in enumerate(labels):
        ax.text(
            0.02,
            0.98 - index * 0.072,
            label,
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=9,
            color="#666666",
            bbox={
                "boxstyle": "round,pad=0.22",
                "facecolor": "#f7f7f7",
                "edgecolor": "#c8c8c8",
                "alpha": 0.96,
            },
            zorder=7,
        )


def configure_style() -> None:
    plt.rcParams.update(
        {
            "figure.dpi": 120,
            "savefig.dpi": 180,
            "font.size": 10,
            "axes.titlesize": 12,
            "axes.labelsize": 10,
            "legend.fontsize": 9,
            "axes.grid": True,
            "grid.alpha": 0.22,
            "grid.linewidth": 0.6,
        }
    )


def draw_topology(
    ax: Axes,
    devices: Sequence[EndDevice],
    fixed_nodes: Sequence[FixedNode],
    *,
    cmap: str = "viridis",
    point_size: float = 24.0,
    alpha: float = 0.88,
    label_fixed_nodes: bool = True,
):
    """Draw end devices and fixed infrastructure on an existing axes."""

    if not devices:
        raise ValueError("cannot draw an empty device collection")

    scatter = ax.scatter(
        [node.x for node in devices],
        [node.y for node in devices],
        c=[node.daily_success_percent for node in devices],
        norm=SUCCESS_NORMALISATION,
        cmap=cmap,
        marker="x",
        s=point_size,
        linewidths=0.8,
        alpha=alpha,
        zorder=2,
    )

    used_labels: set[str] = set()
    role_counters: dict[str, int] = {}
    for node in fixed_nodes:
        style = ROLE_STYLE.get(
            node.role,
            {"marker": "P", "color": "#444444", "label": node.role, "size": 100},
        )
        legend_label = str(style["label"])
        ax.scatter(
            [node.x],
            [node.y],
            marker=str(style["marker"]),
            s=float(style["size"]),
            c=[str(style["color"])],
            edgecolors="black",
            linewidths=0.8,
            label=legend_label if legend_label not in used_labels else "_nolegend_",
            zorder=4,
        )
        used_labels.add(legend_label)
        role_counters[node.role] = role_counters.get(node.role, 0) + 1
        if label_fixed_nodes:
            short_role = "RL" if node.role == "RELAY" else node.role
            ax.annotate(
                f"{short_role}{role_counters[node.role]}",
                (node.x, node.y),
                xytext=(6, 6),
                textcoords="offset points",
                fontsize=8,
                color="#222222",
                zorder=5,
            )

    ax.set_xlabel("x [m]")
    ax.set_ylabel("y [m]")
    ax.set_aspect("equal", adjustable="box")
    add_success_statistics(ax, devices)
    if used_labels:
        ax.legend(title="Stałe węzły", loc="best", framealpha=0.92)
    return scatter


def apply_canvas(ax: Axes, canvas: Sequence[float] | None) -> None:
    if canvas is None:
        ax.margins(x=0.05, y=0.05)
        return
    xmin, xmax, ymin, ymax = canvas
    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin, ymax)


def add_success_colorbar(figure, axes, *, cmap: str = "viridis"):
    mappable = ScalarMappable(norm=SUCCESS_NORMALISATION, cmap=cmap)
    mappable.set_array([])
    colorbar = figure.colorbar(mappable, ax=axes, pad=0.025, shrink=0.92)
    colorbar.set_label("Skuteczność dzienna [%]")
    return colorbar


def save_figure(figure, output: str | Path, *, dpi: int = 180, show: bool = False) -> Path:
    destination = Path(output).expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(destination, dpi=dpi, bbox_inches="tight")
    if show:
        # The Agg backend keeps batch use reliable.  Opening the saved file is
        # intentionally left to the caller/operating system.
        print(f"Zapisano wykres: {destination}")
    plt.close(figure)
    return destination


def flatten(groups: Iterable[Sequence[EndDevice]]) -> tuple[EndDevice, ...]:
    return tuple(node for group in groups for node in group)
