"""Build and exercise the real Radio implementation, with isolated output files."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess


def scalars(path):
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("scalar "):
            _, module, metric, value = shlex.split(line)
            result[module, metric] = float(value)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--omnet", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    args = parser.parse_args()
    repo, omnet, output = args.repo.resolve(), args.omnet.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    clang = omnet / "tools/win32.x86_64/clang64/bin"
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(map(str, [omnet / "bin", clang, omnet / "tools/win32.x86_64/usr/bin"])) + os.pathsep + env["PATH"]

    def run(cmd, directory, log):
        if "-u" in cmd:
            cmd = [*cmd, "--result-dir=" + directory.as_posix(),
                   "--output-scalar-file=" + (directory / "result.sca").as_posix()]
        completed = subprocess.run(list(map(str, cmd)), cwd=directory, env=env,
                                   capture_output=True, text=True, errors="replace")
        (directory / log).write_text(completed.stdout + completed.stderr, encoding="utf-8")
        if completed.returncode:
            raise RuntimeError(f"Failed ({completed.returncode}): {cmd}\n{completed.stdout[-2000:]}\n{completed.stderr[-2000:]}")
        return completed

    compiler = clang / "clang++.exe"
    unit = output / "test_gateway_reception.exe"
    run([compiler, "-std=c++17", repo / "scripts/tests/test_gateway_reception.cc", "-o", unit], output, "unit_build.log")
    run([unit], output, "unit.log")
    integration = output / "test_multichannel_radio.exe"
    run([compiler, "-std=c++17", "-D__GXX_TYPEINFO_EQUALITY_INLINE=0", "-DOMNETPPLIBS_IMPORT",
         "-I" + str(repo / "src"), "-I" + str(omnet / "include"),
         repo / "scripts/tests/MultichannelRadioTest.cc", *sorted((repo / "out/clang-release/src").glob("*.o")),
         "-L" + str(omnet / "lib"), "-L" + str(omnet / "tools/win32.x86_64/clang64/lib"),
         "-loppmain", "-Wl,-u,_cmdenv_lib", "-loppcmdenv", "-loppenvir", "-loppsim", "-o", integration],
        output, "integration_build.log")
    ini = output / "tests.ini"
    ini.write_text(f'include {str(repo / "simulations/omnetpp_chapter7.ini").replace(chr(92), "/")}\n'
                   '[Config PHY]\nnetwork = MultichannelTest\nsim-time-limit = 12s\n'
                   'warmup-period = 0s\nrepeat = 1\n**.numEndnodes = 0\n'
                   '**.lorawanMultichannel = true\n**.exportGatewayReceives = true\n**.pathlosStd = 0\n'
                   '[Config Uniform]\nrepeat = 1\nsim-time-limit = 30d\nwarmup-period = 0s\n'
                   '**.lorawanMultichannel = true\n**.numEndnodes = 100\n'
                   '**.extranodex1 = 0\n**.extranodex2 = 0\n**.R = 50\n'
                   '**.pathlosStd = 0\n**.framesPerWindowMin = 4\n**.framesPerWindowMax = 4\n', encoding="utf-8")
    ned = ";".join(str(repo / x) for x in ["src", "simulations", "scripts/tests"])
    common = ["-u", "Cmdenv", "-n", ned, "-f", ini, "-r", "0", "--cmdenv-express-mode=true", "--cmdenv-performance-display=false"]
    report = {"unit": "passed", "integration": [], "uniform_join_counts": [], "uniform_data_counts": []}
    for scenario in range(7):
        folder = output / f"phy_{scenario}"
        folder.mkdir(exist_ok=True)
        run([integration, *common, "-c", "PHY", f"--**.testScenario={scenario}"], folder, "run.log")
        values = scalars(folder / "result.sca")
        assert any(metric == "Multichannel integration passed" and value == 1 for (_, metric), value in values.items())
        report["integration"].append({"scenario": scenario, "status": "passed"})
    executable = repo / "out/clang-release/src/opp-lora-relay-official.exe"
    folder = output / "legacy_after"
    folder.mkdir(exist_ok=True)
    run([executable, *common, "-c", "Chapter7_T02", "--sim-time-limit=1d", "--warmup-period=0s"], folder, "run.log")
    before, after = scalars(args.baseline), scalars(folder / "result.sca")
    assert before.keys() == after.keys(), "Legacy scalar keys changed"
    changes = {str(k): [before[k], after[k]] for k in before if before[k] != after[k] and not
               (str(before[k]) == str(after[k]) == "nan")}
    assert not changes, f"Legacy results changed: {changes}"
    report["legacy"] = {"status": "identical", "compared_scalars": len(before)}
    folder = output / "uniform"
    folder.mkdir(exist_ok=True)
    # Test-only modules are not instantiated in this production scenario.
    run([executable, *common, "-c", "Uniform"], folder, "run.log")
    values = scalars(folder / "result.sca")
    for label, field in [("Join", "uniform_join_counts"), ("data", "uniform_data_counts")]:
        for channel in range(1, 9):
            report[field].append(int(sum(v for (_, metric), v in values.items()
                                        if metric == f"Direct {label} TX channel {channel}")))
    joins, data = report["uniform_join_counts"], report["uniform_data_counts"]
    assert all(joins[:3]) and not any(joins[3:]), joins
    total = sum(data)
    assert total > 10000 and all(0.10 < n / total < 0.15 for n in data), data
    # Every finished air arrival must have exactly one PHY outcome.
    offered = sum(v for (_, metric), v in values.items() if metric.startswith("GW channel ") and metric.endswith(" offered"))
    completed = sum(v for (_, metric), v in values.items() if metric.startswith("GW channel ") and not metric.endswith(" offered"))
    pending = sum(v for (_, metric), v in values.items() if metric == "GW unfinished on-air signals")
    assert offered == completed + pending, (offered, completed, pending)
    report["gateway_accounting"] = {"offered": offered, "completed": completed, "pending": pending}
    (output / "test_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
