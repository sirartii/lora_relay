# =============================================================================
# STANDARDOWY OPIS SKRYPTU: prepare_measured_multichannel.py
#
# DZIAŁANIE
# Przekształca oczyszczone współrzędne pomiarowe ED do lokalnych metrów
# i przygotowuje konfigurację MeasuredNetwork dla ośmiu kanałów.
# Nie zgaduje brakujących parametrów ani nie traktuje odbiorów jako liczby nadań.
#
# WEJŚCIE I PARAMETRY
# Wymagane: --devices (CSV), --repo (repozytorium), --output (katalog).
# Opcjonalne: --parameters (JSON). CSV UTF-8 z nagłówkiem device,longitude,
# latitude,modal_sf; >= 1 wiersz, unikalne device, skończona długość -180..180°,
# szerokość -90..90°, całkowite SF=7..12.
# JSON: gw_longitude -180..180°, gw_latitude -90..90°, tx_power_dbm -20..30 dBm;
# payload_bytes 1..242 B, frames_per_window i ack_every 1..100000,
# windows_per_day 1..2, window_start1_minutes/window_start2_minutes 0..1439,
# window_duration1_minutes/window_duration2_minutes 1..1440.
# Ostatnie osiem parametrów (po tx_power_dbm) to liczby całkowite; współrzędne
# i moc mogą być rzeczywiste. Początek+czas każdego okna <= 1440 min.
# Brakujące klucze lub null pozostają niewypełnione; nieznane klucze są błędem.
#
# PRZYGOTOWANIE DANYCH Z SYMULATORA
# Przed tym skryptem NIE uruchamia się symulatora: wejściem są oczyszczone
# dane terenowe i parametry wdrożenia. Utwórz devices.csv i parameters.json
# zgodnie z zakresami powyżej; przykład plików jest w scripts/README.md.
# Po przygotowaniu uzupełnij wszystkie null/ask, zbuduj symulator release,
# uruchom wygenerowany piekary_uniform.ini, konfigurację PiekaryUniform,
# powtórzenia -r 0..29 osobno (po 98 dni). Dla każdego użyj osobnego CWD,
# --result-dir i --output-scalar-file. Pełne polecenie podano w README.
#
# URUCHOMIENIE
# python scripts/prepare_measured_multichannel.py --devices tmp/devices.csv --parameters tmp/parameters.json --repo . --output tmp/measured
#
# PRZYKŁADOWY WYNIK
# piekary_uniform.ini, required_parameters.json, topology_local.csv,
# scenario_manifest.json w --output; stdout JSON z devices i missing_parameters.
# status: DRAFT_MISSING_PARAMETERS lub READY_FOR_ASSUMPTION_REVIEW.
# Brak pliku PNG; wynik służy jako wejście symulatora.
#
# UWAGI I ZALEŻNOŚCI
# Python >= 3.10, biblioteka standardowa. Skrypt nie uruchamia symulacji.
# Kanały DATA są wybierane równomiernie (1/8), Join używa pierwszych trzech (1/3).
# Współrzędne GW muszą pochodzić z danych; nawet dla jednego okna JSON wymaga
# parametrów obu okien, aby nie pozostawić ask w konfiguracji.
# =============================================================================

