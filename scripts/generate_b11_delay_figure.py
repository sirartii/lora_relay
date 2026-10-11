# =============================================================================
# STANDARDOWY OPIS SKRYPTU: generate_b11_delay_figure.py
#
# DZIAŁANIE
# Analizuje skalar Average packet time z pełną precyzją: średnie opóźnienia,
# 95% PU, sparowane różnice i dopasowania dla trzech wariantów B11.
#
# WEJŚCIE I PARAMETRY
# --results: katalog wyników, domyślnie C:/Omnet/simulation_results/review_chapter1.
# --figures i --diagnostics: wymagane katalogi wyjściowe, tworzone automatycznie.
# 900 par results.sca i exec_results.txt (3 x 10 obciążeń x 30 replik).
# Wymagane opóźnienie > 0 s, 26 dni statystyki, 55..550 ED co 55;
# różnica względem zaokrąglonego logu <= 0.00500001 s.
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
# python scripts/generate_b11_delay_figure.py --figures tmp/b11_figures --diagnostics tmp/b11_diagnostics
#
# PRZYKŁADOWY WYNIK
# B11_opoznienie.png/.pdf i B11_delay_analysis.json; stdout: JSON z listą
# wykresów, wynikami dla 385 ED, różnicami i wybranymi dopasowaniami.
# Podgląd: generate_b11_delay_figure_example.png.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, Matplotlib, NumPy, SciPy. Metryka obejmuje tylko doręczone UL,
# od zgłoszenia do radia ED; nie obejmuje wcześniejszego WOR ani utraconych ramek.
# Część ilustracyjna JSON zakłada historyczną zwłokę RL=1 s; nie czyta jej z INI.
# SHA-256 źródeł pochodzi z bieżącego C:/Omnet/lora_relay, nie z archiwum wyników.
# =============================================================================

