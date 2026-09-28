"""Reproducible B11 probability figure and descriptive-fit diagnostics.

Uses aggregate simulation means, never extrapolates, and selects from four
predeclared descriptive families by leave-one-load-level-out RMSE, subject to
non-increasing probabilities in [0, 100] over the observed range.
"""
from __future__ import annotations

import csv
import json
import math
import re
import statistics
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter
from matplotlib.transforms import blended_transform_factory
import numpy as np
from scipy.optimize import least_squares, minimize, LinearConstraint

SERIES = ("EP4312388", "TS011", "TS011 bez limitu")
FIELDS = ("daily_success_percent", "pdr_ul_percent")
COLORS = ("#1f77b4", "#e8750a", "#299343")
LABELS = ("EP4312388", "TS011: limit 8 ramek/h/RL", "TS011: bez limitów przekazywania")
KINDS = ("linear", "quadratic", "exponential", "monotone_cubic")


def fit(x, y, kind):
    x, y = np.asarray(x, dtype=float), np.asarray(y, dtype=float)
    if kind in ("linear", "quadratic"):
        degree = 1 if kind == "linear" else 2
        coefficients = np.polyfit(x / 550, y, degree)
        return (lambda z: np.polyval(coefficients, np.asarray(z) / 550)), {
            "coefficients_descending": coefficients.tolist(), "x_scale": 550}
    if kind == "exponential":
        def evaluate(z, b):
            c, q, tau = b
            return c + (100-c) * q * np.exp(-(np.asarray(z)-55)/tau)
        result = least_squares(lambda b: evaluate(x, b)-y,
            [min(y)-0.1, (max(y)-min(y)+0.1)/(100-min(y)+0.1), 150],
            bounds=([0, 0, 1], [100, 1, 10000]), max_nfev=20000,
            xtol=1e-12, ftol=1e-12, gtol=1e-12)
        if not result.success:
            raise RuntimeError(result.message)
        c, q, tau = result.x
        return (lambda z: evaluate(z, result.x)), {
            "formula": "c + A*exp(-(N_ED-55)/tau)",
            "c": float(c), "A": float((100-c)*q), "tau": float(tau)}
    if kind == "monotone_cubic":
        def design(z):
            u = (np.asarray(z)-55)/495
            return np.stack([(1-u)**3, 3*u*(1-u)**2, 3*u*u*(1-u), u**3], axis=-1)
        matrix = design(x)
        initial = np.sort(np.clip(np.linalg.lstsq(matrix, y, rcond=None)[0], 0, 100))[::-1]
        differences = np.array([[1,-1,0,0],[0,1,-1,0],[0,0,1,-1]])
        result = minimize(lambda b: np.sum((matrix@b-y)**2), initial,
            jac=lambda b: 2*matrix.T@(matrix@b-y), method="SLSQP",
            bounds=[(0,100)]*4, constraints=[LinearConstraint(differences, 0, np.inf)],
            options={"ftol": 1e-12, "maxiter": 3000})
        if not result.success:
            raise RuntimeError(result.message)
        return (lambda z: design(z)@result.x), {
            "formula": "sum(c_j * binom(3,j) * u^j * (1-u)^(3-j)), u=(N_ED-55)/495",
            "bernstein_coefficients": result.x.tolist(), "constraints": "100 >= c0 >= c1 >= c2 >= c3 >= 0"}
    raise ValueError(kind)


def select_fit(x, y):
    grid = np.linspace(55, 550, 1000)
    candidates = []
    for kind in KINDS:
        predict, parameters = fit(x, y, kind)
        errors = []
        for j in range(len(x)):
            train = np.arange(len(x)) != j
            trained, _ = fit(x[train], y[train], kind)
            errors.append(float(trained(x[j]))-float(y[j]))
        predictions = predict(grid)
        admissible = bool(np.all(np.diff(predictions) <= 1e-8)
                          and predictions.min() >= 0 and predictions.max() <= 100)
        candidates.append({"family": kind, "parameters": parameters,
            "r_squared": float(1-np.sum((y-predict(x))**2)/np.sum((y-y.mean())**2)),
            "loo_rmse_pp": float(np.sqrt(np.mean(np.square(errors)))),
            "admissible": admissible})
    selected = min((v for v in candidates if v["admissible"]), key=lambda v: v["loo_rmse_pp"])
    predict, _ = fit(x, y, selected["family"])
    return predict, {"selected": selected, "candidates": candidates}


def refine_probabilities(summary, results_root):
    """Avoid averaging the percentages already rounded to 0.01 in run logs."""
    refined = []
    for original in summary:
        if original["group"] != "b11_comparison":
            continue
        row = dict(original)
        observations = {field: [] for field in FIELDS}
        for run in range(int(row["runs"])):
            folder = results_root / "variants" / ("b11_comparison__"+row["variant"]) / "runs" / f"run_{run:02d}"
            content = (folder / "exec_results.txt").read_text(encoding="utf-8", errors="replace")
            def count(label):
                return int(re.search(r"^"+re.escape(label)+r"\s*=\s*(\d+)", content, re.M).group(1))
            observations[FIELDS[0]].append(100*count("Days device-days successful (UL)[%]")/count("Days device-days total (UL)[%]"))
            observations[FIELDS[1]].append(100*count("Packets delivered UL")/count("Packets transmitted UL"))
        for field, values in observations.items():
            mean = statistics.fmean(values)
            half = 2.045*statistics.stdev(values)/math.sqrt(len(values))
            row[field+"_mean"] = mean
            row[field+"_ci_low"] = mean-half
            row[field+"_ci_high"] = mean+half
        refined.append(row)
    return refined


