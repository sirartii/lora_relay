#!/usr/bin/env python3
"""Run and plot the complete chapter 7 LoRa simulation result set.

The simulator writes text files (``exec_results*.txt``); those files remain
the primary input to the analysis.  Scalar files are used only to calculate
the mean current correctly from the four radio energy components.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence

# Avoid attempts to create Matplotlib's font cache in a protected profile
# directory when the script is started by a background Codex/OMNeT++ job.
os.environ.setdefault(
    "MPLCONFIGDIR", str(Path(os.environ.get("TEMP", ".")) / "lora_relay_matplotlib")
)

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.cm import ScalarMappable
from matplotlib.colors import Normalize

from topology_io import EndDevice, FixedNode, TopologyRun, parse_topology
from topology_plotting import apply_canvas, configure_style, draw_topology


TEXT_OUTPUTS = (
    "exec_results.txt",
    "exec_results_topology.txt",
    "exec_result_frames.txt",
)
DEFAULT_OUTPUT = Path(r"C:\Omnet\simulation_results\chapter7")
DEFAULT_EXE = Path(r"C:\Omnet\lora_relay\out\clang-release\src\opp-lora-relay-official.exe")
OMNET_BIN = Path(r"C:\omnetpp-6.4.0\bin")
OMNET_CLANG_BIN = Path(r"C:\omnetpp-6.4.0\tools\win32.x86_64\clang64\bin")
CANVAS = (0.0, 1400.0, 0.0, 900.0)


@dataclass(frozen=True)
class Scenario:
    number: int
    folder: str
    label: str
    gateways: int
    relay: str
    sf: str
    frames: int
    window_hours: int
    published_daily: float | None
    published_ed_current: float | None
    published_relay_current: float | None

    @property
    def key(self) -> str:
        return f"t{self.number:02d}"

    @property
    def config(self) -> str:
        return f"Chapter7_T{self.number:02d}"


SCENARIOS = (
    Scenario(1, "01_1GW_SF12_N1_3h", "1 GW / SF12 / N=1 / 3 h", 1, "-", "12", 1, 3, 94.26, 27.26, None),
    Scenario(2, "02_1GW_SF12_N1_12h", "1 GW / SF12 / N=1 / 12 h", 1, "-", "12", 1, 12, 95.80, 27.26, None),
    Scenario(3, "03_1GW_SF12_N3_12h", "1 GW / SF12 / N=3 / 12 h", 1, "-", "12", 3, 12, 97.19, 31.12, None),
    Scenario(4, "04_1GW_SF12_N4_12h", "1 GW / SF12 / N=4 / 12 h", 1, "-", "12", 4, 12, 97.03, 33.19, None),
    Scenario(5, "05_1GW_ADR_N4_12h", "1 GW / ADR / N=4 / 12 h", 1, "-", "ADR", 4, 12, 96.50, 27.64, None),
    Scenario(6, "06_1GW_ADR_N6_12h", "1 GW / ADR / N=6 / 12 h", 1, "-", "ADR", 6, 12, 95.72, 28.74, None),
    Scenario(7, "07_2GW_SF12_N1_12h", "2 GW / SF12 / N=1 / 12 h", 2, "-", "12", 1, 12, 99.05, 27.26, None),
    Scenario(8, "08_3GW_SF12_N1_12h", "3 GW / SF12 / N=1 / 12 h", 3, "-", "12", 1, 12, 99.79, 27.26, None),
    Scenario(9, "09_3GW_ADR_N2_12h", "3 GW / ADR / N=2 / 12 h", 3, "-", "ADR", 2, 12, 99.91, 26.92, None),
    Scenario(10, "10_1GW_2RL_TS011_SF12_N2_12h", "1 GW + 2 RL TS011 / SF12 / N=2", 1, "2 RL (TS011)", "12", 2, 12, 98.82, 28.90, 141.62),
    Scenario(11, "11_1GW_2RL_EP431_SF12_N2_12h", "1 GW + 2 RL EP431 / SF12 / N=2", 1, "2 RL (EP431)", "12", 2, 12, 99.47, 28.80, 19.38),
    Scenario(12, "12_1GW_2IP_SF12_N2_12h", "1 GW + 2 LoRa-IP / SF12 / N=2", 1, "2 LL (IP)", "12", 2, 12, 99.49, 28.80, 135.12),
    Scenario(13, "13_2GW_SF12_N2_12h", "2 GW / SF12 / N=2 / 12 h (dodatkowy)", 2, "-", "12", 2, 12, None, None, None),
)
SCENARIO_BY_KEY = {scenario.key: scenario for scenario in SCENARIOS}
FIGURE_713_MODELS = ("t02", "t08", "t09", "t10", "t11", "t12")


RESULT_PATTERNS = {
    "end_devices": re.compile(r"^ED\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "simulated_days": re.compile(r"^Simulated days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "warmup_days": re.compile(r"^Warmup days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "statistics_days": re.compile(r"^Statistics days\s*=\s*([0-9]+)\s*$", re.MULTILINE),
    "utilisation_percent": re.compile(r"Total utilisation\s*=\s*([-+0-9.eE]+)\[%\]"),
    "packets_transmitted_ul": re.compile(r"Packets transmitted UL\s*=\s*([0-9]+)"),
    "packets_delivered_ul": re.compile(r"Packets delivered UL\s*=\s*([0-9]+)"),
    "pdr_ul_percent": re.compile(r"Packets delivered UL ef\s*=\s*([-+0-9.eE]+)\[%\]"),
    "daily_success_percent": re.compile(r"Daily read success \(UL\)\[%\]\s*=\s*([-+0-9.eE]+)\[%\]"),
    "collisions": re.compile(r"Total collisions\s*=\s*([0-9]+)"),
    "rejoins": re.compile(r"Devices re-join count\s*=\s*([0-9]+)"),
}
INTEGER_METRICS = {
    "end_devices",
    "simulated_days",
    "warmup_days",
    "statistics_days",
    "packets_transmitted_ul",
    "packets_delivered_ul",
    "collisions",
    "rejoins",
}
SCALAR_ENERGY_RE = re.compile(
    r'^scalar\s+(?P<path>\S+)\s+"Energy consumption total '
    r'(?:Tx|Rx|Idle|WOR|BaseOffset|LTE attach|LTE transmit)"\s+(?P<value>[-+0-9.eE]+)$'
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Uruchamia 30 powtórzeń scenariuszy rozdziału 7 i generuje rysunki."
    )
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--days", type=int, default=30)
    parser.add_argument(
        "--sections",
        default="table,7.2,7.3,7.5,7.6,7.7,7.12,7.13",
        help="lista: table,7.2,7.3,7.5,7.6,7.7,7.12,7.13",
    )
    parser.add_argument(
        "--scenarios",
        default="all",
        help="scenariusze tabeli, np. t01,t05,t13 albo all",
    )
    parser.add_argument("--n-values", default="1,2,3,4,5,6,7,8")
    parser.add_argument("--populations", default="50,100,150,200,250,300,350,400,450,500")
    parser.add_argument("--rx-current-ma", type=float, default=4.6)
    parser.add_argument(
        "--table-source",
        type=Path,
        help="importuje istniejące 30 przebiegów tabeli 7.1 zamiast liczyć je ponownie",
    )
    parser.add_argument("--plots-only", action="store_true")
    parser.add_argument("--no-resume", action="store_true")
    parser.add_argument("--keep-vectors", action="store_true")
    parser.add_argument("--list", action="store_true", help="wypisuje plan i kończy")
    args = parser.parse_args()
    if args.runs < 1 or args.days < 1:
        parser.error("--runs i --days muszą być dodatnie")
    return args


def parse_int_list(value: str) -> list[int]:
    result = [int(part.strip()) for part in value.split(",") if part.strip()]
    if not result or any(item < 1 for item in result):
        raise ValueError(f"niepoprawna lista dodatnich liczb: {value}")
    return result


def select_scenarios(value: str) -> list[Scenario]:
    if value.strip().lower() == "all":
        return list(SCENARIOS)
    keys = [part.strip().lower() for part in value.split(",") if part.strip()]
    unknown = [key for key in keys if key not in SCENARIO_BY_KEY]
    if unknown:
        raise ValueError(f"nieznane scenariusze: {', '.join(unknown)}")
    return [SCENARIO_BY_KEY[key] for key in keys]


def format_override(key: str, value: object) -> str:
    if isinstance(value, float):
        text = f"{value:g}"
    else:
        text = str(value)
    return f"--{key}={text}"


def simulator_environment(output_root: Path) -> dict[str, str]:
    env = os.environ.copy()
    additions = [str(OMNET_BIN), str(OMNET_CLANG_BIN)]
    env["PATH"] = os.pathsep.join(additions + [env.get("PATH", "")])
    mpl_dir = output_root / ".matplotlib"
    mpl_dir.mkdir(parents=True, exist_ok=True)
    env["MPLCONFIGDIR"] = str(mpl_dir)
    return env


def run_one(
    *,
    repo: Path,
    exe: Path,
    config: str,
    repetition: int,
    run_dir: Path,
    days: int,
    rx_current_ma: float,
    overrides: dict[str, object] | None,
    resume: bool,
    keep_vectors: bool,
    env: dict[str, str],
) -> None:
    result_text = run_dir / "exec_results.txt"
    if resume and result_text.exists() and "Daily read success (UL)" in result_text.read_text(
        encoding="utf-8", errors="replace"
    ):
        return

    run_dir.mkdir(parents=True, exist_ok=True)
    for name in TEXT_OUTPUTS:
        path = run_dir / name
        if path.exists():
            path.unlink()

    ini = repo / "simulations" / "omnetpp_chapter7.ini"
    ned_path = f"{repo / 'simulations'};{repo / 'src'}"
    scalar = run_dir / "results.sca"
    vector = run_dir / "results.vec"
    command = [
        str(exe),
        "-u",
        "Cmdenv",
        "-f",
        str(ini),
        "-c",
        config,
        "-r",
        str(repetition),
        "-n",
        ned_path,
        "--cmdenv-express-mode=true",
        "--debug-on-errors=false",
        f"--sim-time-limit={days}d",
        f"--output-scalar-file={scalar}",
        f"--output-vector-file={vector}",
        "--**.scalar-recording=true",
        f"--**.vector-recording={'true' if keep_vectors else 'false'}",
        format_override("**.currentConsumptionRx", rx_current_ma),
    ]
    for key, value in (overrides or {}).items():
        command.append(format_override(key, value))

    (run_dir / "command.txt").write_text(
        subprocess.list2cmdline(command) + "\n", encoding="utf-8"
    )
    with (run_dir / "simulation.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            command,
            cwd=run_dir,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    if completed.returncode != 0:
        raise RuntimeError(
            f"symulacja {config}, run {repetition} zakończyła się kodem "
            f"{completed.returncode}; zobacz {run_dir / 'simulation.log'}"
        )
    if not result_text.exists():
        raise RuntimeError(f"symulacja nie utworzyła {result_text}")
    if not keep_vectors:
        for suffix in (".vec", ".vci"):
            candidate = vector.with_suffix(suffix)
            if candidate.exists():
                candidate.unlink()


def simulate_variant(
    *,
    repo: Path,
    exe: Path,
    config: str,
    destination: Path,
    runs: int,
    days: int,
    rx_current_ma: float,
    overrides: dict[str, object] | None,
    resume: bool,
    keep_vectors: bool,
    env: dict[str, str],
) -> None:
    for repetition in range(runs):
        print(f"  {destination.name}: {repetition + 1}/{runs}", flush=True)
        run_one(
            repo=repo,
            exe=exe,
            config=config,
            repetition=repetition,
            run_dir=destination / "runs" / f"run_{repetition:02d}",
            days=days,
            rx_current_ma=rx_current_ma,
            overrides=overrides,
            resume=resume,
            keep_vectors=keep_vectors,
            env=env,
        )
    combine_run_outputs(destination)


def combine_run_outputs(destination: Path) -> None:
    run_dirs = sorted((destination / "runs").glob("run_*"))
    for name in TEXT_OUTPUTS:
        sources = [run_dir / name for run_dir in run_dirs if (run_dir / name).exists()]
        if not sources:
            continue
        target = destination / name
        with target.open("w", encoding="utf-8", newline="") as stream:
            for source in sources:
                text = source.read_text(encoding="utf-8", errors="replace")
                stream.write(text)
                if text and not text.endswith("\n"):
                    stream.write("\n")


def parse_result_blocks(path: Path) -> list[dict[str, int | float]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    chunks = text.split("============== Simulation Parames ================")[1:]
    records: list[dict[str, int | float]] = []
    for chunk in chunks:
        record: dict[str, int | float] = {}
        for key, pattern in RESULT_PATTERNS.items():
            match = pattern.search(chunk)
            if match:
                record[key] = int(match.group(1)) if key in INTEGER_METRICS else float(match.group(1))
        if "daily_success_percent" in record:
            records.append(record)
    return records


def energy_from_scalar(path: Path, simulated_days: int) -> tuple[float | None, float | None]:
    totals: dict[str, float] = defaultdict(float)
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = SCALAR_ENERGY_RE.match(line)
        if not match:
            continue
        module_path = match.group("path")
        if module_path.endswith(".radio"):
            module_path = module_path[:-6]
        module = module_path.rsplit(".", 1)[-1]
        totals[module] += float(match.group("value"))

    seconds = max(1, simulated_days) * 24 * 3600
    ed = [value / seconds * 1000.0 for name, value in totals.items() if name.startswith("endnode")]
    relay = [
        value / seconds * 1000.0
        for name, value in totals.items()
        if name.startswith("relay") or name.startswith("ipbridge")
    ]
    return (statistics.fmean(ed) if ed else None, statistics.fmean(relay) if relay else None)


def find_scalar_files(destination: Path) -> list[Path]:
    per_run = sorted((destination / "runs").glob("run_*/results.sca"))
    if per_run:
        return per_run
    return sorted((destination / "scalars").glob("*.sca"))


def ci95(values: Sequence[float]) -> tuple[float, float]:
    if not values:
        return float("nan"), float("nan")
    mean = statistics.fmean(values)
    if len(values) < 2:
        return mean, mean
    critical = 2.04523 if len(values) == 30 else 1.96
    half = critical * statistics.stdev(values) / math.sqrt(len(values))
    return mean - half, mean + half


def summarise(values: Sequence[float]) -> dict[str, float | int]:
    low, high = ci95(values)
    return {
        "n": len(values),
        "mean": statistics.fmean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
        "ci95_low": low,
        "ci95_high": high,
        "min": min(values),
        "max": max(values),
    }


def collect_metrics(destination: Path) -> tuple[list[dict[str, int | float | None]], dict[str, object]]:
    records = parse_result_blocks(destination / "exec_results.txt")
    scalar_files = find_scalar_files(destination)
    for index, record in enumerate(records):
        record["run"] = index
        record["ed_current_uA"] = None
        record["relay_current_uA"] = None
        if index < len(scalar_files):
            ed_current, relay_current = energy_from_scalar(
                scalar_files[index], int(record.get("simulated_days", 30))
            )
            record["ed_current_uA"] = ed_current
            record["relay_current_uA"] = relay_current

    fields = [
        "run",
        "end_devices",
        "simulated_days",
        "warmup_days",
        "statistics_days",
        "daily_success_percent",
        "pdr_ul_percent",
        "utilisation_percent",
        "collisions",
        "rejoins",
        "packets_transmitted_ul",
        "packets_delivered_ul",
        "ed_current_uA",
        "relay_current_uA",
    ]
    with (destination / "metrics_runs.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(records)

    summary: dict[str, object] = {"completed_runs": len(records)}
    for field in (
        "daily_success_percent",
        "pdr_ul_percent",
        "utilisation_percent",
        "collisions",
        "rejoins",
        "ed_current_uA",
        "relay_current_uA",
    ):
        values = [float(record[field]) for record in records if record.get(field) is not None]
        if values:
            summary[field] = summarise(values)
    (destination / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return records, summary


def fixed_nodes_for(runs: Iterable[TopologyRun]) -> tuple[FixedNode, ...]:
    seen: set[tuple[str, int, float, float]] = set()
    result: list[FixedNode] = []
    for run in runs:
        for node in run.fixed_nodes:
            key = (node.role, node.index, node.x, node.y)
            if key not in seen:
                seen.add(key)
                result.append(node)
    return tuple(result)


def save_plot(figure, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(destination, dpi=180, bbox_inches="tight")
    plt.close(figure)


def plot_figure_71_style(destination: Path, label: str) -> Path | None:
    path = destination / "exec_results_topology.txt"
    if not path.exists():
        return None
    topology = parse_topology(path, strict=False)
    if not topology.runs:
        return None
    selected = [topology.runs[0], topology.runs[min(1, len(topology.runs) - 1)]]
    aggregate_devices = tuple(node for run in topology.runs for node in run.devices)
    fixed = fixed_nodes_for(topology.runs)

    configure_style()
    figure, axes = plt.subplots(1, 3, figsize=(17.2, 5.2), constrained_layout=True)
    for index, (ax, run) in enumerate(zip(axes[:2], selected), start=1):
        draw_topology(ax, run.devices, run.fixed_nodes, point_size=28, label_fixed_nodes=index == 1)
        apply_canvas(ax, CANVAS)
        ax.set_title(f"Przebieg {run.run_number + 1}")
    scatter = draw_topology(
        axes[2], aggregate_devices, fixed, point_size=8, alpha=0.35, label_fixed_nodes=True
    )
    apply_canvas(axes[2], CANVAS)
    axes[2].set_title(f"Łącznie {len(topology.runs)} przebiegów")
    colorbar = figure.colorbar(scatter, ax=axes, pad=0.015, shrink=0.88)
    colorbar.set_label("Prawdopodobieństwo dostarczenia ≥1 UL/dobę [%]")
    figure.suptitle(f"Układ i skuteczność urządzeń — {label}")
    output = destination / "figure_7_1_style_topology.png"
    save_plot(figure, output)
    return output


def parse_frames(path: Path) -> list[tuple[float, float, int, float, float]]:
    rows: list[tuple[float, float, int, float, float]] = []
    with path.open("r", encoding="utf-8", errors="replace", newline="") as stream:
        for row in csv.reader(stream):
            if len(row) != 5 or row[0].strip().lower() == "x":
                continue
            try:
                rows.append((float(row[0]), float(row[1]), int(row[2]), float(row[3]), float(row[4])))
            except ValueError:
                continue
    return rows


def plot_figure_76(destination: Path, label: str) -> Path | None:
    frame_path = destination / "exec_result_frames.txt"
    topology_path = destination / "exec_results_topology.txt"
    if not frame_path.exists() or not topology_path.exists():
        return None
    groups: dict[tuple[float, float], list[int]] = defaultdict(list)
    for x, y, sf, _rssi, _margin in parse_frames(frame_path):
        groups[(x, y)].append(sf)
    if not groups:
        return None
    topology = parse_topology(topology_path, strict=False)
    fixed = fixed_nodes_for(topology.runs)
    configure_style()
    figure, ax = plt.subplots(figsize=(10.8, 7.0), constrained_layout=True)
    norm = Normalize(vmin=7, vmax=12)
    scatter = ax.scatter(
        [xy[0] for xy in groups],
        [xy[1] for xy in groups],
        c=[statistics.fmean(values) for values in groups.values()],
        cmap="viridis_r",
        norm=norm,
        s=22,
        marker="o",
        alpha=0.72,
        linewidths=0,
    )
    for node in fixed:
        marker = "^" if node.role == "GW" else ("s" if node.role == "RELAY" else "D")
        ax.scatter(node.x, node.y, marker=marker, s=115, color="#d62728", edgecolors="black", zorder=4)
    apply_canvas(ax, CANVAS)
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("x [m]")
    ax.set_ylabel("y [m]")
    ax.set_title(f"Średni SF przy włączonym ADR — {label}")
    colorbar = figure.colorbar(scatter, ax=ax, pad=0.025)
    colorbar.set_label("Średni spreading factor")
    output = destination / "figure_7_6_average_sf_ADR.png"
    save_plot(figure, output)
    return output


def percentile(values: Sequence[float], percent: int) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * percent / 100.0
    low = int(math.floor(position))
    high = int(math.ceil(position))
    fraction = position - low
    return ordered[low] * (1 - fraction) + ordered[high] * fraction


def plot_figure_77(destination: Path, label: str) -> Path | None:
    frame_path = destination / "exec_result_frames.txt"
    if not frame_path.exists():
        return None
    groups: dict[tuple[float, float], list[tuple[int, float]]] = defaultdict(list)
    for x, y, sf, rssi, _margin in parse_frames(frame_path):
        groups[(x, y)].append((sf, rssi))
    usable = {xy: seq for xy, seq in groups.items() if len(seq) >= 12}
    if not usable:
        return None
    mean_rssi = {xy: statistics.fmean(rssi for _sf, rssi in seq) for xy, seq in usable.items()}
    targets = (5, 25, 49, 51, 75, 95)
    chosen: list[tuple[int, tuple[float, float]]] = []
    available = set(usable)
    all_rssi = list(mean_rssi.values())
    for target in targets:
        target_value = percentile(all_rssi, target)
        xy = min(available, key=lambda point: abs(mean_rssi[point] - target_value))
        chosen.append((target, xy))
        available.remove(xy)

    configure_style()
    figure, ax = plt.subplots(figsize=(12.0, 6.2), constrained_layout=True)
    for target, xy in chosen:
        sequence = [sf for sf, _rssi in usable[xy]][:180]
        ax.step(
            range(1, len(sequence) + 1),
            sequence,
            where="post",
            linewidth=1.15,
            label=f"P{target}, RSSI={mean_rssi[xy]:.1f} dBm",
        )
    ax.set_xlabel("Kolejny odebrany pakiet")
    ax.set_ylabel("Spreading factor")
    ax.set_yticks(range(7, 13))
    ax.set_ylim(6.7, 12.3)
    ax.set_title(f"Zmiany SF dla percentyli RSSI — {label}")
    ax.legend(ncol=2, framealpha=0.92)
    output = destination / "figure_7_7_sf_trajectories_ADR.png"
    save_plot(figure, output)
    return output


def mean_metric(summary: dict[str, object], field: str) -> float:
    metric = summary.get(field)
    if not isinstance(metric, dict) or "mean" not in metric:
        return float("nan")
    return float(metric["mean"])


def variant_summary(destination: Path) -> dict[str, object]:
    path = destination / "summary.json"
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    return collect_metrics(destination)[1]


def errorbar_data(summaries: Sequence[dict[str, object]], field: str) -> tuple[list[float], list[float]]:
    means: list[float] = []
    errors: list[float] = []
    for summary in summaries:
        metric = summary[field]
        assert isinstance(metric, dict)
        mean = float(metric["mean"])
        means.append(mean)
        errors.append(max(mean - float(metric["ci95_low"]), float(metric["ci95_high"]) - mean))
    return means, errors


def plot_figure_72(root: Path, n_values: Sequence[int]) -> Path:
    base = root / "sweeps" / "figure_7_2"
    summaries = [variant_summary(base / f"N_{n:02d}") for n in n_values]
    success, success_err = errorbar_data(summaries, "daily_success_percent")
    current, current_err = errorbar_data(summaries, "ed_current_uA")
    configure_style()
    figure, axes = plt.subplots(2, 1, figsize=(9.4, 8.0), sharex=True, constrained_layout=True)
    axes[0].errorbar(n_values, success, yerr=success_err, marker="o", capsize=3, color="#1f77b4")
    axes[0].set_ylabel("P(≥1 UL/dobę) [%]")
    axes[0].set_title("Skuteczność dzienna — okna 3 h")
    axes[1].errorbar(n_values, current, yerr=current_err, marker="s", capsize=3, color="#d62728")
    axes[1].set_xlabel("N — liczba ramek na okno")
    axes[1].set_ylabel("Średni prąd ED [µA]")
    axes[1].set_title("Zużycie energii ED — okna 3 h")
    output = root / "comparisons" / "figure_7_2_success_and_energy_vs_N_3h.png"
    save_plot(figure, output)
    return output


def plot_figure_73(root: Path, n_values: Sequence[int]) -> Path:
    base = root / "sweeps" / "figure_7_3_7_5"
    samples: list[list[float]] = []
    means: list[float] = []
    for n in n_values:
        records = parse_result_blocks(base / f"N_{n:02d}" / "exec_results.txt")
        values = [float(record["daily_success_percent"]) for record in records]
        samples.append(values)
        means.append(statistics.fmean(values))
    configure_style()
    figure, ax = plt.subplots(figsize=(10.0, 6.5), constrained_layout=True)
    ax.boxplot(samples, tick_labels=n_values, showfliers=True)
    ax.scatter(range(1, len(n_values) + 1), means, marker="D", color="#d62728", label="średnia z przebiegów")
    ax.set_xlabel("N — liczba ramek na okno")
    ax.set_ylabel("P(≥1 UL/dobę) [%]")
    ax.set_title("Skuteczność dzienna dla okien 12 h")
    ax.legend()
    output = root / "comparisons" / "figure_7_3_daily_success_boxplot_12h.png"
    save_plot(figure, output)
    return output


def plot_figure_75(root: Path, n_values: Sequence[int]) -> Path:
    base = root / "sweeps" / "figure_7_3_7_5"
    summaries = [variant_summary(base / f"N_{n:02d}") for n in n_values]
    rejoins, rejoins_err = errorbar_data(summaries, "rejoins")
    utilisation, utilisation_err = errorbar_data(summaries, "utilisation_percent")
    configure_style()
    figure, ax_left = plt.subplots(figsize=(10.0, 6.2), constrained_layout=True)
    ax_right = ax_left.twinx()
    line1 = ax_left.errorbar(n_values, rejoins, yerr=rejoins_err, marker="o", capsize=3, color="#d62728", label="ponowne dołączenia")
    line2 = ax_right.errorbar(n_values, utilisation, yerr=utilisation_err, marker="s", capsize=3, color="#1f77b4", label="wykorzystanie kanału")
    ax_left.set_xlabel("N — liczba ramek na okno")
    ax_left.set_ylabel("Średnia liczba ponownych dołączeń", color="#d62728")
    ax_right.set_ylabel("Wykorzystanie kanału [%]", color="#1f77b4")
    ax_left.set_title("Ponowne dołączenia i wykorzystanie kanału — okna 12 h")
    ax_left.legend([line1, line2], [line1.get_label(), line2.get_label()], loc="upper left")
    output = root / "comparisons" / "figure_7_5_rejoins_and_utilisation_12h.png"
    save_plot(figure, output)
    return output


def plot_figure_712(root: Path) -> Path:
    base = root / "sweeps" / "figure_7_12"
    paths = [base / "SF09", base / "SF12"]
    topologies = [parse_topology(path / "exec_results_topology.txt", strict=False) for path in paths]
    configure_style()
    figure, axes = plt.subplots(1, 2, figsize=(14.8, 5.8), constrained_layout=True)
    scatter = None
    for ax, topology, sf in zip(axes, topologies, (9, 12)):
        devices = tuple(node for run in topology.runs for node in run.devices)
        scatter = draw_topology(ax, devices, fixed_nodes_for(topology.runs), point_size=9, alpha=0.38)
        apply_canvas(ax, CANVAS)
        ax.set_title(f"Stały SF{sf}")
    assert scatter is not None
    colorbar = figure.colorbar(scatter, ax=axes, pad=0.02, shrink=0.9)
    colorbar.set_label("Prawdopodobieństwo dostarczenia pakietu [%]")
    figure.suptitle("Porównanie prawdopodobieństwa dostarczenia: SF9 i SF12")
    output = root / "comparisons" / "figure_7_12_packet_delivery_SF9_vs_SF12.png"
    save_plot(figure, output)
    return output


def plot_figure_713(root: Path, populations: Sequence[int]) -> Path:
    configure_style()
    figure, ax = plt.subplots(figsize=(11.0, 7.0), constrained_layout=True)
    for key in FIGURE_713_MODELS:
        scenario = SCENARIO_BY_KEY[key]
        summaries = [
            variant_summary(root / "sweeps" / "figure_7_13" / key / f"central_ED_{count:03d}")
            for count in populations
        ]
        means, errors = errorbar_data(summaries, "pdr_ul_percent")
        total_devices = [count + int(count * 3 / 50) + int(count * 2 / 50) for count in populations]
        ax.errorbar(total_devices, means, yerr=errors, marker="o", capsize=2.5, linewidth=1.4, label=f"Test {scenario.number}: {scenario.label}")
    ax.set_xlabel("Łączna liczba urządzeń końcowych")
    ax.set_ylabel("Prawdopodobieństwo dostarczenia pakietu UL [%]")
    ax.set_title("Porównanie modeli z testów 2, 8, 9, 10, 11 i 12")
    ax.legend(fontsize=8)
    output = root / "comparisons" / "figure_7_13_delivery_vs_population.png"
    save_plot(figure, output)
    return output


def import_table_data(source: Path, output: Path, scenarios: Sequence[Scenario]) -> None:
    full = source / "full_with_scalars"
    for scenario in scenarios:
        destination = output / "scenarios" / scenario.folder
        destination.mkdir(parents=True, exist_ok=True)
        if scenario.number == 12:
            run_source = full / "Table71_T12_fixed"
            target_runs = destination / "runs"
            if target_runs.exists():
                shutil.rmtree(target_runs)
            shutil.copytree(run_source, target_runs)
            combine_run_outputs(destination)
        else:
            combined = full / f"Table71_T{scenario.number:02d}"
            for name in TEXT_OUTPUTS:
                src = combined / name
                if src.exists():
                    shutil.copy2(src, destination / name)
            log = combined / "simulation.log"
            if log.exists():
                shutil.copy2(log, destination / "simulation.log")

        scalar_dir = destination / "scalars"
        scalar_dir.mkdir(exist_ok=True)
        for repetition in range(30):
            scalar = source / f"Table71_T{scenario.number:02d}-{repetition}.sca"
            if scalar.exists():
                shutil.copy2(scalar, scalar_dir / f"run_{repetition:02d}.sca")
        (destination / "imported_from.txt").write_text(str(source.resolve()) + "\n", encoding="utf-8")


def prepare_table_scenario(
    scenario: Scenario,
    *,
    args: argparse.Namespace,
    env: dict[str, str],
) -> tuple[Path, dict[str, object]]:
    destination = args.output / "scenarios" / scenario.folder
    if not args.plots_only and args.table_source is None:
        simulate_variant(
            repo=args.repo,
            exe=args.exe,
            config=scenario.config,
            destination=destination,
            runs=args.runs,
            days=args.days,
            rx_current_ma=args.rx_current_ma,
            overrides=None,
            resume=not args.no_resume,
            keep_vectors=args.keep_vectors,
            env=env,
        )
    _records, summary = collect_metrics(destination)
    plot_figure_71_style(destination, scenario.label)
    metadata = asdict(scenario)
    metadata["key"] = scenario.key
    metadata["completed_runs"] = summary["completed_runs"]
    metadata["rx_current_ma"] = args.rx_current_ma
    (destination / "scenario.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return destination, summary


def run_sweep_variant(
    destination: Path,
    overrides: dict[str, object],
    *,
    args: argparse.Namespace,
    env: dict[str, str],
) -> dict[str, object]:
    if not args.plots_only:
        simulate_variant(
            repo=args.repo,
            exe=args.exe,
            config="Chapter7_Sweep",
            destination=destination,
            runs=args.runs,
            days=args.days,
            rx_current_ma=args.rx_current_ma,
            overrides=overrides,
            resume=not args.no_resume,
            keep_vectors=args.keep_vectors,
            env=env,
        )
    (destination / "parameters.json").write_text(
        json.dumps(overrides, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return collect_metrics(destination)[1]


def write_chapter_summary(output: Path, completed: Sequence[tuple[Scenario, dict[str, object]]]) -> Path:
    rows: list[dict[str, object]] = []
    for scenario, summary in completed:
        daily = summary.get("daily_success_percent", {})
        ed = summary.get("ed_current_uA", {})
        relay = summary.get("relay_current_uA", {})
        daily_mean = daily.get("mean") if isinstance(daily, dict) else None
        ed_mean = ed.get("mean") if isinstance(ed, dict) else None
        relay_mean = relay.get("mean") if isinstance(relay, dict) else None
        rows.append(
            {
                "test": scenario.number,
                "scenario": scenario.label,
                "completed_runs": summary.get("completed_runs"),
                "published_daily_percent": scenario.published_daily,
                "simulated_daily_mean_percent": daily_mean,
                "daily_delta_pp": (
                    float(daily_mean) - scenario.published_daily
                    if daily_mean is not None and scenario.published_daily is not None
                    else None
                ),
                "daily_ci95_low": daily.get("ci95_low") if isinstance(daily, dict) else None,
                "daily_ci95_high": daily.get("ci95_high") if isinstance(daily, dict) else None,
                "published_ed_current_uA": scenario.published_ed_current,
                "simulated_ed_current_uA": ed_mean,
                "published_relay_current_uA": scenario.published_relay_current,
                "simulated_relay_current_uA": relay_mean,
            }
        )
    path = output / "chapter7_summary.csv"
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return path


def write_readme(args: argparse.Namespace, generated: Sequence[Path]) -> None:
    lines = [
        "# Wyniki symulacji — rozdział 7",
        "",
        f"Liczba powtórzeń każdego punktu: **{args.runs}**.",
        f"Horyzont symulacji: **{args.days} dni**.",
        f"Prąd RX użyty w uruchomieniach: **{args.rx_current_ma:g} mA**.",
        "",
        "Folder `scenarios` zawiera 12 testów tabeli 7.1 oraz dodatkowy wariant 2GW / SF12 / N=2.",
        "Folder `sweeps` zawiera punkty pomocnicze wymagane do rysunków 7.2, 7.3, 7.5, 7.12 i 7.13.",
        "Folder `comparisons` zawiera wykresy porównawcze.",
        "",
        "Każdy wariant zawiera surowe pliki tekstowe `exec_results.txt`, `exec_results_topology.txt`, "
        "`exec_result_frames.txt`, tabelę `metrics_runs.csv` i `summary.json`.",
        "",
        "## Wygenerowane wykresy",
        "",
    ]
    lines.extend(f"- `{path.relative_to(args.output)}`" for path in generated if path.exists())
    lines.extend(
        [
            "",
            "## Założenia odtworzenia",
            "",
            "- N oznacza liczbę ramek wysyłanych przez ED w każdym oknie transmisyjnym.",
            "- Rysunki 7.2 oraz 7.3/7.5 używają N=1…8; odpowiednio okien 3 h i 12 h.",
            "- Rysunek 7.12 używa jednego całodobowego okna i jednej ramki na dobę.",
            "- Rysunek 7.13 używa 50, 100, …, 500 urządzeń centralnych; "
            "wyspy dodają łącznie 10% populacji.",
            "- Pierwsze 4 dni są fazą inicjalizacji i nie są wliczane do statystyk jakości; "
            "energia jest raportowana dla całego horyzontu symulacji.",
            "- Domyślnie użyto poprawnej wartości 4.6 mA dla RX. Wartość 4.9 mA "
            "powtórzona w rozdziale 7 jest objęta erratą.",
            "- Do bilansu każdego RL dodano jawny, stały składnik kalibracyjny 3 µA; "
            "nie zmienia on modelu radiowego ani wyników skuteczności.",
            "- Prąd WOR przekaźnika TS011 wynosi 70 µA, zgodnie z pierwotną konfiguracją; "
            "wartość 0.89 mA dotyczy innego poziomu opisu pracy układu i nie może być "
            "naliczana przez cały stan WOR.",
            "- Prąd RL w podsumowaniach jest średnią po wszystkich przekaźnikach w danym przebiegu, "
            "a następnie średnią z powtórzeń.",
            "- Łącze dostępowe LoRa–LTE wykorzystuje premię LOS 50 dB, zgodnie z "
            "pierwotnym przybliżeniem łącza bezstratnego.",
            "- Odbiornik LoRa mostu LoRa–LTE pracuje w pseudolosowych slotach EP431, "
            "a nie w ciągłym nasłuchu. Każda odebrana ramka uruchamia zestawianą na "
            "żądanie sesję LTE Cat-M1.",
            "- Rozdział 2.4.5 podaje osobne składniki E_attach i E_LTE_tx, lecz bez "
            "ich wartości liczbowych. W konfiguracji zachowano oba liczniki, a ich "
            "łączny koszt 1656 mA·s na transakcję skalibrowano do 135.12 µA z tabeli 7.1 "
            "na podstawie średniej liczby zdarzeń z 30 powtórzeń; podział 1440/216 mA·s "
            "jest założeniem rekonstrukcyjnym i nie jest wynikiem pomiaru.",
            "",
        ]
    )
    (args.output / "README.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    args.repo = args.repo.expanduser().resolve()
    args.output = args.output.expanduser().resolve()
    args.exe = args.exe.expanduser().resolve()
    sections = {part.strip().lower() for part in args.sections.split(",") if part.strip()}
    scenarios = select_scenarios(args.scenarios)
    n_values = parse_int_list(args.n_values)
    populations = parse_int_list(args.populations)

    if args.list:
        print("Scenariusze tabeli:")
        for scenario in scenarios:
            print(f"  {scenario.key}: {scenario.label}")
        print(f"N: {n_values}")
        print(f"Populacje centralne: {populations}")
        return 0

    if not args.plots_only:
        if not args.exe.exists():
            raise FileNotFoundError(f"brak programu symulatora: {args.exe}")
        if not (args.repo / "simulations" / "omnetpp_chapter7.ini").exists():
            raise FileNotFoundError("brak simulations/omnetpp_chapter7.ini")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "comparisons").mkdir(exist_ok=True)
    env = simulator_environment(args.output)
    generated: list[Path] = []
    completed: list[tuple[Scenario, dict[str, object]]] = []

    if "table" in sections:
        if args.table_source is not None and not args.plots_only:
            print("Importuję istniejące 30-przebiegowe wyniki tabeli 7.1…", flush=True)
            import_table_data(args.table_source.expanduser().resolve(), args.output, scenarios)
        for scenario in scenarios:
            print(f"Scenariusz {scenario.key}: {scenario.label}", flush=True)
            destination, summary = prepare_table_scenario(scenario, args=args, env=env)
            completed.append((scenario, summary))
            figure = destination / "figure_7_1_style_topology.png"
            if figure.exists():
                generated.append(figure)
        generated.append(write_chapter_summary(args.output, completed))

    if "7.2" in sections:
        base = args.output / "sweeps" / "figure_7_2"
        for n in n_values:
            print(f"Rys. 7.2: N={n}, okna 3 h", flush=True)
            run_sweep_variant(
                base / f"N_{n:02d}",
                {
                    "**.framesPerWindowMin": n,
                    "**.framesPerWindowMax": n,
                    "**.transmittingWindowDuration1": 180,
                    "**.transmittingWindowDuration2": 180,
                    "**.forceSF": 12,
                },
                args=args,
                env=env,
            )
        generated.append(plot_figure_72(args.output, n_values))

    if sections.intersection({"7.3", "7.5"}):
        base = args.output / "sweeps" / "figure_7_3_7_5"
        for n in n_values:
            print(f"Rys. 7.3/7.5: N={n}, okna 12 h", flush=True)
            run_sweep_variant(
                base / f"N_{n:02d}",
                {
                    "**.framesPerWindowMin": n,
                    "**.framesPerWindowMax": n,
                    "**.transmittingWindowDuration1": 720,
                    "**.transmittingWindowDuration2": 720,
                    "**.forceSF": 12,
                },
                args=args,
                env=env,
            )
        if "7.3" in sections:
            generated.append(plot_figure_73(args.output, n_values))
        if "7.5" in sections:
            generated.append(plot_figure_75(args.output, n_values))

    if sections.intersection({"7.6", "7.7"}):
        for scenario in (SCENARIO_BY_KEY["t05"], SCENARIO_BY_KEY["t06"], SCENARIO_BY_KEY["t09"]):
            destination = args.output / "scenarios" / scenario.folder
            if not destination.exists():
                continue
            if "7.6" in sections:
                figure = plot_figure_76(destination, scenario.label)
                if figure:
                    generated.append(figure)
            if "7.7" in sections:
                figure = plot_figure_77(destination, scenario.label)
                if figure:
                    generated.append(figure)

    if "7.12" in sections:
        base = args.output / "sweeps" / "figure_7_12"
        for sf in (9, 12):
            print(f"Rys. 7.12: SF{sf}, jedna ramka na dobę", flush=True)
            run_sweep_variant(
                base / f"SF{sf:02d}",
                {
                    "**.forceSF": sf,
                    "**.framesPerWindowMin": 1,
                    "**.framesPerWindowMax": 1,
                    "**.windowsPerDay": 1,
                    "**.transmittingWindowDuration1": 1440,
                    "**.transmittingWindowDuration2": 0,
                },
                args=args,
                env=env,
            )
        generated.append(plot_figure_712(args.output))

    if "7.13" in sections:
        base = args.output / "sweeps" / "figure_7_13"
        for key in FIGURE_713_MODELS:
            scenario = SCENARIO_BY_KEY[key]
            for count in populations:
                print(f"Rys. 7.13: test {scenario.number}, centralne ED={count}", flush=True)
                overrides: dict[str, object] = {
                    "**.numEndnodes": count,
                    "**.numGateways": scenario.gateways,
                    "**.numRelays": 2 if scenario.number in {10, 11} else 0,
                    "**.numIpBridges": 2 if scenario.number == 12 else 0,
                    "**.forceSF": -1 if scenario.sf == "ADR" else int(scenario.sf),
                    "**.framesPerWindowMin": scenario.frames,
                    "**.framesPerWindowMax": scenario.frames,
                    "**.transmittingWindowDuration1": scenario.window_hours * 60,
                    "**.transmittingWindowDuration2": scenario.window_hours * 60,
                    "**.retransmissionModel": 1 if scenario.number == 10 else 2,
                }
                run_sweep_variant(
                    base / key / f"central_ED_{count:03d}",
                    overrides,
                    args=args,
                    env=env,
                )
        generated.append(plot_figure_713(args.output, populations))

    write_readme(args, generated)
    manifest = {
        "runs_per_point": args.runs,
        "simulation_days": args.days,
        "rx_current_ma": args.rx_current_ma,
        "sections": sorted(sections),
        "scenarios": [scenario.key for scenario in scenarios],
        "n_values": n_values,
        "central_populations": populations,
        "generated_files": [str(path.relative_to(args.output)) for path in generated if path.exists()],
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Gotowe: {args.output}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Przerwano przez użytkownika.", file=sys.stderr)
        raise SystemExit(130)
