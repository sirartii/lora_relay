# Skrypty wizualizacji topologii LoRa

Zestaw odtwarza wykresy topologii na podstawie tekstowego pliku
`exec_results_topology.txt` generowanego przez `Keeper.cc`. Nie używa plików
`.anf`, `.sca` ani `.vec`.

Parser obsługuje:

- jeden lub wiele przebiegów dopisanych do tego samego pliku,
- 50 zwykłych ED i dodatkowe węzły wysp oznaczone indeksem `-1`,
- stałe węzły `GW`, `RELAY`/`RL` i `IPB`/`IPBRIDGE`,
- pliki tworzone osobno dla każdego uruchomienia,
- kontrolę liczby ED oraz zakresu skuteczności 0–100%.

## Python i zależności

Środowisko dołączone do używanej instalacji OMNeT++ ma już `matplotlib`:

```powershell
$python = 'C:\omnetpp-6.4.0\tools\win32.x86_64\clang64\bin\python.exe'
```

Jeżeli używasz innej instalacji Pythona, zainstaluj zależności:

```powershell
python -m pip install -r C:\Omnet\lora_relay\scripts\requirements.txt
```

## Skumulowana topologia z 30 przebiegów

```powershell
& $python C:\Omnet\lora_relay\scripts\plot_topology.py `
  C:\Omnet\lora_relay\simulations\exec_results_topology.txt `
  --runs all --canvas 0 1400 0 900 `
  -o C:\Omnet\lora_relay\simulations\topology_all.png
```

## Jeden przebieg

```powershell
& $python C:\Omnet\lora_relay\scripts\plot_topology.py `
  C:\Omnet\lora_relay\simulations\exec_results_topology.txt `
  --runs 0 -o C:\Omnet\lora_relay\simulations\topology_run_01.png
```

## Siatka wybranych przebiegów

```powershell
& $python C:\Omnet\lora_relay\scripts\plot_topology_grid.py `
  C:\Omnet\lora_relay\simulations\exec_results_topology.txt `
  --runs 0-5 --columns 3 `
  -o C:\Omnet\lora_relay\simulations\topology_runs.png
```

## Porównanie scenariuszy

```powershell
& $python C:\Omnet\lora_relay\scripts\plot_topology_compare.py `
  "1GW=C:\wyniki\1GW\exec_results_topology.txt" `
  "2GW=C:\wyniki\2GW\exec_results_topology.txt" `
  "EP431=C:\wyniki\EP431\exec_results_topology.txt" `
  -o C:\wyniki\topology_comparison.png
```

## Renderowanie całego katalogu wyników

```powershell
& $python C:\Omnet\lora_relay\scripts\plot_topology_batch.py `
  C:\wyniki -o C:\wyniki\topology_plots --lenient
```

Opcja `--lenient` pozwala pominąć niedokończony blok na końcu pliku po
przerwanym uruchomieniu. Domyślnie parser działa rygorystycznie i zgłasza taki
problem.
