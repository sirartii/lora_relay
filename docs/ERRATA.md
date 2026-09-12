# Errata do rozprawy doktorskiej

Dokument roboczy. Kolejne wykryte korekty będą dopisywane jako osobne,
numerowane pozycje.

## E-001 — prąd odbiornika w założeniach rozdziału 7

- Lokalizacja: rozdział 7, lista założeń modelu symulacyjnego.
- Jest: `IRX = 4,9 mA`.
- Powinno być: `IRX = 4,6 mA`.
- Uzasadnienie: poprawna wartość `4,6 mA` wynika z wcześniejszego wyliczenia
  przedstawionego w rozdziale 5. Wartość `4,9 mA` w rozdziale 7 jest omyłką.
- Zakres wpływu: korekta dotyczy wyników zużycia energii; nie zmienia modelu
  propagacji ani definicji metryk skuteczności transmisji.