def draw_probability_figure(summary, figures: Path, diagnostics_dir: Path):
    rows = [r for r in summary if r["group"] == "b11_comparison"]
    grouped = {s: sorted([r for r in rows if r["series"] == s], key=lambda r: float(r["x"])) for s in SERIES}
    for s, part in grouped.items():
        if len(part) != 10 or any(int(r["runs"]) != 30 for r in part):
            raise ValueError(f"Incomplete B11 series: {s}")
    report = {"criterion": "minimum leave-one-load-level-out RMSE among in-range monotone candidates",
        "range_ed": [55, 550], "replications_per_level": 30,
        "weights": "equal weight for each of the 10 load-level means",
        "fits": []}
    with plt.rc_context({"font.family": "DejaVu Sans", "font.size": 10,
            "axes.titlesize": 11, "axes.labelsize": 10, "legend.fontsize": 9,
            "axes.spines.top": False, "axes.spines.right": False}):
        fig, axes = plt.subplots(2, 1, figsize=(7.2, 9.1))
        fig.subplots_adjust(left=.12, right=.73, bottom=.07, top=.875, hspace=.31)
        handles = []
        grid = np.linspace(55, 550, 600)
        for panel, (ax, field) in enumerate(zip(axes, FIELDS)):
            values = {}
            for s, color, label, marker in zip(SERIES, COLORS, LABELS, ("o", "s", "^")):
                part = grouped[s]
                x = np.array([float(r["x"]) for r in part])
                y = np.array([float(r[field+"_mean"]) for r in part])
                err = np.array([[float(r[field+"_mean"])-float(r[field+"_ci_low"]),
                                 float(r[field+"_ci_high"])-float(r[field+"_mean"])] for r in part]).T
                plotted = ax.errorbar(x, y, yerr=err, marker=marker, markersize=4.7,
                    linestyle="none", capsize=2.7, elinewidth=1, color=color, label=label, zorder=4)
                if panel == 0:
                    handles.append(plotted)
                predict, diagnostic = select_fit(x, y)
                ax.plot(grid, predict(grid), "--", color=color, linewidth=1.35, alpha=.85, zorder=2)
                report["fits"].append({"series": s, "metric": field, **diagnostic})
                values[s] = y
            limited, unlimited, ep = (values[s] for s in ("TS011", "TS011 bez limitu", "EP4312388"))
            # The shaded region uses simulation means, not fitted values.
            ax.fill_between(x, limited, unlimited, where=unlimited >= limited,
                color="#f0bd69", alpha=.18, interpolate=True, zorder=1)
            trans = blended_transform_factory(ax.transAxes, ax.transData)
            focus = int(np.where(x == 385)[0][0])
            ax.axvline(385, color="#888888", lw=.8, linestyle=":", zorder=0)
            ax.text(1.04, 1.02, "Różnice dla 385 ED", transform=ax.transAxes, fontsize=8.7,
                ha="left", va="bottom", color="#444444")
            for lo, hi, label in ((limited[focus], unlimited[focus], "Wpływ zniesienia\nlimitów"),
                                  (unlimited[focus], ep[focus], "Pozostała\nróżnica")):
                ax.annotate("", xy=(1.035, hi), xytext=(1.035, lo),
                    xycoords=trans, textcoords=trans, annotation_clip=False,
                    arrowprops={"arrowstyle": "<->", "color": "#555555", "lw": 1.1})
                value = f"{hi-lo:.3f}".replace(".", ",")
                ax.text(1.075, (lo+hi)/2, label+"\n"+value+" p.p.", transform=trans,
                    ha="left", va="center", fontsize=8.4, color="#333333")
            ax.set_xlim(30, 570)
            ax.set_xticks([55, 165, 275, 385, 495, 550])
            ax.set_xlabel(r"liczba urządzeń $N_{\mathrm{ED}}$")
            ax.set_ylabel(r"$P_{\geq 1/\mathrm{d}}$ [%]" if panel == 0 else r"$P_{\mathrm{UL}}$ [%]")
            ax.set_title("(a) Co najmniej jeden UL na dobę" if panel == 0 else "(b) Doręczenie pojedynczego UL", loc="left", pad=12)
            ax.set_ylim((95.25, 100.05) if panel == 0 else (72.5, 96.5))
            ax.grid(alpha=.2)
            ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}".replace(".", ",")))
        handles.append(Line2D([0], [0], color="#555555", linestyle="--", label="dopasowanie opisowe"))
        fig.legend(handles=handles, labels=[*LABELS, "dopasowanie opisowe"], loc="upper left",
            bbox_to_anchor=(.10, .992), frameon=False, ncol=1, labelspacing=.45)
        fig.text(.12, .012, "Punkty: średnie z 30 replik; słupki: 95% PU średniej. Zacieniowanie: zmiana po zniesieniu limitów.", fontsize=7.5)
        figures.mkdir(parents=True, exist_ok=True)
        outputs = []
        for extension in ("png", "pdf"):
            path = figures / f"B11_prawdopodobienstwa.{extension}"
            fig.savefig(path, dpi=240, bbox_inches="tight", pad_inches=.05)
            outputs.append(path)
        plt.close(fig)
    diagnostics_dir.mkdir(parents=True, exist_ok=True)
    (diagnostics_dir / "B11_probability_fits.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    (diagnostics_dir / "B11_probabilities.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    return outputs


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--figures", type=Path, required=True)
    parser.add_argument("--diagnostics", type=Path, required=True)
    parser.add_argument("--results-root", type=Path, required=True)
    args = parser.parse_args()
    with args.summary.open(encoding="utf-8-sig", newline="") as stream:
        summary = list(csv.DictReader(stream))
    summary = refine_probabilities(summary, args.results_root)
    print(draw_probability_figure(summary, args.figures, args.diagnostics))