"""Analyse the existing B11 delay scalar at full precision; no simulations."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re

os.environ.setdefault("MPLCONFIGDIR", str(Path(os.environ.get("TEMP", ".")) / "lora_review_matplotlib"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter
import numpy as np
from scipy.optimize import least_squares
from scipy.stats import t

SERIES = ("EP4312388", "TS011", "TS011 bez limitu")
KEYS = ("ep4312388", "ts011", "ts011_unlimited")
LABELS = ("EP4312388", "TS011: limit 8 ramek/h/RL", "TS011: bez limitów przekazywania")
COLORS = ("#1f77b4", "#e8750a", "#299343")
LOADS = np.arange(55, 551, 55)


def describe(values):
    a = np.asarray(values, dtype=float)
    mean = float(a.mean())
    half = float(t.ppf(.975, len(a)-1) * a.std(ddof=1) / np.sqrt(len(a)))
    return {"n": len(a), "mean": mean, "ci95": [mean-half, mean+half],
            "sd": float(a.std(ddof=1)), "min": float(a.min()), "max": float(a.max())}


def fit(x, y, kind):
    x, y = np.asarray(x), np.asarray(y)
    if kind in ("linear", "quadratic"):
        coefficients = np.polyfit(x / 550, y, 1 if kind == "linear" else 2)
        return lambda z: np.polyval(coefficients, np.asarray(z)/550), {"coefficients_descending": coefficients.tolist(), "x_scale": 550}
    if kind == "exponential":
        def evaluate(z, b):
            return b[0] + b[1]*np.exp(-(np.asarray(z)-55)/b[2])
        solutions = [least_squares(lambda b: evaluate(x, b)-y,
                       [max(0, min(y)*.8), max(.01, max(y)-min(y)*.8), tau],
                       bounds=([0, 0, 1], [np.inf, np.inf, 10000]),
                       xtol=1e-12, ftol=1e-12, gtol=1e-12, max_nfev=10000)
                     for tau in (60, 150, 450)]
        result = min((v for v in solutions if v.success), key=lambda v: np.sum(v.fun**2))
        return lambda z: evaluate(z, result.x), {"formula": "c + A*exp(-(N_ED-55)/tau)",
                  "c": float(result.x[0]), "A": float(result.x[1]), "tau": float(result.x[2])}
    raise ValueError(kind)


def select_fit(x, y):
    grid = np.linspace(55, 550, 1000)
    candidates = []
    for family in ("linear", "quadratic", "exponential"):
        predict, parameters = fit(x, y, family)
        errors = []
        for j in range(len(x)):
            mask = np.arange(len(x)) != j
            trained, _ = fit(x[mask], y[mask], family)
            errors.append(float(trained(x[j]))-float(y[j]))
        curve = predict(grid)
        candidates.append({"family": family, "parameters": parameters,
              "loo_rmse_s": float(np.sqrt(np.mean(np.square(errors)))),
              "r_squared": float(1-np.sum((y-predict(x))**2)/np.sum((y-y.mean())**2)),
              "admissible": bool(np.all(curve >= 0) and np.all(np.diff(curve) <= 1e-8))})
    selected = min((v for v in candidates if v["admissible"]), key=lambda v: v["loo_rmse_s"])
    predict, _ = fit(x, y, selected["family"])
    return predict, {"selected": selected, "candidates": candidates}


def read_scalar(path):
    # The scalar is near the beginning of the file: do not load all ED scalars.
    pattern = re.compile(r'^scalar\s+\S+\.keeper\s+"Average packet time"\s+(\S+)\s*$')
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            match = pattern.match(line)
            if match:
                value = float(match.group(1))
                assert np.isfinite(value) and value > 0, path
                return value
    raise ValueError(f"Missing delay scalar: {path}")


def model_airtime(length, sf):
    """Exactly the formula in LoRaPacket::calcMsgDuration, before float rounding."""
    optimisation = 1 if sf in (11, 12) else 0
    encoded = np.ceil((8*(length+13)-4*sf+28+16)/(4*(sf-2*optimisation)))*5
    return float((8+4.25+8+max(encoded, 0))*2**sf/125000)


def generate(results, figures, diagnostics):
    raw = {}
    summary = {}
    source_paths = []
    max_rounding_difference = 0
    for series, key in zip(SERIES, KEYS):
        raw[series], summary[series] = {}, {}
        for load in LOADS:
            values = []
            for run in range(30):
                central = int(load) * 10 // 11
                folder = results / "variants" / f"b11_comparison__{key}_{central}" / "runs" / f"run_{run:02d}"
                scalar = folder / "results.sca"
                value = read_scalar(scalar)
                log = (folder / "exec_results.txt").read_text(encoding="utf-8")
                reported = float(re.search(r'^Average packet delivery time = (\S+)', log, re.M).group(1))
                difference = abs(value-reported)
                assert difference <= .00500001, (scalar, value, reported)
                max_rounding_difference = max(max_rounding_difference, difference)
                assert int(re.search(r'^ED\s*=\s*(\d+)', log, re.M).group(1)) == load
                assert int(re.search(r'^Statistics days\s*=\s*(\d+)', log, re.M).group(1)) == 26
                values.append(value)
                source_paths.append(str(scalar))
            raw[series][int(load)] = values
            summary[series][int(load)] = describe(values)

    differences = {}
    for name, first, second in (("TS0_minus_TS", SERIES[2], SERIES[1]),
                                ("TS_minus_EP", SERIES[1], SERIES[0]),
                                ("TS0_minus_EP", SERIES[2], SERIES[0])):
        differences[name] = {int(load): describe(np.array(raw[first][int(load)])-np.array(raw[second][int(load)])) for load in LOADS}

    report = {"metric": "Average packet time", "unit": "s", "source": "full-precision Keeper scalar; not rounded exec_results.txt",
              "definition": "first sendDataFrame UL handoff to ED radio to first Keeper reception after LNS deduplication; includes subsequent radio queueing, but not pre-handoff WOR access or EP drift wait; delivered UL only",
              "replications_per_load": 30, "quality_days": 26, "load_range": [55, 550],
              "ci_method": "pointwise Student t, df=29; paired differences across matched run numbers; no familywise correction",
              "fit_method": "equal-weight least squares on 10 load means; minimum leave-one-load-level-out RMSE among nonnegative nonincreasing linear/quadratic/exponential candidates",
              "rounded_log_max_absolute_difference_s": max_rounding_difference,
              "summary": summary, "paired_differences": differences, "raw": raw, "fits": {}, "sources": source_paths}
    report["illustrative_path_times_without_queueing_s"] = {
        "direct_SF12_51B_plus_dedup": model_airtime(51, 12)+.1,
        "EP_two_SF9_51B_plus_dedup": 2*model_airtime(51, 9)+.1,
        "TS_two_SF9_51B_then_70B_plus_1s_and_dedup": model_airtime(51, 9)+1+model_airtime(70, 9)+.1,
        "TS_minus_EP_on_same_idealised_path": 1+model_airtime(70, 9)-model_airtime(51, 9)}
    repo = Path(r"C:\Omnet\lora_relay")
    report["model_sources_sha256"] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
         for path in (repo/"src"/name for name in ("Keeper.cc", "LogicEndnode.cc", "LogicRelay.cc", "Radio.cc", "LoRaPacket.cc", "LNS.cc", "LNS.ned"))}
    predictors = {}
    for series in SERIES:
        means = np.array([summary[series][int(load)]["mean"] for load in LOADS])
        predictors[series], report["fits"][series] = select_fit(LOADS, means)

    with plt.rc_context({"font.family": "DejaVu Sans", "font.size": 10,
              "axes.labelsize": 10, "axes.titlesize": 11,
              "axes.spines.top": False, "axes.spines.right": False, "pdf.fonttype": 42}):
        fig, axes = plt.subplots(2, 1, figsize=(7.2, 9.1), gridspec_kw={"height_ratios": [1.6, 1]})
        fig.subplots_adjust(left=.14, right=.97, bottom=.125, top=.865, hspace=.43)
        handles = []
        grid = np.linspace(55, 550, 600)
        for series, color, label, marker in zip(SERIES, COLORS, LABELS, ("o", "s", "^")):
            observations = [summary[series][int(load)] for load in LOADS]
            means = np.array([v["mean"] for v in observations])
            errors = np.array([v["ci95"][1]-v["mean"] for v in observations])
            handles.append(axes[0].errorbar(LOADS, means, yerr=errors, linestyle="none", marker=marker,
                     markersize=4.6, color=color, capsize=2.7, linewidth=1, label=label, zorder=4))
            axes[0].plot(grid, predictors[series](grid), "--", color=color, linewidth=1.35, alpha=.85)
        axes[0].set_title("(a) Czas doręczonych UL: zgłoszenie do radia ED → LNS", loc="left", pad=13)
        axes[0].set_ylabel(r"$\overline{T}_{\mathrm{UL}\mid\mathrm{dor.}}$ [s]")
        axes[0].set_ylim(.65, 3.15)

        changes = [differences["TS0_minus_TS"][int(load)] for load in LOADS]
        means = np.array([v["mean"] for v in changes])
        errors = [v["ci95"][1]-v["mean"] for v in changes]
        axes[1].errorbar(LOADS, means, yerr=errors, marker="o", linestyle="none", color="#4f5965",
                       capsize=3, markersize=4.8, linewidth=1.2, zorder=4)
        axes[1].axhline(0, color="#666666", linewidth=1)
        focus = int(np.where(LOADS == 385)[0][0])
        axes[1].errorbar([385], [means[focus]], yerr=[errors[focus]], marker="o", color="#b36c16", capsize=4, zorder=5)
        axes[1].set_title("(b) Zmiana po wyłączeniu limitów TS011", loc="left", pad=13)
        axes[1].set_ylabel(r"$\Delta\overline{T}=\overline{T}_{\mathrm{TS0}}-\overline{T}_{\mathrm{TS}}$ [s]")
        axes[1].text(.01, .96, "+ dłuższy czas w TS0", transform=axes[1].transAxes, va="top", fontsize=8.5, color="#555555")
        axes[1].text(.01, .04, "− krótszy czas w TS0", transform=axes[1].transAxes, va="bottom", fontsize=8.5, color="#555555")
        axes[1].set_ylim(-.77, .87)
        for ax in axes:
            ax.axvline(385, linestyle=":", color="#888888", linewidth=.85, zorder=0)
            ax.set_xlim(30, 570)
            ax.set_xticks([55, 165, 275, 385, 495, 550])
            ax.set_xlabel(r"liczba urządzeń $N_{\mathrm{ED}}$")
            ax.grid(alpha=.18)
            ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}".replace(".", ",")))
        handles.append(Line2D([0], [0], color="#555555", linestyle="--"))
        fig.legend(handles, [*LABELS, "gładkie dopasowanie opisowe"], loc="upper left",
                   bbox_to_anchor=(.12, .992), frameon=False, fontsize=9, labelspacing=.45)
        fig.text(.14, .04, "Punkty: 30 replik; słupki: punktowe 95% PU średniej (a) i sparowanej różnicy (b).", fontsize=7.8)
        fig.text(.14, .017, "Bez procedury dostępu przed zgłoszeniem UL do radia (m.in. WOR/WOR ACK) i wiadomości utraconych.", fontsize=7.3)
        figures.mkdir(parents=True, exist_ok=True)
        outputs = []
        for extension in ("pdf", "png"):
            destination = figures / f"B11_opoznienie.{extension}"
            fig.savefig(destination, dpi=240, bbox_inches="tight", pad_inches=.05)
            outputs.append(str(destination))
        plt.close(fig)
    diagnostics.mkdir(parents=True, exist_ok=True)
    (diagnostics / "B11_delay_analysis.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    concise = {"outputs": outputs, "case_385": {s: summary[s][385] for s in SERIES},
               "differences_385": {k: v[385] for k,v in differences.items()},
               "selected_fits": {k: v["selected"] for k,v in report["fits"].items()},
               "differences_by_load": differences["TS0_minus_TS"],
               "means_by_load": {s: {n: v["mean"] for n,v in summary[s].items()} for s in SERIES}}
    print(json.dumps(concise, ensure_ascii=False, indent=2))
    return outputs


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, default=Path(r"C:\Omnet\simulation_results\review_chapter1"))
    parser.add_argument("--figures", type=Path, required=True)
    parser.add_argument("--diagnostics", type=Path, required=True)
    args = parser.parse_args()
    generate(args.results, args.figures, args.diagnostics)
