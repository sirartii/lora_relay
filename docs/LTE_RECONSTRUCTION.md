# Rekonstrukcja modelu energetycznego LoRa–LTE

## Podstawa w rozprawie

Rozdział 2.4.5 rozdziela energię mostu LoRa–LTE na energię stanów radia
`idle`, `rx`, `tx` oraz dwa składniki zdarzeniowe:

`N_attach * E_attach + N_LTE_tx * E_LTE_tx`.

Rozdział 7 opisuje LTE Cat-M1 zestawiane na żądanie przed przewidywanym
odbiorem LoRa, pseudolosowe sloty odbiorcze EP431 oraz bezstratne łącze TCP/IP.
Rozprawa nie podaje jednak wartości liczbowych `E_attach` ani `E_LTE_tx`.
Jedynym liczbowym punktem odniesienia jest średni prąd `135,12 µA` w tabeli
7.1 dla testu 12.

## Implementacja

- Most IP subskrybuje ten sam sygnał budzenia co retransmiter EP431 i poza
  zaplanowanym slotem utrzymuje radio LoRa w stanie wyłączonym.
- Ramkę odbiera i przekazuje wyłącznie najbliższy most, co zapobiega
  dublowaniu pakietów przez dwa mosty.
- Każdy poprawnie odebrany uplink wymagający retransmisji powoduje jedno
  zdarzenie `attach` i jedno zdarzenie transmisji LTE.
- Energia obu zdarzeń jest rejestrowana w osobnych skalarach razem z ich
  licznikami i wchodzi do całkowitego bilansu mostu.
- Łącze dostępowe zachowuje premię LOS `+50 dB`, będącą przyjętym wcześniej
  odpowiednikiem połączenia bezstratnego.

## Kalibracja brakujących parametrów

Ponieważ rozprawa identyfikuje tylko sumę kosztów, przyjęto:

- `E_attach = 1440 mA·s` (`0,40 mAh`),
- `E_LTE_tx = 216 mA·s` (`0,06 mAh`),
- razem `1656 mA·s` (`0,46 mAh`) na transakcję.

Podział między zestawienie i transmisję jest założeniem rekonstrukcyjnym, a
nie wynikiem pomiaru. Łączną wartość skalibrowano na podstawie średniej liczby
zdarzeń w 30 powtórzeniach testu 12.

## Weryfikacja 30 × 30 dni

- średni prąd mostu: `135,29 µA`,
- wartość tabeli 7.1: `135,12 µA`,
- różnica: `+0,17 µA` (`+0,13%`),
- średnia skuteczność dobowa: `99,951%`,
- wartość tabeli 7.1: `99,49%`,
- różnica skuteczności: `+0,461 p.p.`.

Wynik energetyczny jest zgodny z punktem kalibracyjnym. Różnica skuteczności
pozostaje do wyjaśnienia przed uruchomieniem następnych scenariuszy.
