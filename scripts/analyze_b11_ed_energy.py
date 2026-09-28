"""End-device average current from all ED scalars in 600 existing B11 runs."""
from pathlib import Path
import argparse
import re
import json
import hashlib
import numpy as np
from analyze_b11_energy import ROOT, COMPONENTS, DURATION_S, stats

KEYS = ("ep4312388", "ts011")
SCALAR = re.compile(r'^scalar (Chapter7Network\.endnode(?:1|2)?\[\d+\])\.radio "Energy consumption total ([^"]+)" (\S+)')
PARAM = re.compile(r'^par (Chapter7Network\.endnode(?:1|2)?\[\d+\])\.radio currentConsumption(\w+) (\S+)')
EXPECTED = dict(Tx=25.5, Rx=4.6, WOR=.89, Idle=.0253, BaseOffset=0.)


def main(output):
    raw, summary, sources = {}, {}, []
    worst_duration_error, ed_balances = 0., 0
    for key in KEYS:
        raw[key], summary[key] = {}, {}
        for load in range(55,551,55):
            observations = []
            for run in range(30):
                path = ROOT/"variants"/f"b11_comparison__{key}_{load*10//11}"/"runs"/f"run_{run:02d}"/"results.sca"
                charge, params = {}, {}
                with path.open(encoding="utf-8") as stream:
                    for line in stream:
                        m = SCALAR.match(line)
                        if m:
                            node,name,value = m.groups()
                            charge.setdefault(node,{})[name] = float(value)
                        m = PARAM.match(line)
                        if m:
                            node,name,value = m.groups()
                            params.setdefault(node,{})[name] = float(value)
                assert len(charge) == load, (path,len(charge))
                assert len(params) == load, path
                currents = []
                for node,q in charge.items():
                    assert params[node] == EXPECTED, (path,node,params[node])
                    assert all(np.isfinite(q[c]) and q[c] >= 0 for c in COMPONENTS)
                    assert q["LTE attach"] == q["LTE transmit"] == q["BaseOffset"] == 0
                    time = sum(q[c]/EXPECTED[c] for c in COMPONENTS if c != "BaseOffset")
                    worst_duration_error = max(worst_duration_error,abs(time-DURATION_S))
                    assert abs(time-DURATION_S) < .001,(path,node,time)
                    currents.append([q[c]/DURATION_S*1000 for c in COMPONENTS])
                observations.append(np.mean(currents,axis=0))
                sources.append(str(path))
                ed_balances += len(charge)
            a = np.asarray(observations)
            raw[key][load] = a.tolist()
            summary[key][load] = dict(total_uA=stats(a.sum(axis=1)),
                components_uA={c:stats(a[:,i]) for i,c in enumerate(COMPONENTS)})
    differences = {load:stats(np.asarray(raw["ts011"][load]).sum(axis=1)
                              -np.asarray(raw["ep4312388"][load]).sum(axis=1))
                   for load in range(55,551,55)}
    report = dict(summary=summary,raw_run_mean_components_uA=raw,paired_TS_minus_EP_uA=differences,
        unit="uA",raw_scalar_unit="mA*s (charge)",duration_s=DURATION_S,replications=30,
        definition="equally weighted average over ALL ED in each run, then equally weighted average over 30 runs; includes full 30 days and initialization, regardless of message delivery",
        ci_method="pointwise Student t, df=29; network run is the independent replication, not each ED",
        components=COMPONENTS,current_parameters_mA=EXPECTED,ed_balances_checked=ed_balances,
        max_state_duration_error_s=worst_duration_error,sources=sources,
        source_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(r"C:\Omnet\lora_relay\src\Radio.cc"),Path(r"C:\Omnet\lora_relay\src\Radio.ned")]})
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"means":{k:{n:v["total_uA"]["mean"] for n,v in s.items()} for k,s in summary.items()},
        "case385":{k:s[385] for k,s in summary.items()},"difference385":differences[385],
        "ed_balances_checked":ed_balances,"max_state_duration_error_s":worst_duration_error},ensure_ascii=False,indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--output",type=Path,default=ROOT/"B11_ed_energy_analysis.json")
    main(p.parse_args().output)
