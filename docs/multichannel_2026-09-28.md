# Model wielokanałowy, 28.09.2026

Punkty kontrolne sprzed zmiany: symulator `0ef86da`, rozprawa `5a468cd`.
Oba zapisano na zdalnych repozytoriach przed modyfikacją modelu.
Rozprawa: gałąź `checkpoint-before-multichannel-2026-09-28`, bez zmiany zdalnego `main`.

## Przyjęta polityka kanałów

Zgodnie z decyzją użytkownika z 28.09.2026 losowanie jest równomierne.
Udziały odebranych ramek z instalacji nie są wagami generatora transmisji.
Nierówność odbiorów może wynikać ze strat i zakłóceń, lecz eksport odbiorów
nie pozwala jednoznacznie rozdzielić tych przyczyn.

| ID w symulatorze | MHz | Join-Request | Dane po Join |
|---|---:|---:|---:|
| 1 | 868,1 | 1/3 | 1/8 |
| 2 | 868,3 | 1/3 | 1/8 |
| 3 | 868,5 | 1/3 | 1/8 |
| 4 | 867,1 | 0 | 1/8 |
| 5 | 867,3 | 0 | 1/8 |
| 6 | 867,5 | 0 | 1/8 |
| 7 | 867,7 | 0 | 1/8 |
| 8 | 867,9 | 0 | 1/8 |

