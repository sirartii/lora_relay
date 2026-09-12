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

Opcja `--lenient` pomija każdy niedokończony lub uszkodzony blok i kontynuuje
czytanie następnych przebiegów. Domyślnie parser działa rygorystycznie i zgłasza
taki problem.

## Pełny zestaw wyników rozdziału 7

`generate_chapter7_results.py` jest skryptem głównym. Wykonuje 30 niezależnych
powtórzeń każdego punktu, zachowuje tekstowe wyniki symulatora w osobnym folderze
dla każdego scenariusza, oblicza podsumowania CSV/JSON i tworzy odpowiedniki
rysunków 7.1, 7.2, 7.3, 7.5, 7.6, 7.7, 7.12 oraz 7.13.

```powershell
& $python C:\Omnet\lora_relay\scripts\generate_chapter7_results.py
```

Domyślny folder wyników to `C:\Omnet\simulation_results\chapter7`. Skrypt można
wznawiać — pomija kompletne przebiegi. Podgląd planu bez uruchamiania symulatora:

```powershell
& $python C:\Omnet\lora_relay\scripts\generate_chapter7_results.py --list
```

Przykład uruchomienia tylko dodatkowego scenariusza:

```powershell
& $python C:\Omnet\lora_relay\scripts\generate_chapter7_results.py `
  --sections table --scenarios t13
```

Konfiguracja repozytorium używa 4,6 mA dla odbioru radiowego. Aby zastosować
wartość 4,9 mA opisaną w rozdziale 7.1, dodaj `--rx-current-ma 4.9`.

Model energetyczny mostu LoRa–LTE, jego brakujące parametry źródłowe oraz
kalibrację do tabeli 7.1 opisano w `docs/LTE_RECONSTRUCTION.md`.
