"""Full-precision relay charge balance from existing B11 scalar files."""
from pathlib import Path
import argparse
import re
import json
import hashlib
import numpy as np
from scipy.stats import t

ROOT = Path(r"C:\Omnet\simulation_results\review_chapter1")
KEYS = ("ep4312388", "ts011", "ts011_unlimited")
COMPONENTS = ("Tx", "Rx", "WOR", "Idle", "BaseOffset")
DURATION_S = 30 * 86400
SCALAR = re.compile(r'^scalar Chapter7Network\.relay\[(\d+)\]\.radio "Energy consumption total ([^"]+)" (\S+)')
PARAM = re.compile(r'^par Chapter7Network\.relay\[(\d+)\]\.radio currentConsumption(\w+) (\S+)')
EXPECTED = dict(Tx=25.5, Rx=4.6, WOR=.07, Idle=.0056, BaseOffset=.003)


def stats(a):
    a = np.asarray(a)
    mean = a.mean()
    half = t.ppf(.975, len(a)-1)*a.std(ddof=1)/np.sqrt(len(a))
    return dict(mean=float(mean), ci95=[float(mean-half), float(mean+half)], n=len(a))


def main(output):
    data, summary, shares, sources = {}, {}, {}, []
    worst_duration_error = 0.
    for key in KEYS:
        data[key], summary[key], shares[key] = {}, {}, {}
        for load in range(55, 551, 55):
            observations = []
            for run in range(30):
                path = ROOT / "variants" / f"b11_comparison__{key}_{load*10//11}" / "runs" / f"run_{run:02d}" / "results.sca"
                charge, params = {}, {}
                with path.open(encoding="utf-8") as stream:
                    for line in stream:
                        match = SCALAR.match(line)
                        if match:
                            relay, name, value = match.groups()
                            charge.setdefault(int(relay), {})[name] = float(value)
                        match = PARAM.match(line)
                        if match:
                            relay, name, value = match.groups()
                            params.setdefault(int(relay), {})[name] = float(value)
                assert set(charge) == {0, 1}, path
                for relay, q in charge.items():
                    assert params[relay] == EXPECTED, (path, params)
                    assert all(q[k] >= 0 and np.isfinite(q[k]) for k in COMPONENTS)
                    assert q["LTE attach"] == q["LTE transmit"] == 0
                    # Activity states are mutually exclusive; BaseOffset is additive.
                    state_time = sum(q[k]/EXPECTED[k] for k in COMPONENTS if k != "BaseOffset")
                    worst_duration_error = max(worst_duration_error, abs(state_time-DURATION_S))
                    assert abs(state_time-DURATION_S) < .001, (path, state_time)
                    assert abs(q["BaseOffset"]/.003-DURATION_S) < .001
                # Two relays form one observation, not two independent replications.
                observations.append([[charge[r][c]/DURATION_S*1000 for c in COMPONENTS] for r in (0, 1)])
                sources.append(str(path))
            a = np.asarray(observations)  # run, relay, component (uA contributions)
            averaged = a.mean(axis=1)
            totals = averaged.sum(axis=1)
            data[key][load] = a.tolist()
            summary[key][load] = {"total_uA": stats(totals),
                "components_uA": {c: stats(averaged[:, i]) for i, c in enumerate(COMPONENTS)},
                "mean_max_relay_uA": stats(a.sum(axis=2).max(axis=1))}
            shares[key][load] = dict(zip(COMPONENTS, (100*averaged.mean(axis=0)/totals.mean()).tolist()))
    differences = {}
    for name, a, b in (("TS_minus_EP", "ts011", "ep4312388"),
                        ("TS0_minus_EP", "ts011_unlimited", "ep4312388"),
                        ("TS0_minus_TS", "ts011_unlimited", "ts011")):
        differences[name] = {load: stats(np.asarray(data[a][load]).mean(axis=1).sum(axis=1)
                                        - np.asarray(data[b][load]).mean(axis=1).sum(axis=1))
                            for load in range(55, 551, 55)}
    report = dict(duration_s=DURATION_S, replications=30, relays_per_replication=2,
        unit_raw="mA*s = charge, despite scalar Energy name",
        unit_analysis="uA", component_order=COMPONENTS, current_parameters_mA=EXPECTED,
        definition="mean of two relays in each run, then equally weighted mean of 30 runs; full 30 days including initialization",
        share_definition="ratio of pooled charge per component to total pooled charge, equal observation duration; equal energy fraction only for common constant supply voltage",
        ci_method="pointwise Student t, df=29, independent replication unit is the network run; paired comparisons by seed",
        max_state_duration_error_s=worst_duration_error,
        summary=summary, shares_percent=shares, paired_differences=differences, raw_components_uA=data, sources=sources,
        source_hashes={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(r"C:\Omnet\lora_relay\src\Radio.cc"), Path(r"C:\Omnet\lora_relay\src\Radio.ned")]})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"output":str(output), "means": {key:{n:v["total_uA"]["mean"] for n,v in vals.items()} for key, vals in summary.items()},
        "focus": {key:{n:summary[key][n] for n in (55,385)} for key in KEYS},
        "shares": {key:{n:shares[key][n] for n in (55,385)} for key in KEYS},
        "differences":{key:{n:vals[n] for n in (55,385)} for key,vals in differences.items()},
        "max_state_duration_error_s":worst_duration_error}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT/"B11_energy_analysis.json")
    main(parser.parse_args().output)