Wszystkie osiem częstotliwości występuje w dostępnym eksporcie.
Trzy podstawowe częstotliwości Join są zgodne z tabelą EU863-870
w [LoRaWAN Regional Parameters RP002-1.0.3](https://lora-alliance.org/wp-content/uploads/2021/05/RP002-1.0.3-FINAL-1.pdf).
Model nie odtwarza wymiany CFList i komend zarządzania maską kanałów.
Udostępnienie ośmiu kanałów po udanym Join jest założeniem scenariusza.

## Implementacja i granice modelu

Włączenie: `**.lorawanMultichannel = true`. Wartość domyślna to `false`.
Tryb historyczny nie wykonuje dodatkowych losowań kanałów i zachowuje dotychczasową ścieżkę odbioru.
ED losuje kanał osobno dla każdej bezpośredniej próby nadania.
LNS zachowuje kanał uplinku w odpowiedzi RX1, zgodnie z istniejącą logiką.

GW może jednocześnie odebrać ramki na różnych kanałach. Rozpoczęcie nadawania
przerywa trwające odbiory na wszystkich kanałach. Ramka rozpoczynająca się
podczas TX nie może zostać przejęta w połowie po zakończeniu nadawania GW.
Blokada półdupleksowa ma oddzielne liczniki od kolizji radiowych.

Na wspólnej częstotliwości pozostawiono przybliżenie przechwycenia przy różnicy
mocy przekraczającej 6 dB. Utracone sygnały nadal zakłócają kolejne ramki aż do
końca swego czasu nadawania. Sygnały z różnych kanałów nie interferują.
Nie dodano modelu selektywności kanałów sąsiednich, macierzy zakłóceń pomiędzy SF
ani szczegółowego przydziału demodulatorów koncentratora. Próg przechwycenia
jest porównywany parami, bez sumowania mocy wielu interferentów.
Nowa ścieżka kończy ocenę odbioru dopiero na końcu ramki.

ED i RL nadal mają pojedynczy tor radiowy. Kanały WOR 10/11 pozostają oddzielne.
Losowanie ośmiu kanałów dotyczy bezpośrednich transmisji ED do GW. Plan ED–RL
oraz kanał przekazywania przez RL nie zostały rozszerzone w ramach tego testu
instalacji bez retransmiterów. Istniejące przybliżenia RX2, ADR, mocy, propagacji,
airtime i ruchu aplikacyjnego wymagają osobnej weryfikacji przed oceną zgodności
z konkretnym urządzeniem i instalacją.

Diagnostyka GW zapisuje liczbę rozpoczętych i zakończonych prób dla każdego
kanału, podział strat oraz liczbę sygnałów niedokończonych na końcu symulacji.
Opcja `**.exportGatewayReceives = true` zapisuje `gw_rx_<id>.csv` z czasem,
kanałem, SF, RSSI oraz końcowym wynikiem każdej próby odbioru.
W każdym powtórzeniu należy użyć odrębnego katalogu roboczego, ponieważ
eksporty Keeper i ślad GW mają stałe nazwy plików. Ścieżki wyników `.sca`
należy podać jawnie przez `--result-dir` i `--output-scalar-file`.

## Weryfikacja

`scripts/tests/test_gateway_reception.cc` sprawdza osiem niezależnych kanałów,
kolizje, oba kierunki przechwycenia, granicę 6 dB, stykające się ramki,
przerwanie RX przez TX, blokadę nowego RX, interferujące końcówki oraz czułość.
`scripts/tests/MultichannelRadioTest.cc` powtarza siedem przypadków na rzeczywistej
implementacji Radio w OMNeT++. Moduły testowe nie wchodzą do standardowego pliku wykonywalnego.
`scripts/tests/run_multichannel_tests.py` buduje te testy, porównuje wynik
historyczny i wykonuje 30-dniowy syntetyczny test rozkładu kanałów.

Wyniki testu z 28.09.2026: siedem przypadków integracyjnych poprawnych,
1561 identycznych skalarów w jednodniowym teście regresji `Chapter7_T02`,
23 897 nadań danych na ośmiu kanałach. Jest to kontrola implementacji,
nie walidacja na danych instalacji ani dowód zgodności wszystkich starych scenariuszy.

## Przygotowanie topologii Piekar

`scripts/prepare_measured_multichannel.py` przyjmuje lokalny plik `devices.csv`
i tworzy deterministyczne współrzędne 675 ED, konfigurację NED/INI oraz manifest.
Jedno ED z niejednoznaczną lokalizacją wyłączono w audycie danych. Oryginał
eksportu pozostaje niezmieniony. Surowych danych i współrzędnych nie dołącza się
do publicznego repozytorium.

Współrzędne przeliczane są do lokalnego układu equirectangular.
Początek układu wyznacza średnia współrzędnych ED. Nie oznacza on pozycji GW.
Bez potwierdzonej lokalizacji GW `7276ff000b031174` oraz parametrów ruchu plik
pozostaje szkicem z polami `ask`. Nie należy uruchamiać go, przyjmując zastępcze
dane jako parametry rzeczywistej instalacji.

Modalny SF na ED, start wszystkich ED od Join i stała populacja przez 98 dni
są jawnymi przybliżeniami generatora. Nie odtworzono jeszcze historii ADR,
czasów instalacji i wszystkich faktycznych nadań. Odebrane ramki nie wyznaczają
całego ruchu oferowanego. Znaczniki czasu eksportu mają dokładność minutową.
Na tym etapie nie sformułowano wyniku walidacji modelu na obiekcie.

## Uzupełnienie po analizie archiwum Piekary

Powyższy szkic dotyczył pierwszego eksportu. W archiwum
`analiza_Piekary_2020.zip` znaleziono dokładniejsze znaczniki czasu, z sekundami,
oraz mapę z oznaczeniem GW. Z mapy odczytano przybliżoną pozycję
50,3430° N, 19,0083° E. Nie znaleziono oryginalnego zapisu współrzędnych GW.
Po ograniczeniu poprzednio oczyszczonej populacji do promienia 1 km pozostało
638 ED. Przebadano 97 pełnych dób od południa 9 stycznia do południa
15 kwietnia 2020 r. Takie granice zachowują nocny cykl raportowania.

Opcjonalne `**.startJoined = true` rozpoczyna symulację z aktywnymi sesjami.
Każde ED otrzymuje niezależnie losowaną fazę licznika żądań ACK z zakresu
`0..ackEvery-1`. Pozwala to odtworzyć stan działającej instalacji bez narzucania
wspólnej fazy potwierdzeń wszystkim urządzeniom. Nie odtwarza historii
rzeczywistych resetów i ponownych Join. Domyślne `false` zachowuje wcześniejszą
procedurę Join i nie dodaje losowania fazy do historycznego strumienia RNG.

Serię Piekary zapisano lokalnie w katalogu
`C:/Omnet/simulation/_results/2026-09-28_51_Piekary_1km_porownanie`.
Wykonano 30 powtórzeń wariantu podstawowego oraz po 30 powtórzeń czterech
wariantów mocy i częstości ACK. Położenia ED były stałe. Model propagacji
i progi odbioru pozostawiono bez dopasowania do skuteczności pomiarowej.
Parametry 51 B, 14 dBm i ACK co 5 ramek są założeniami wymagającymi
potwierdzenia dla instalacji. Wyniki i ograniczenia porównania opisuje
lokalny `PODSUMOWANIE.md`. Dane lokalizacyjne i surowe odbiory pozostają
poza publicznym repozytorium.

Końcowy test implementacji: siedem przypadków integracyjnych poprawnych,
1561 identycznych skalarów regresji jednodniowej oraz poprawny bilans
24 000 zakończonych prób w syntetycznym teście wielokanałowym.
Raport zapisano w `tests_final/test_report.json` katalogu eksperymentu.
