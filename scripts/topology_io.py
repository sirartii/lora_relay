"""Parser for the text topology files produced by Keeper.cc.

The simulator appends many runs to one ``exec_results_topology.txt`` file.
Some batches, however, create one file per run.  This module accepts both
layouts and keeps the fixed nodes that apply to every parsed run.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
import re
from typing import Iterable, Sequence


_COUNT_RE = re.compile(r"^ED\s*=\s*(\d+)\s*$", re.IGNORECASE)
_DAYS_RE = re.compile(r"^Simulated days\s*=\s*(\d+)\s*$", re.IGNORECASE)
_FIXED_HEADER = "role,idx,x,y"
_ED_HEADER = "idx,x,y,avg_daily_success_ul[%]"
_TIMESTAMP_FORMAT = "%a %b %d %H:%M:%S %Y"


@dataclass(frozen=True, slots=True)
class FixedNode:
    role: str
    index: int
    x: float
    y: float


@dataclass(frozen=True, slots=True)
class EndDevice:
    index: int
    x: float
    y: float
    daily_success_percent: float

    @property
    def is_weak_island_node(self) -> bool:
        """Keeper writes the additional island nodes with index ``-1``."""

        return self.index < 0


@dataclass(frozen=True, slots=True)
class TopologyRun:
    run_number: int
    declared_end_devices: int
    simulated_days: int
    devices: tuple[EndDevice, ...]
    fixed_nodes: tuple[FixedNode, ...]
    written_at: datetime | None = None

    @property
    def mean_daily_success_percent(self) -> float:
        if not self.devices:
            return float("nan")
        return sum(node.daily_success_percent for node in self.devices) / len(self.devices)


@dataclass(frozen=True, slots=True)
class TopologyFile:
    path: Path
    runs: tuple[TopologyRun, ...]

    @property
    def devices(self) -> tuple[EndDevice, ...]:
        return tuple(node for run in self.runs for node in run.devices)

    @property
    def fixed_nodes(self) -> tuple[FixedNode, ...]:
        return unique_fixed_nodes(run.fixed_nodes for run in self.runs)


def _parse_timestamp(line: str) -> datetime | None:
    try:
        return datetime.strptime(line.strip(), _TIMESTAMP_FORMAT)
    except ValueError:
        return None


def _normalise_role(role: str) -> str:
    value = role.strip().upper().replace(" ", "")
    aliases = {
        "GATEWAY": "GW",
        "RL": "RELAY",
        "IPBRIDGE": "IPB",
        "IP-BRIDGE": "IPB",
    }
    return aliases.get(value, value)


def _parse_fixed_row(line: str, line_number: int) -> FixedNode:
    fields = [field.strip() for field in line.split(",")]
    if len(fields) != 4:
        raise ValueError(f"line {line_number}: expected 4 fixed-node fields, got {len(fields)}")
    role, index, x, y = fields
    return FixedNode(_normalise_role(role), int(index), float(x), float(y))


def _parse_device_row(line: str, line_number: int) -> EndDevice:
    fields = [field.strip() for field in line.split(",")]
    if len(fields) != 4:
        raise ValueError(f"line {line_number}: expected 4 ED fields, got {len(fields)}")
    index, x, y, success = fields
    return EndDevice(int(index), float(x), float(y), float(success))


def parse_topology(path: str | Path, *, strict: bool = True) -> TopologyFile:
    """Parse one topology export.

    Args:
        path: Path to ``exec_results_topology.txt``.
        strict: Validate the number of ED rows and the 0..100 success range.
    """

    source = Path(path).expanduser().resolve()
    lines = source.read_text(encoding="utf-8-sig", errors="replace").splitlines()
    runs: list[TopologyRun] = []
    active_fixed: tuple[FixedNode, ...] = ()
    pending_timestamp: datetime | None = None
    i = 0

    while i < len(lines):
        stripped = lines[i].strip()

        if stripped.startswith("# Fixed nodes"):
            i += 1
            fixed: list[FixedNode] = []
            while i < len(lines) and lines[i].strip().lower() != _FIXED_HEADER:
                timestamp = _parse_timestamp(lines[i].removeprefix("# Written on:").strip())
                if timestamp is not None:
                    pending_timestamp = timestamp
                i += 1
            if i >= len(lines):
                if strict:
                    raise ValueError("fixed-node section has no role,idx,x,y header")
                break
            i += 1
            while i < len(lines) and lines[i].strip():
                try:
                    fixed.append(_parse_fixed_row(lines[i], i + 1))
                except (TypeError, ValueError):
                    if strict:
                        raise
                    break
                i += 1
            active_fixed = tuple(fixed)
            continue

        timestamp = _parse_timestamp(stripped)
        if timestamp is not None:
            pending_timestamp = timestamp
            i += 1
            continue

        count_match = _COUNT_RE.match(stripped)
        if count_match:
            declared_count = int(count_match.group(1))
            simulated_days = 0
            i += 1

            if i < len(lines):
                days_match = _DAYS_RE.match(lines[i].strip())
                if days_match:
                    simulated_days = int(days_match.group(1))
                    i += 1

            while i < len(lines) and lines[i].strip().lower() != _ED_HEADER:
                if lines[i].strip() and _COUNT_RE.match(lines[i].strip()):
                    break
                i += 1

            if i >= len(lines) or lines[i].strip().lower() != _ED_HEADER:
                if strict:
                    raise ValueError(f"run {len(runs)} has no ED CSV header")
                continue

            i += 1
            devices: list[EndDevice] = []
            while i < len(lines) and lines[i].strip():
                try:
                    node = _parse_device_row(lines[i], i + 1)
                except (TypeError, ValueError):
                    if strict:
                        raise
                    break
                if strict and not 0.0 <= node.daily_success_percent <= 100.0:
                    raise ValueError(
                        f"line {i + 1}: daily success must be in [0, 100], "
                        f"got {node.daily_success_percent}"
                    )
                devices.append(node)
                i += 1

            if strict and len(devices) != declared_count:
                raise ValueError(
                    f"run {len(runs)} declares {declared_count} EDs but contains {len(devices)} rows"
                )

            runs.append(
                TopologyRun(
                    run_number=len(runs),
                    declared_end_devices=declared_count,
                    simulated_days=simulated_days,
                    devices=tuple(devices),
                    fixed_nodes=active_fixed,
                    written_at=pending_timestamp,
                )
            )
            pending_timestamp = None
            continue

        i += 1

    if strict and not runs:
        raise ValueError(f"no topology runs found in {source}")
    return TopologyFile(source, tuple(runs))


def unique_fixed_nodes(groups: Iterable[Sequence[FixedNode]]) -> tuple[FixedNode, ...]:
    """Return fixed nodes in stable order without duplicate coordinates/roles."""

    result: list[FixedNode] = []
    seen: set[tuple[str, int, float, float]] = set()
    for group in groups:
        for node in group:
            key = (node.role, node.index, node.x, node.y)
            if key not in seen:
                seen.add(key)
                result.append(node)
    return tuple(result)


def select_runs(topology: TopologyFile, selection: str) -> tuple[TopologyRun, ...]:
    """Select ``all``, one index, comma-separated indexes, or inclusive ranges."""

    value = selection.strip().lower()
    if value == "all":
        return topology.runs

    indexes: list[int] = []
    for token in value.split(","):
        token = token.strip()
        if not token:
            continue
        if "-" in token[1:]:
            start_text, end_text = token.split("-", 1)
            start, end = int(start_text), int(end_text)
            step = 1 if end >= start else -1
            indexes.extend(range(start, end + step, step))
        else:
            indexes.append(int(token))

    if not indexes:
        raise ValueError("empty run selection")
    invalid = [index for index in indexes if index < 0 or index >= len(topology.runs)]
    if invalid:
        raise IndexError(
            f"run indexes out of range: {invalid}; file contains 0..{len(topology.runs) - 1}"
        )
    return tuple(topology.runs[index] for index in indexes)
