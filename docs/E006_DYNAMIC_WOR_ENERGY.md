# E006. Dynamiczny bilans gotowości WOR

Data: 27.09.2026. Zakres: TS011, energia radia. Dokument odpowiedzi recenzentom pozostaje bez zmian.

## Korekta

Poprzednio relay w stanie WOR zużywał stałe 0,070 mA niezależnie od okresu CAD.
Ta wartość nie pochodziła z pomiaru prototypu. Nie odzwierciedlała zmiany częstości skanowania.
Obecnie domyślnie działa `ts011DynamicWorEnergy=true`.

W stanie oczekiwania na WOR średni prąd wynosi:

`I_WOR = I_sleep + (I_CAD - I_sleep) * n_channels * t_scan / T_CAD`.

Czas `t_scan` jest pobierany z profilu SX126x dla bieżącego SF i liczby symboli
sprzętowej detekcji `ts011CadSymbolNum`. Pasmo wynosi 125 kHz, zgodnie z obecnym
modelem wszystkich ramek w `LoRaPacket::symbolTime()`. Profil nie służy do analizy innych pasm.
Źródłem czasów jest nota Semtech AN1200.48, rev. 2.1, §4.2.1, tabele 93 oraz 95-99.
Domyślny profil obejmuje 4 symbole i prąd CAD 3,84 mA. SF9 oznacza czas 19,145 ms.
Nie należy utożsamiać `ts011CadSymbolNum` z protokołowymi `CadToRx` i `ChannelScan`.

Źródło: [Semtech AN1200.48, kopia noty producenta](https://manuals.plus/m/6fc059fa9e99f1ce788c08531e3ef6d61fa6724c41e35c02633468eee2112685.pdf).

Bilans gotowości jest naliczany wyłącznie podczas stanu WOR. Gdy radio odbiera
lub nadaje, obowiązuje koszt właściwego stanu. Stały pobór układów towarzyszących
`currentConsumptionBaseOffset` jest dodawany oddzielnie jeden raz. Obsługa drugiego
kanału zwiększa liczbę skanowań w okresie do dwóch. Konfiguracja, w której skanowania
nie mieszczą się w okresie pojedynczego toru radiowego, kończy się błędem.

Dla parametrów serii gotowość wynosi 79,010 µA przy okresie 1 s oraz 3676,079 µA
przy 20 ms, przed doliczeniem 3 µA układów towarzyszących i kosztów obsługi ruchu.
Zależność dominującego składnika ma postać odwrotności okresu CAD.

## Nadawanie WOR przez ED

Istniejący model już wyznaczał czas WOR z SF, długości pakietu oraz zmiennej
liczby symboli preambuły. Radio całkuje prąd TX przez faktyczny czas nadawania.
Nie dodano powtórnie tej energii. Dodano osobne liczniki czasu i ładunku WOR,
stanowiące podzbiór istniejącego bilansu TX, przydatne do kontroli wyników.

Przy pierwszym kontakcie ED nie zna jeszcze okresu relay i stosuje dotychczasowy
okres początkowy 1 s. Po WOR ACK uzyskuje parametry relay. Długość kolejnych
preambuł zależy także od synchronizacji i narastania niepewności zegarów.
Dlatego średni koszt WOR nie musi rosnąć liniowo z okresem CAD.

## Ograniczenia

Jest to profil referencyjny układu SX126x, bez pomiarowej walidacji konkretnej płytki.
Prądy RX, TX, uśpienia i układów towarzyszących zachowano z konfiguracji badawczej.
Prąd CAD można zmienić parametrem `currentConsumptionCad` po pomiarze sprzętu.
Energia skanowania jest uśredniona w stanie WOR. Dotychczasowy model zdarzenia
wykrycia CAD, jego czułości i fazy pozostawiono bez zmian. Nie odwzorowuje on
oddzielnych impulsów poboru prądu, przejść zasilania MCU ani fałszywych detekcji.
Zmiana liczby symboli CAD zmienia tu energię. Nie jest pełnym modelem wpływu
tej liczby na czułość wykrywania. Seria wykorzystuje niezmienne 4 symbole.

Opcja `ts011DynamicWorEnergy=false` służy wyłącznie do odtwarzania starego bilansu.
Historyczna wartość `currentConsumptionWOR` nie wpływa na normalną pracę TS011
z włączonym modelem dynamicznym. Model EP pozostał bez zmian.

## Kontrole

- Testy profilu, zależności od okresu, sumowania dwóch kanałów i odrzucania błędnych parametrów.
- Identyczne rekordy ramek TS011 przed zmianą i po zmianie przy tych samych parametrach.
- Identyczne stare skalary po wyłączeniu modelu dynamicznego.
- Niezmienione ramki i prąd modelu EP.
- Zgodność ładunku WOR TX z iloczynem czasu emisji oraz prądu TX, bez podwójnego naliczania.
- Zgodność ładunku gotowości WOR z czasem tego stanu i wyliczonym prądem.
- Dodatnia energia oraz kontrola liczby ED i RL w każdej replice.

Bilans jest wyrażony w mA·s, czyli jako ładunek. Suma prądów ED i RL nie obejmuje
bramy zasilanej sieciowo. Nie stanowi sumy mocy przy różnych napięciach baterii.