"""Prepare a measured topology without inventing missing deployment parameters.

Input devices.csv is the cleaned, local-only audit table. No observed receive
counts are used as transmit counts or as weights of channel selection.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

FREQUENCIES = [868100000, 868300000, 868500000, 867100000,
               867300000, 867500000, 867700000, 867900000]
REQUIRED = ["gw_longitude", "gw_latitude", "payload_bytes", "tx_power_dbm",
            "frames_per_window", "ack_every", "windows_per_day",
            "window_start1_minutes", "window_duration1_minutes",
            "window_start2_minutes", "window_duration2_minutes"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--devices", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--parameters", type=Path)
    args = parser.parse_args()
    with args.devices.open(encoding="utf-8", newline="") as stream:
        devices = list(csv.DictReader(stream))
    if not devices or len({r["device"] for r in devices}) != len(devices):
        raise ValueError("Expected one cleaned coordinate per unique device")
    for row in devices:
        for field, bound in [("longitude", 180), ("latitude", 90)]:
            number = float(row[field])
            if not math.isfinite(number) or not -bound <= number <= bound:
                raise ValueError(f"Invalid {field} for {row['device']}")
        if int(row["modal_sf"]) not in range(7, 13):
            raise ValueError("Invalid SF")
    parameters = {key: None for key in REQUIRED}
    if args.parameters:
        supplied = json.loads(args.parameters.read_text(encoding="utf-8"))
        unknown = supplied.keys() - parameters.keys()
        if unknown:
            raise ValueError(f"Unknown parameters: {unknown}")
        parameters.update(supplied)
    missing = [key for key in REQUIRED if parameters[key] is None]
    for key, value in parameters.items():
        if value is not None and (isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value)):
            raise ValueError(f"Invalid numeric parameter: {key}")
    bounds = {"gw_longitude": (-180, 180), "gw_latitude": (-90, 90),
              "payload_bytes": (1, 242), "tx_power_dbm": (-20, 30),
              "frames_per_window": (1, 100000), "ack_every": (1, 100000), "windows_per_day": (1, 2),
              "window_start1_minutes": (0, 1439), "window_start2_minutes": (0, 1439),
              "window_duration1_minutes": (1, 1440), "window_duration2_minutes": (1, 1440)}
    for key, (low, high) in bounds.items():
        value = parameters[key]
        if value is not None and not low <= value <= high:
            raise ValueError(f"Out-of-range {key}")
        if value is not None and key not in ("gw_longitude", "gw_latitude", "tx_power_dbm") and int(value) != value:
            raise ValueError(f"Expected integer {key}")
    for n in (1, 2):
        start, duration = parameters[f"window_start{n}_minutes"], parameters[f"window_duration{n}_minutes"]
        if start is not None and duration is not None and start + duration > 1440:
            raise ValueError("Windows must fit within a calendar day")
    # Local equirectangular approximation, NOT an inferred GW position.
    lon0 = sum(float(r["longitude"]) for r in devices) / len(devices)
    lat0 = sum(float(r["latitude"]) for r in devices) / len(devices)
    radius = 6371008.8

    def xy(lon, lat):
        return (radius * math.radians(lon - lon0) * math.cos(math.radians(lat0)),
                radius * math.radians(lat - lat0))

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    lines = [f'include {(args.repo.resolve() / "simulations/omnetpp_chapter7.ini").as_posix()}',
             "", "[Config PiekaryUniform]", "network = opp_lora_relay_official.simulations.MeasuredNetwork",
             "repeat = 30", "sim-time-limit = 98d", "warmup-period = 0s",
             '# DRAFT if any value is "ask". Batch run must not silently substitute parameters.',
             f"**.numEndnodes = {len(devices)}", "**.lorawanMultichannel = true",
             "**.exportGatewayReceives = true", "**.exportFrameRecords = true"]
    if parameters["gw_longitude"] is not None and parameters["gw_latitude"] is not None:
        gwx, gwy = xy(parameters["gw_longitude"], parameters["gw_latitude"])
        lines += [f"*.gwX = {gwx:.6f}", f"*.gwY = {gwy:.6f}"]
    else:
        lines += ["*.gwX = ask", "*.gwY = ask"]
    mapping = {"applicationPayloadLength": "payload_bytes", "txPower": "tx_power_dbm",
               "framesPerWindowMin": "frames_per_window", "framesPerWindowMax": "frames_per_window",
               "ackEvery": "ack_every", "windowsPerDay": "windows_per_day",
               "transmittingWindowStart1": "window_start1_minutes", "transmittingWindowStart2": "window_start2_minutes",
               "transmittingWindowDuration1": "window_duration1_minutes", "transmittingWindowDuration2": "window_duration2_minutes"}
    for ned, key in mapping.items():
        value = parameters[key]
        lines.append(f"**.{ned} = {value if value is not None else 'ask'}")
    positions = []
    for i, device in enumerate(devices):
        x, y = xy(float(device["longitude"]), float(device["latitude"]))
        lines += [f"*.endnode[{i}].xpos = {x:.6f}", f"*.endnode[{i}].ypos = {y:.6f}",
                  f"*.endnode[{i}].**.forceSF = {int(device['modal_sf'])}"]
        positions.append(dict(index=i, device=device["device"], x_m=x, y_m=y,
                              longitude=device["longitude"], latitude=device["latitude"], sf=int(device["modal_sf"])))
    # Root result paths are absolute. Each repetition must run in its own CWD
    # because legacy Keeper and the gateway CSV trace write there.
    lines += ["", "# Run each repetition in its own directory.",
              "# Use --result-dir=<that directory> --output-scalar-file=<absolute file>."]
    (output / "piekary_uniform.ini").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (output / "required_parameters.json").write_text(json.dumps(parameters, indent=2), encoding="utf-8")
    with (output / "topology_local.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(positions[0]))
        writer.writeheader()
        writer.writerows(positions)
    manifest = dict(status="DRAFT_MISSING_PARAMETERS" if missing else "READY_FOR_ASSUMPTION_REVIEW",
                    missing_parameters=missing, devices=len(devices),
                    source_sha256=hashlib.sha256(args.devices.read_bytes()).hexdigest(),
                    projection=dict(method="local equirectangular", origin_longitude=lon0, origin_latitude=lat0),
                    channel_ids=list(range(1, 9)), frequency_hz=FREQUENCIES,
                    data_channel_probabilities=[0.125] * 8,
                    join_channel_probabilities=[1 / 3] * 3 + [0] * 5,
                    channel_policy="uniform, as explicitly requested 2026-09-28; receive counts do not set TX weights",
                    assumptions=["One GW, no relay. GW coordinates must be supplied, not inferred from ED centroid.",
                                 "Positions remain fixed across repetitions. RNG seed equals repetition index.",
                                 "Fixed modal SF per ED is an approximation, not replay of measured ADR dynamics.",
                                 "All ED start unjoined. Operational logs may begin with existing sessions.",
                                 "Simulator uses BW125 and its existing PHY coding/airtime model. Verify against deployment.",
                                 "Traffic windows, application payload, ACK policy and TX power must be confirmed separately.",
                                 "98 days describe the export span. Per-device installation/active dates are not replayed.",
                                 "No synthetic per-channel interference bias is inferred from receive counts.",
                                 "This preparation is not yet a validation result or a replay of all physical transmissions."])
    (output / "scenario_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps({"status": manifest["status"], "devices": len(devices), "missing_parameters": missing}, indent=2))


if __name__ == "__main__":
    main()
