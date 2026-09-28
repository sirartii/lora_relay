#!/usr/bin/env python3
"""Run the experiments used in chapter 1 of the reviewer responses.

The script deliberately keeps raw OMNeT++ scalars, the exact command line for
every repetition, a run-level CSV and an aggregate CSV.  Plots are copied to
the thesis image directory only after all requested variants have completed.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

os.environ.setdefault(
    "MPLCONFIGDIR", str(Path(os.environ.get("TEMP", ".")) / "lora_review_matplotlib")
)

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from topology_plotting import configure_style
from generate_b11_probability_figure import draw_probability_figure, refine_probabilities


DEFAULT_REPO = Path(r"C:\Omnet\lora_relay")
DEFAULT_OUTPUT = Path(r"C:\Omnet\simulation_results\review_chapter1")
DEFAULT_FIGURES = Path(r"C:\Omnet\DoktoratAF\img\recenzje\chapter1")
DEFAULT_EXE = DEFAULT_REPO / "out" / "clang-release" / "src" / "opp-lora-relay-official.exe"
OMNET_BIN = Path(r"C:\omnetpp-6.4.0\bin")
OMNET_CLANG_BIN = Path(r"C:\omnetpp-6.4.0\tools\win32.x86_64\clang64\bin")

RESULT_PATTERNS = {
    "end_devices": re.compile(r"^ED\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "simulated_days": re.compile(r"^Simulated days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "warmup_days": re.compile(r"^Warmup days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "statistics_days": re.compile(r"^Statistics days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "daily_success_percent": re.compile(r"Daily read success \(UL\)\[%\]\s*=\s*([-+0-9.eE]+)\[%\]"),
    "pdr_ul_percent": re.compile(r"Packets delivered UL ef\s*=\s*([-+0-9.eE]+)\[%\]"),
    "utilisation_percent": re.compile(r"Total utilisation\s*=\s*([-+0-9.eE]+)\[%\]"),
    "collisions": re.compile(r"Total collisions\s*=\s*([0-9]+)"),
    "rejoins": re.compile(r"Devices re-join count\s*=\s*([0-9]+)"),
    "packets_transmitted_ul": re.compile(r"Packets transmitted UL\s*=\s*([0-9]+)"),
    "packets_delivered_ul": re.compile(r"Packets delivered UL\s*=\s*([0-9]+)"),
    "average_delay_s": re.compile(r"Average packet delivery time\s*=\s*([-+0-9.eE]+)"),
    "offline_longest_s": re.compile(r"Devices offline longest\s*=\s*([-+0-9.eE]+)\[s\]"),
}
INTEGER_FIELDS = {
    "end_devices", "simulated_days", "warmup_days", "statistics_days", "collisions",
    "rejoins", "packets_transmitted_ul", "packets_delivered_ul",
}
SCALAR_RE = re.compile(r'^scalar\s+(?P<path>\S+)\s+"(?P<name>[^"]+)"\s+(?P<value>[-+0-9.eE]+)$')
ENERGY_NAMES = {
    "Energy consumption total Tx", "Energy consumption total Rx",
    "Energy consumption total Idle", "Energy consumption total WOR",
    "Energy consumption total BaseOffset", "Energy consumption total LTE attach",
    "Energy consumption total LTE transmit",
}


@dataclass(frozen=True)
class Variant:
    group: str
    key: str
    label: str
    config: str = "Review_EP_Base"
    days: int = 30
    runs: int = 30
    x: float = 0.0
    series: str = ""
    overrides: dict[str, object] = field(default_factory=dict)

    @property
    def destination_name(self) -> str:
        return f"{self.group}__{self.key}"


def population(central: int, weak_total: int | None = None) -> dict[str, object]:
    weak = max(0, round(central * 0.10)) if weak_total is None else weak_total
    first = (weak + 1) // 2
    return {
        "**.numEndnodes": central,
        "**.extranodex1": first,
        "**.extranodex2": weak - first,
    }


def experiment_plan(runs: int) -> list[Variant]:
    v: list[Variant] = []
    for central in (50, 100, 150, 200, 250, 300, 350, 400, 450, 500):
        total = central + round(central * 0.10)
        for series, config in (("TS011", "Review_TS011_Base"), ("EP4312388", "Review_EP_Base")):
            v.append(Variant("b11_comparison", f"{series.lower()}_{central}", series,
                             config, 30, runs, total, series, population(central)))

    # A full reference series isolates the TS011 WOR/CAD procedure from the
    # configured hourly forwarding token buckets.
    for central in (50, 100, 150, 200, 250, 300, 350, 400, 450, 500):
        total = central + round(central * 0.10)
        v.append(Variant("b11_comparison", f"ts011_unlimited_{central}",
                         "TS011 bez limitu", "Review_TS011_Base", 30, runs, total,
                         "TS011 bez limitu", {
                             **population(central), "**.ts011JoinReloadRate": -1,
                             "**.ts011TrustedReloadRate": -1,
                             "**.ts011GlobalReloadRate": -1,
                         }))

    infrastructure = (
        ("1gw", "1 GW", 1, 0, "Review_EP_Base"),
        ("2gw", "2 GW", 2, 0, "Review_2GW_Missing"),
        ("3gw", "3 GW", 3, 0, "Review_EP_Base"),
        ("2rl", "1 GW + 2 RL (EP)", 1, 2, "Review_EP_Base"),
    )
    for key, label, gateways, relays, config in infrastructure:
        v.append(Variant("n15_infrastructure", key, label, config, 30, runs, gateways, label,
                         {**population(50), "**.numGateways": gateways, "**.numRelays": relays}))

    for central in (50, 100, 200, 300):
        total = central + round(central * 0.10)
        common = {
            **population(central), "warmup-period": "1d", "**.windowsPerDay": 1,
            "**.transmittingWindowStart1": 720, "**.transmittingWindowDuration1": 1,
            "**.framesPerWindowMin": 1, "**.framesPerWindowMax": 1,
        }
        for series, config in (("TS011", "Review_TS011_Base"), ("EP4312388", "Review_EP_Burst")):
            v.append(Variant("b10_alarm", f"{series.lower()}_{central}", series, config,
                             7, max(15, runs // 2), total, series, common))

    for window in (15, 180, 720):
        for frames in (1, 2, 4, 6):
            v.append(Variant("b08_window_n", f"w{window}_n{frames}", f"{window} min / N={frames}",
                             "Review_EP_Base", 30, max(20, runs // 2), frames, str(window), {
                                 "**.transmittingWindowDuration1": window,
                                 "**.transmittingWindowDuration2": window,
                                 "**.framesPerWindowMin": frames,
                                 "**.framesPerWindowMax": frames,
                             }))

    # TS011-1.0.1 defines exactly these six CAD periodicities.
    cad_periods_s = (0.02, 0.05, 0.10, 0.25, 0.50, 1.00)
    for cad_s in cad_periods_s:
        for dual in (False, True):
            v.append(Variant("b08_ts011_channels", f"cad{cad_s:g}_{'dual' if dual else 'single'}",
                             "2 kanały WOR" if dual else "1 kanał WOR", "Review_TS011_Base",
                             30, max(20, runs // 2), cad_s, "2 kanały" if dual else "1 kanał", {
                                 "**.ts011CadPeriodicity": f"{cad_s:g}s",
                                 "**.ts011SecondChannelEnabled": str(dual).lower(),
                             }))

    for force_sf, label in ((12, "SF12"), (-1, "ADR")):
        v.append(Variant("n18_adr", label.lower(), label, "Review_EP_Base", 30, runs,
                         force_sf, label, {"**.forceSF": force_sf}))

    for days in (1, 2, 4, 7, 14, 30):
        for series, config in (("EP4312388", "Review_EP_Base"),
                               ("TS011", "Review_TS011_Base")):
            key = f"d{days}" if series == "EP4312388" else f"ts011_d{days}"
            v.append(Variant("n07_initialization", key, f"{series}, {days} d", config,
                             days, runs, days, series, {"warmup-period": "0d"}))

    for weak in (2, 3, 5, 10, 20):
        v.append(Variant("n10_relay_load", f"weak{weak}", f"{weak} słabych ED", "Review_EP_Base",
                         30, max(20, runs // 2), weak, "1 RL", {
                             **population(50, weak), "**.extranodex1": weak,
                             "**.extranodex2": 0, "**.numRelays": 1,
                             "**.relay1x": 300, "**.relay1y": 400,
                         }))

    for weak in (10, 20, 40):
        for relays in (1, 2):
            common = {**population(50, weak), "**.numRelays": relays}
            if relays == 1:
                common.update({"**.relay1x": 300, "**.relay1y": 450})
            v.append(Variant("n08_n11_weak_share", f"weak{weak}_r{relays}", f"{relays} RL",
                             "Review_EP_Base", 30, max(20, runs // 2), weak, f"{relays} RL", common))

    gateway_topologies = (
        ("1gw_central", "1 GW: centralna", 1, 750, 450, 150, 250),
        ("2gw_original", "2 GW: układ z rozprawy", 2, 750, 450, 150, 250),
        ("2gw_west", "2 GW: para zachodnia", 2, 550, 300, 550, 650),
        ("2gw_compromise", "2 GW: kompromis", 2, 600, 350, 350, 650),
    )
    for key, label, count, x1, y1, x2, y2 in gateway_topologies:
        v.append(Variant("n12_n13_gateway_position", key, label, "Review_EP_Base", 30, runs,
                         len(v), "GW", {**population(50), "**.numRelays": 0,
                         "**.numGateways": count, "**.gw1x": x1, "**.gw1y": y1,
                         "**.gw2x": x2, "**.gw2y": y2}))

    relay_topologies = (
        ("original", "RL: układ z rozprawy", 300, 250, 300, 650),
        ("island_centres", "RL: środki wysp", 258, 400, 200, 700),
        ("towards_gateway", "RL: bliżej GW", 450, 350, 450, 600),
        ("balanced", "RL: kompromis łącza", 375, 350, 375, 600),
    )
    for index, (key, label, x1, y1, x2, y2) in enumerate(relay_topologies):
        v.append(Variant("n14_relay_position", key, label, "Review_EP_Base", 30, runs,
                         index, "RL", {**population(50), "**.numRelays": 2,
                         "**.relay1x": x1, "**.relay1y": y1,
                         "**.relay2x": x2, "**.relay2y": y2}))

    for min_radius, label in ((0, "jednorodny dysk"), (250, "pierścień brzegowy")):
        v.append(Variant("p16_distribution", label.replace(" ", "_"), label,
                         "Review_EP_Base", 30, runs, min_radius, label,
                         {"**.centralMinRadius": min_radius}))

    for sigma in (2.0, 4.41, 8.0):
        for dynamic in (False, True):
            label = "czasowo zmienny" if dynamic else "statyczny"
            v.append(Variant("p05_p06_propagation", f"s{sigma:g}_{'dyn' if dynamic else 'static'}",
                             label, "Review_EP_Dynamic" if dynamic else "Review_EP_Base",
                             30, runs, sigma, label, {"**.pathlosStd": sigma,
                             "**.enableKeeperDistortion": int(dynamic)}))
    return v


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=DEFAULT_REPO)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--figures", type=Path, default=DEFAULT_FIGURES)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--groups", default="all")
    parser.add_argument("--plots-only", action="store_true")
    parser.add_argument("--no-resume", action="store_true")
    parser.add_argument("--quick", action="store_true")
    return parser.parse_args()


def environment(output: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join((str(OMNET_BIN), str(OMNET_CLANG_BIN), env.get("PATH", "")))
    env["MPLCONFIGDIR"] = str(output / ".matplotlib")
    Path(env["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)
    return env


def override_arg(key: str, value: object) -> str:
    return f"--{key}={value}"


def run_one(repo: Path, exe: Path, output: Path, variant: Variant, repetition: int,
            resume: bool, env: dict[str, str]) -> Path:
    run_dir = output / "variants" / variant.destination_name / "runs" / f"run_{repetition:02d}"
    result_text = run_dir / "exec_results.txt"
    if resume and result_text.exists() and "Daily read success (UL)" in result_text.read_text(
        encoding="utf-8", errors="replace"
    ):
        return run_dir
    run_dir.mkdir(parents=True, exist_ok=True)
    for old in run_dir.glob("*"):
        if old.is_file():
            old.unlink()
    command = [
        str(exe), "-u", "Cmdenv", "-f", str(repo / "simulations" / "omnetpp_review.ini"),
        "-c", variant.config, "-r", str(repetition), "-n",
        f"{repo / 'simulations'};{repo / 'src'}", "--cmdenv-express-mode=true",
        "--debug-on-errors=false", f"--sim-time-limit={variant.days}d",
        "--output-scalar-file=results.sca", "--output-vector-file=results.vec",
        "--**.scalar-recording=true", "--**.vector-recording=false",
    ]
    command.extend(override_arg(key, value) for key, value in variant.overrides.items())
    (run_dir / "command.txt").write_text(subprocess.list2cmdline(command) + "\n", encoding="utf-8")
    with (run_dir / "simulation.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, cwd=run_dir, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, text=True, check=False)
    if completed.returncode != 0 or not result_text.exists():
        raise RuntimeError(f"{variant.destination_name}, run {repetition}: see {run_dir / 'simulation.log'}")
    for extra in (run_dir / "results.vec", run_dir / "results.vci"):
        if extra.exists():
            extra.unlink()
    return run_dir


def parse_result(path: Path) -> dict[str, float | int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    result: dict[str, float | int] = {}
    for name, pattern in RESULT_PATTERNS.items():
        match = pattern.search(text)
        if match:
            result[name] = int(match.group(1)) if name in INTEGER_FIELDS else float(match.group(1))
    if "offline_longest_s" not in result and "Devices offline longest = nan[s]" in text:
        # OMNeT++ reports NaN when no completed offline interval was observed.
        result["offline_longest_s"] = 0.0
    if "daily_success_percent" not in result:
        raise ValueError(f"incomplete result: {path}")
    tx = int(result.get("packets_transmitted_ul", 0))
    result["collisions_per_1000"] = 1000.0 * int(result.get("collisions", 0)) / max(1, tx)
    return result


def parse_scalars(path: Path, days: int) -> dict[str, float]:
    module_energy: dict[str, float] = {}
    values: dict[str, list[float]] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = SCALAR_RE.match(line)
        if not match:
            continue
        module, name, value = match.group("path"), match.group("name"), float(match.group("value"))
        values.setdefault(name, []).append(value)
        if name in ENERGY_NAMES:
            host = module[:-6] if module.endswith(".radio") else module
            module_energy[host] = module_energy.get(host, 0.0) + value
    seconds = max(1, days) * 86400.0
    ed = [energy / seconds * 1000.0 for module, energy in module_energy.items()
          if re.search(r"\.endnode(?:1|2)?\[", module)]
    relay = [energy / seconds * 1000.0 for module, energy in module_energy.items()
             if ".relay[" in module]
    result = {
        "ed_current_uA": statistics.fmean(ed) if ed else math.nan,
        "relay_current_uA": statistics.fmean(relay) if relay else math.nan,
        "relay_current_max_uA": max(relay) if relay else math.nan,
    }
    scalar_map = {
        "TS011 uplinks forwarded": "relay_uplinks_forwarded",
        "TS011 forwarding-limit drops": "forwarding_limit_drops",
        "TS011 synchronization losses": "sync_losses",
        "TS011 WOR transmitted": "wor_transmitted",
        "TS011 total WOR airtime": "wor_airtime_s",
    }
    for scalar_name, output_name in scalar_map.items():
        result[output_name] = sum(values.get(scalar_name, ()))
    return result


def collect_rows(output: Path, variants: Sequence[Variant]) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for variant in variants:
        for repetition in range(variant.runs):
            run_dir = output / "variants" / variant.destination_name / "runs" / f"run_{repetition:02d}"
            row: dict[str, object] = {
                "group": variant.group, "variant": variant.key, "label": variant.label,
                "series": variant.series, "x": variant.x, "run": repetition,
                "days": variant.days,
            }
            row.update(parse_result(run_dir / "exec_results.txt"))
            row.update(parse_scalars(run_dir / "results.sca", variant.days))
            rows.append(row)
    fieldnames = list(rows[0])
    with (output / "chapter1_runs.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    return rows


def quantile(values: Sequence[float], p: float) -> float:
    return float(np.quantile(np.asarray(values, dtype=float), p))


def summarise(rows: Sequence[dict[str, object]]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str], list[dict[str, object]]] = {}
    for row in rows:
        grouped.setdefault((str(row["group"]), str(row["variant"])), []).append(row)
    output: list[dict[str, object]] = []
    metrics = (
        "daily_success_percent", "pdr_ul_percent", "collisions_per_1000",
        "utilisation_percent", "average_delay_s", "offline_longest_s", "rejoins",
        "ed_current_uA", "relay_current_uA", "relay_current_max_uA",
        "relay_uplinks_forwarded", "forwarding_limit_drops", "sync_losses",
        "wor_airtime_s",
    )
    for (_, _), group_rows in grouped.items():
        first = group_rows[0]
        item: dict[str, object] = {name: first[name] for name in
            ("group", "variant", "label", "series", "x", "days")}
        item["runs"] = len(group_rows)
        for metric in metrics:
            vals = [float(row[metric]) for row in group_rows
                    if metric in row and math.isfinite(float(row[metric]))]
            if not vals:
                continue
            mean = statistics.fmean(vals)
            se = statistics.stdev(vals) / math.sqrt(len(vals)) if len(vals) > 1 else 0.0
            item[f"{metric}_mean"] = mean
            item[f"{metric}_median"] = statistics.median(vals)
            item[f"{metric}_q1"] = quantile(vals, 0.25)
            item[f"{metric}_q3"] = quantile(vals, 0.75)
            item[f"{metric}_ci_low"] = mean - 2.045 * se
            item[f"{metric}_ci_high"] = mean + 2.045 * se
        output.append(item)
    fieldnames = sorted({key for item in output for key in item})
    return output, fieldnames


def write_summary(path: Path, summary: Sequence[dict[str, object]], fields: Sequence[str]) -> None:
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(summary)


def subset(summary: Sequence[dict[str, object]], group: str) -> list[dict[str, object]]:
    return [row for row in summary if row["group"] == group]


def metric(row: dict[str, object], name: str) -> float:
    return float(row[f"{name}_mean"])


def error(row: dict[str, object], name: str) -> tuple[float, float]:
    mean = metric(row, name)
    return mean - float(row[f"{name}_ci_low"]), float(row[f"{name}_ci_high"]) - mean


def line_by_series(ax, data: Sequence[dict[str, object]], field: str, ylabel: str) -> None:
    for series in sorted({str(row["series"]) for row in data}):
        part = sorted((row for row in data if row["series"] == series), key=lambda row: float(row["x"]))
        x = [float(row["x"]) for row in part]
        y = [metric(row, field) for row in part]
        errs = np.asarray([error(row, field) for row in part]).T
        ax.errorbar(x, y, yerr=errs, marker="o", capsize=3, linewidth=1.6, label=series)
    ax.set_xlabel("liczba urządzeń ED")
    ax.set_ylabel(ylabel)
    ax.legend()


def save(fig, figures: Path, name: str) -> Path:
    destination = figures / name
    fig.savefig(destination, dpi=220, bbox_inches="tight")
    plt.close(fig)
    return destination


def make_plots(summary: Sequence[dict[str, object]], figures: Path) -> list[Path]:
    configure_style()
    figures.mkdir(parents=True, exist_ok=True)
    made: list[Path] = []

    data = subset(summary, "b11_comparison")
    fig, axes = plt.subplots(2, 2, figsize=(10.2, 10.6))
    line_by_series(axes[0, 0], data, "daily_success_percent", r"$P_{\geq1/d}$ [%]")
    line_by_series(axes[0, 1], data, "pdr_ul_percent", r"$P_{UL}$ [%]")
    line_by_series(axes[1, 0], data, "collisions_per_1000", "kolizje / 1000 ramek")
    line_by_series(axes[1, 1], data, "relay_current_uA", r"średni prąd RL [$\mu$A]")

    at_550 = {str(row["series"]): row for row in data if float(row["x"]) == 550}
    limited = metric(at_550["TS011"], "daily_success_percent")
    unlimited = metric(at_550["TS011 bez limitu"], "daily_success_percent")
    ep = metric(at_550["EP4312388"], "daily_success_percent")
    ax = axes[0, 0]
    arrow_x = 525
    ax.annotate("", xy=(arrow_x, unlimited), xytext=(arrow_x, limited),
                arrowprops={"arrowstyle": "<->", "color": "#444444", "lw": 1.4})
    ax.text(515, (limited + unlimited) / 2,
            "wpływ limitu\n+" + f"{unlimited - limited:.2f}".replace(".", ",") + " p.p.",
            ha="right", va="center", fontsize=9,
            bbox={"boxstyle": "round,pad=0.25", "fc": "white", "ec": "#777777", "alpha": 0.9})
    ax.annotate("", xy=(arrow_x, ep), xytext=(arrow_x, unlimited),
                arrowprops={"arrowstyle": "<->", "color": "#444444", "lw": 1.4})
    ax.text(515, (unlimited + ep) / 2,
            "pozostała różnica\n" + f"{ep - unlimited:.2f}".replace(".", ",") + " p.p.",
            ha="right", va="center", fontsize=9,
            bbox={"boxstyle": "round,pad=0.25", "fc": "white", "ec": "#777777", "alpha": 0.9})

    fig.suptitle("Wpływ limitu przekazywania TS011 na porównanie z EP4312388")
    fig.tight_layout()
    made.append(save(fig, figures, "B11_porownanie_TS011_EP.png"))

    if not subset(summary, "b10_alarm"):
        return made

    data = subset(summary, "b10_alarm")
    fig, axes = plt.subplots(1, 2, figsize=(10.2, 4.0))
    line_by_series(axes[0], data, "pdr_ul_percent", r"$P_{UL}$ [%]")
    line_by_series(axes[1], data, "collisions_per_1000", "kolizje / 1000 ramek")
    fig.suptitle("Jednominutowy ruch alarmowy i nasycenie sieci")
    fig.tight_layout()
    made.append(save(fig, figures, "B10_ruch_alarmowy.png"))

    data = subset(summary, "b08_window_n")
    windows = sorted({int(row["series"]) for row in data})
    frames = sorted({int(float(row["x"])) for row in data})
    pdr = np.asarray([[metric(next(row for row in data if int(row["series"]) == w and int(float(row["x"])) == n),
                              "daily_success_percent") for n in frames] for w in windows])
    current = np.asarray([[metric(next(row for row in data if int(row["series"]) == w and int(float(row["x"])) == n),
                                  "ed_current_uA") for n in frames] for w in windows])
    fig, axes = plt.subplots(1, 2, figsize=(10.2, 3.8))
    for ax, matrix, title, fmt in ((axes[0], pdr, r"$P_{\geq1/d}$ [%]", ".1f"),
                                    (axes[1], current, r"średni prąd ED [$\mu$A]", ".1f")):
        image = ax.imshow(matrix, aspect="auto", cmap="viridis")
        for iy in range(len(windows)):
            for ix in range(len(frames)):
                ax.text(ix, iy, format(matrix[iy, ix], fmt), ha="center", va="center",
                        color="white" if matrix[iy, ix] < np.nanmean(matrix) else "black", fontsize=8)
        ax.set_xticks(range(len(frames)), frames)
        ax.set_yticks(range(len(windows)), windows)
        ax.set_xlabel("liczba ramek N w oknie")
        ax.set_ylabel("długość okna [min]")
        ax.set_title(title)
        fig.colorbar(image, ax=ax, shrink=0.88)
    fig.suptitle("Wrażliwość EP4312388 na długość okna i liczbę ramek")
    fig.tight_layout()
    made.append(save(fig, figures, "B08_okno_liczba_ramek.png"))

    data = subset(summary, "b08_ts011_channels")
    fig, axes = plt.subplots(1, 2, figsize=(10.2, 4.0))
    for series in ("1 kanał", "2 kanały"):
        part = sorted((row for row in data if row["series"] == series), key=lambda row: float(row["x"]))
        x = [1000 * float(row["x"]) for row in part]
        for ax, field in ((axes[0], "daily_success_percent"),
                          (axes[1], "relay_current_uA")):
            ax.plot(x, [metric(row, field) for row in part], marker="o",
                    markersize=4.2, linewidth=1.5, label=series)
    for ax, ylabel in zip(axes, (r"$P_{\geq1/d}$ [%]", r"średni prąd RL [$\mu$A]")):
        ax.set_xlabel("CADPeriodicity [ms]")
        ax.set_ylabel(ylabel)
        ax.legend()
    fig.suptitle("TS011: wszystkie wartości CADPeriodicity i opcjonalny drugi kanał WOR")
    fig.tight_layout()
    made.append(save(fig, figures, "B08_TS011_CAD_kanaly.png"))

    init = subset(summary, "n07_initialization")
    fig, axes = plt.subplots(1, 2, figsize=(10.2, 4.0))
    for series in ("EP4312388", "TS011"):
        part = sorted((row for row in init if row["series"] == series),
                      key=lambda row: float(row["x"]))
        for ax, field in ((axes[0], "daily_success_percent"),
                          (axes[1], "relay_current_uA")):
            errs = np.asarray([error(row, field) for row in part]).T
            ax.errorbar([row["x"] for row in part], [metric(row, field) for row in part],
                        yerr=errs, marker="o", capsize=3, linewidth=1.6, label=series)
    axes[0].set(xlabel="horyzont obserwacji [d]", ylabel=r"$P_{\geq1/d}$ [%]")
    axes[1].set(xlabel="horyzont obserwacji [d]", ylabel=r"średni prąd RL [$\mu$A]")
    axes[0].legend()
    axes[1].legend()
    fig.suptitle("Wpływ fazy inicjalnej: EP4312388 i TS011")
    fig.tight_layout()
    made.append(save(fig, figures, "N07_faza_inicjalna.png"))

    single_load = sorted(subset(summary, "n10_relay_load"), key=lambda row: float(row["x"]))
    load = subset(summary, "n08_n11_weak_share")
    fig, axes = plt.subplots(2, 2, figsize=(10.2, 7.4))
    axes[0, 0].plot([row["x"] for row in single_load],
                    [metric(row, "daily_success_percent") for row in single_load], marker="o")
    axes[0, 1].plot([row["x"] for row in single_load],
                    [metric(row, "relay_current_uA") for row in single_load], marker="o", color="#f8961e")
    axes[0, 0].set(xlabel="słabe ED obsługiwane przez 1 RL", ylabel=r"$P_{\geq1/d}$ [%]")
    axes[0, 1].set(xlabel="słabe ED obsługiwane przez 1 RL", ylabel=r"średni prąd RL [$\mu$A]")
    for series in ("1 RL", "2 RL"):
        part = sorted((row for row in load if row["series"] == series), key=lambda row: float(row["x"]))
        axes[1, 0].plot([row["x"] for row in part], [metric(row, "daily_success_percent") for row in part], marker="o", label=series)
        axes[1, 1].plot([row["x"] for row in part], [metric(row, "relay_current_max_uA") for row in part], marker="o", label=series)
    axes[1, 0].set(xlabel="liczba słabych ED na dwóch wyspach", ylabel=r"$P_{\geq1/d}$ [%]")
    axes[1, 1].set(xlabel="liczba słabych ED na dwóch wyspach", ylabel=r"maks. średni prąd pojedynczego RL [$\mu$A]")
    axes[1, 0].legend()
    axes[1, 1].legend()
    fig.suptitle("Udział słabych urządzeń i rozłożenie obciążenia RL")
    fig.tight_layout()
    made.append(save(fig, figures, "N08_N11_udzial_slabych_ED.png"))

    gateway = subset(summary, "n12_n13_gateway_position")
    relay = subset(summary, "n14_relay_position")
    fig, axes = plt.subplots(1, 2, figsize=(10.8, 4.5))
    for ax, data, title in ((axes[0], gateway, "Położenie GW"), (axes[1], relay, "Położenie RL")):
        ordered = sorted(data, key=lambda row: metric(row, "daily_success_percent"))
        ax.barh([str(row["label"]) for row in ordered], [metric(row, "daily_success_percent") for row in ordered], color="#277da1")
        ax.set_xlabel(r"$P_{\geq1/d}$ [%]")
        ax.set_title(title)
        ax.set_xlim(max(0, min(metric(row, "daily_success_percent") for row in ordered) - 3), 100)
    fig.suptitle("Wrażliwość wyniku na arbitralny wybór lokalizacji")
    fig.tight_layout()
    made.append(save(fig, figures, "N12_N14_lokalizacja_GW_RL.png"))

    distribution = subset(summary, "p16_distribution")
    rng = np.random.default_rng(20260913)
    theta = rng.uniform(0, 2 * np.pi, 200)
    fig, axes = plt.subplots(1, 3, figsize=(11.4, 3.8), gridspec_kw={"width_ratios": [1, 1, 1.15]})
    for ax, minimum, title in ((axes[0], 0.0, "jednorodny dysk"),
                               (axes[1], 250.0, "pierścień brzegowy")):
        radius = np.sqrt(rng.uniform(minimum ** 2, 350.0 ** 2, 200))
        ax.scatter(750 + radius * np.cos(theta), 450 + radius * np.sin(theta), s=8, alpha=0.65)
        ax.scatter([750], [450], marker="^", c="#d62728", edgecolors="black", s=90)
        ax.set_title(title)
        ax.set_aspect("equal")
        ax.set(xlabel="x [m]", ylabel="y [m]")
    ordered = sorted(distribution, key=lambda row: float(row["x"]))
    positions = np.arange(len(ordered))
    axes[2].bar(positions - 0.18, [metric(row, "daily_success_percent") for row in ordered],
                width=0.36, label=r"$P_{\geq1/d}$")
    axes[2].bar(positions + 0.18, [metric(row, "pdr_ul_percent") for row in ordered],
                width=0.36, label=r"$P_{UL}$")
    axes[2].set_xticks(positions, ["dysk", "pierścień"])
    axes[2].set_ylabel("skuteczność [%]")
    axes[2].set_ylim(85, 100)
    axes[2].legend()
    axes[2].set_title("wpływ rozkładu")
    fig.suptitle("Jawna procedura losowania współrzędnych urządzeń centralnych")
    fig.tight_layout()
    made.append(save(fig, figures, "P16_rozklad_wspolrzednych.png"))

    prop = subset(summary, "p05_p06_propagation")
    fig, axes = plt.subplots(1, 2, figsize=(10.2, 4.0))
    for series in ("statyczny", "czasowo zmienny"):
        part = sorted((row for row in prop if row["series"] == series), key=lambda row: float(row["x"]))
        axes[0].plot([row["x"] for row in part], [metric(row, "daily_success_percent") for row in part], marker="o", label=series)
        axes[1].plot([row["x"] for row in part], [metric(row, "offline_longest_s") / 3600 for row in part], marker="o", label=series)
    axes[0].set(xlabel=r"$\sigma$ losowej tłumienności [dB]", ylabel=r"$P_{\geq1/d}$ [%]")
    axes[1].set(xlabel=r"$\sigma$ losowej tłumienności [dB]", ylabel="najdłuższy okres offline [h]")
    for ax in axes: ax.legend()
    fig.suptitle("Model statyczny i czasowo zmienna składowa propagacyjna")
    fig.tight_layout()
    made.append(save(fig, figures, "P05_P06_propagacja_AB.png"))

    extra = subset(summary, "n15_infrastructure") + subset(summary, "n18_adr")
    fig, ax = plt.subplots(figsize=(9.2, 4.2))
    labels = [str(row["label"]) for row in extra]
    success = [metric(row, "daily_success_percent") for row in extra]
    current = [metric(row, "ed_current_uA") for row in extra]
    positions = np.arange(len(extra))
    ax.bar(positions - 0.18, success, width=0.36, color="#277da1", label=r"$P_{\geq1/d}$ [%]")
    ax2 = ax.twinx()
    ax2.bar(positions + 0.18, current, width=0.36, color="#f8961e", label=r"$I_{ED}$ [$\mu$A]")
    ax.set_xticks(positions, labels, rotation=18, ha="right")
    ax.set_ylabel(r"$P_{\geq1/d}$ [%]")
    ax2.set_ylabel(r"średni prąd ED [$\mu$A]")
    ax.legend(loc="upper left")
    ax2.legend(loc="upper right")
    ax.set_title("Brakujący wariant 2 GW oraz znaczenie ADR")
    fig.tight_layout()
    made.append(save(fig, figures, "N15_N18_2GW_ADR.png"))
    return made


def main() -> int:
    args = parse_args()
    output = args.output.resolve()
    figures = args.figures.resolve()
    output.mkdir(parents=True, exist_ok=True)
    plan = experiment_plan(3 if args.quick else args.runs)
    if args.quick:
        plan = [Variant(**{**asdict(item), "days": min(item.days, 3), "runs": 3,
                           "overrides": {**item.overrides, "warmup-period": "0d"}})
                for item in plan]
    if args.groups.lower() != "all":
        selected = {part.strip() for part in args.groups.split(",") if part.strip()}
        plan = [item for item in plan if item.group in selected]
    if not plan:
        raise SystemExit("No variants selected")
    (output / "plan.json").write_text(json.dumps([asdict(item) for item in plan], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    env = environment(output)
    if not args.plots_only:
        jobs = [(variant, repetition) for variant in plan for repetition in range(variant.runs)]
        print(f"Running {len(jobs)} simulations in {len(plan)} variants", flush=True)
        with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
            futures = {pool.submit(run_one, args.repo.resolve(), args.exe.resolve(), output,
                                   variant, repetition, not args.no_resume, env): (variant, repetition)
                       for variant, repetition in jobs}
            completed = 0
            for future in as_completed(futures):
                variant, repetition = futures[future]
                future.result()
                completed += 1
                if completed % 25 == 0 or completed == len(jobs):
                    print(f"Completed {completed}/{len(jobs)}", flush=True)

    rows = collect_rows(output, plan)
    summary, fields = summarise(rows)
    write_summary(output / "chapter1_summary.csv", summary, fields)
    plots = make_plots(summary, figures)
    if any(row["group"] == "b11_comparison" for row in summary):
        plots.extend(draw_probability_figure(refine_probabilities(summary, output), figures, output))

    source_paths = [
        Path("src/common.h"),
        Path("src/Keeper.h"),
        Path("src/Keeper.cc"),
        Path("src/LogicEndnode.cc"),
        Path("src/LogicRelay.cc"),
        Path("src/LogicRelay.h"),
        Path("src/Radio.cc"),
        Path("simulations/Chapter7Network.ned"),
        Path("simulations/omnetpp_review.ini"),
        Path("scripts/generate_review_chapter1_results.py"),
        Path("scripts/generate_b11_probability_figure.py"),
        Path("scripts/analyze_b11_case.py"),
    ]
    source_hash = hashlib.sha256()
    snapshot_dir = output / "reproducibility"
    snapshot_dir.mkdir(parents=True, exist_ok=True)
    for relative_path in source_paths:
        source_path = args.repo.resolve() / relative_path
        source_hash.update(relative_path.as_posix().encode("utf-8"))
        source_hash.update(source_path.read_bytes())
        snapshot_path = snapshot_dir / relative_path
        snapshot_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, snapshot_path)
    manifest = {
        "simulation_count": len(rows), "variant_count": len(plan),
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.repo,
                                                   text=True).strip(),
        "source_snapshot_sha256": source_hash.hexdigest(),
        "source_snapshot": str(snapshot_dir.resolve()),
        "source_files": [path.as_posix() for path in source_paths],
        "config": str((args.repo / "simulations" / "omnetpp_review.ini").resolve()),
        "summary": str((output / "chapter1_summary.csv").resolve()),
        "figures": [str(path) for path in plots],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
