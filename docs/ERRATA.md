# Errata do rozprawy doktorskiej

Dokument roboczy. Kolejne wykryte korekty będą dopisywane jako osobne,
numerowane pozycje.

**Aktualny etap: E-005, 20.09.2026.** Rozdzielono częstotliwości WOR
i zwykłego LoRaWAN, zachowując pojedynczy odbiornik relay. Wykonano testy
implementacji, ale nie przeliczono wykresów odpowiedzi. Publikowane dotąd
wyniki pozostają wynikami wcześniejszych wersji, nie E-005. E-004 poprawiała
metrykę odczytu DATA i zastąpiła wyniki N07 w punkcie 1.3.

## E-001 — prąd odbiornika w założeniach rozdziału 7

- Lokalizacja: rozdział 7, lista założeń modelu symulacyjnego.
- Jest: `IRX = 4,9 mA`.
- Powinno być: `IRX = 4,6 mA`.
- Uzasadnienie: poprawna wartość `4,6 mA` wynika z wcześniejszego wyliczenia
  przedstawionego w rozdziale 5. Wartość `4,9 mA` w rozdziale 7 jest omyłką.
- Zakres wpływu: korekta dotyczy wyników zużycia energii; nie zmienia modelu
  propagacji ani definicji metryk skuteczności transmisji.

## E-002 — rozkład terminów transmisji przy wielu ramkach w oknie

- Lokalizacja: założenie równomiernego rozkładu chwil transmisji w rozdziale 5,
  w szczególności §5.2.4 (s. 72); okna zgłoszeniowe w §6.9 oraz warianty
  liczby ramek w §7.1. Błąd potwierdzono w rozszerzonym modelu wykorzystanym
  do punktu 1.3 odpowiedzi recenzentom (N07).
- Charakter korekty: błąd implementacji harmonogramu ruchu dla więcej niż
  jednej ramki w oknie, a nie błąd generatora liczb pseudolosowych ani
  samej koncepcji przewidywania terminów transmisji.

### Opis rozbieżności

W badanej implementacji pierwszy termin losowano jednostajnie w całym
oknie, natomiast kolejny — w części pozostałej do jego końca. Dla dwóch
ramek i okna o długości `T`, po pominięciu milisekundowej dyskretyzacji,
odpowiada to zależnościom `t₁ ~ U(0,T)` oraz `t₂ | t₁ ~ U(t₁,T)`.
Kolejny termin wyznaczano przy obsłudze pierwszego zdarzenia aplikacyjnego,
nie dopiero po zakończeniu fizycznej transmisji radiowej.

Taki harmonogram nie zapewnia jednostajnego rozkładu łącznego ruchu w oknie:
drugie transmisje skupiają się bliżej jego końca. Dla okna 3 h wartości
oczekiwane terminów wynoszą odpowiednio 90 i 135 minut. W przebiegach N07
około 42,3% wszystkich UL przypadało na ostatnie 45 minut okna, zamiast
około 25% oczekiwanych przy równomiernym rozłożeniu ruchu. Powoduje to
systematyczne różnice obciążenia między częściami okna; samego
naprzemiennego przebiegu skuteczności nie należy interpretować jako
niestabilności mechanizmu retransmisji.

### Właściwa reguła i wprowadzona poprawka

Przed uruchomieniem transmisji należy wylosować wszystkie terminy z pełnego,
tego samego przedziału okna. Jeżeli kolejna próbka wskazuje termin już
wylosowany dla tego ED w danym oknie, losowanie tej próbki należy powtórzyć
z pełnego przedziału. Zaakceptowane terminy należy następnie uporządkować
chronologicznie i zrealizować bez ponownego zawężania przedziału losowania.
Regułę zastosowano także dla liczby ramek większej niż dwa.

W implementacji przedział jest prawostronnie otwarty, a terminy mają
rozdzielczość 1 ms. Powtórne losowanie usuwa wyłącznie identyczne terminy
tego samego ED; nie usuwa kolizji radiowych różnych urządzeń i nie ustanawia
dodatkowego minimalnego odstępu między ramkami. Bliskie terminy nadal
podlegają istniejącej obsłudze radia.

Istotne jest rozróżnienie kolejności losowania od kolejności nadawania.
Terminy `A` i `B`, oznaczone według kolejności losowania, mają każdy
jednostajny rozkład brzegowy w całym oknie. Warunek różnych terminów
uzależnia jednak zaakceptowane próbki od siebie. Dopiero po ich
uporządkowaniu pierwsza chronologicznie ramka otrzymuje termin
`t₁ = min(A,B)`, a druga — `t₂ = max(A,B)`. Rozkłady `t₁` i `t₂`
nie są oddzielnie jednostajne: wcześniejszy termin częściej przypada
w początkowej części okna, a późniejszy w końcowej. W przybliżeniu ciągłym
ich średnie wynoszą `T/3` i `2T/3`, czyli 60 i 120 min dla okna 3 h.
Jednostajny pozostaje rozkład ruchu rozpatrywanego łącznie.

Gdyby na histogramie zachować etykiety `A` i `B` zamiast etykiet
„pierwsza” i „druga chronologicznie”, oba przebiegi byłyby w przybliżeniu
płaskie. Byłaby to wyłącznie zmiana sposobu grupowania tych samych
terminów: rzeczywisty harmonogram transmisji i wyniki symulacji
pozostałyby bez zmian. Poprawność generatora należy zatem oceniać
na podstawie rozkładu łącznego, a nie wymagać stałej intensywności
oddzielnie dla pierwszej i drugiej chronologicznie ramki.

### Weryfikacja i wpływ na wyniki

Ponowiono 90 przebiegów N07: po 30 replik EP4312388, TS011 legacy i TS011
bez limitów przekazywania. Zachowano 385 ED, te same topologie dla każdego
ziarna, dwa okna po 3 h, dwie ramki ED na okno, 15 dni oraz brak rozgrzewki.
Nie zmieniano mechanizmów retransmisji ani parametrów limitów. Jednostką
statystyczną pozostaje replika; ponowienie nie zwiększa liczby niezależnych
replik danej konfiguracji do 60.

Po korekcie średnie terminy UL w EP wynoszą 59,98 i 119,91 min,
a ostatnia ćwiartka okna obejmuje 24,95% UL. Dla ramek 17–60 uzyskano:

| Mechanizm | Średnie P_UL przed korektą | Średnie P_UL po korekcie |
|---|---:|---:|
| EP4312388 | 80,47% | 83,10% |
| TS011 legacy | 64,85% | 66,49% |
| TS011 bez limitów przekazywania | 77,73% | 80,21% |

W EP różnica skuteczności pierwszej i drugiej ramki okna zmniejszyła się
z 4,24 do 0,07 p.p.; w TS011 legacy z 6,44 do 2,45 p.p. Utrzymująca się
różnica TS011 nie jest zatem wyłącznie skutkiem harmonogramu. Oddzielnej
weryfikacji wymaga obsługa informacji o wyczerpaniu limitów przekazywania;
nie została ona zmieniona w ramach E-002.

Testy obejmują ponowne losowanie identycznego terminu, granice przedziału,
sortowanie, liczbę ramek większą niż dwa oraz rozkład zagregowany. Kontrole
N=1 dla EP i TS011 (ziarno 0, pełne 15 dni) odtwarzają dokładnie poprzednie
rekordy ramek i wszystkie statystyki skalarne.

Zakres potwierdzony dotyczy wskazanej wersji rozszerzonego symulatora.
Wcześniejsza rewizja `a28d4ef` zawiera inny harmonogram; przypisanie błędu
konkretnym historycznym wynikom złożonej rozprawy wymaga powiązania ich
z wersją kodu użytego do obliczeń. Nie należy automatycznie przenosić
niniejszego rozpoznania na wszystkie rysunki rozdziału 7 ani uznawać
pozostałych analiz za ponownie przeliczone.

Stan na 19.09.2026: poprawiony harmonogram i nowe wyniki N07 przygotowano;
wcześniejsze dane zachowano. Tekst odpowiedzi i zbiorczy PDF nie zostały
jeszcze zaktualizowane i nadal zawierają wcześniejsze wyniki N07.

Materiały audytowe: `src/WindowSchedule.h`, `src/LogicEndnode.cc`, test
`scripts/tests/test_window_schedule.cc` oraz archiwum wyników
`C:/Omnet/simulation_results/review_chapter1/n07_uniform_385_3h/`
(parametry, 90 przebiegów, manifest i `analysis.json`).

## E-003 — potwierdzenia, przyłączenie i czasowanie wymiany z relay

### Zakres i związek z rozprawą

Rozbieżności stwierdzono w aktualnym, rozszerzonym symulatorze używanym do
odpowiedzi recenzentom. Punktem odniesienia są: strategia ACK 1:5 z §6.9
(`sec:strategia_potwierdzen`), parametry i kryterium K=6 z §7.1 oraz opis
przyłączenia i mechanizmów pośredniczenia z rozdziału 5. Audyt obejmuje też
TS011-1.0.1, TS001-1.0.4 i RP002-1.0.4. Nie jest dowodem, że każda z poniższych
rozbieżności występowała w wersji użytej do złożonej rozprawy. Przypisanie
korekty konkretnemu historycznemu wynikowi wymaga wskazania rewizji kodu.

Należy oddzielić **błędy implementacji** od **zmian konfiguracji badawczej**.
Próg TS011 równy jednemu brakowi oczekiwanego ACK wprowadzono na podstawie
decyzji autora podczas audytu; nie jest on obowiązkowym progiem standardu.
Nie wolno przedstawiać porównania z różnymi progami jako eksperymentu, w którym
różni się wyłącznie format radiowy mechanizmu pośredniczącego.

### Potwierdzone błędy i sposób korekty

1. **Niejawne potwierdzanie każdego bezpośredniego UL.** Obliczana reguła
   ACK co pięć ramek była zastępowana stałą wartością `true` w gałęzi
   bezpośredniej. Licznik obejmował ponadto Join, a po porażce wymuszał
   kolejne ACK. Wprowadzono osobny licznik danych: potwierdzenia dotyczą
   danych 5, 10, 15 itd., niezależnie od drogi. Join zawsze oczekuje
   Join-Accept, ale nie przesuwa cyklu 1:5. Ponowienie tej samej wiadomości
   także nie przesuwa tego cyklu.

2. **Brak identyfikacji oczekiwanej odpowiedzi.** Odpowiedź mogła skasować
   oczekiwanie dotyczące innej, późniejszej transmisji. Stan wymiany
   powiązano z urządzeniem, identyfikatorem wiadomości i numerem próby oraz
   sprawdzanym rodzajem odpowiedzi. ACK nie kończy Join, a WOR ACK nie
   zastępuje odpowiedzi LNS. Identyfikatory prób są metadanymi modelu,
   nie dodatkowymi bajtami nadawanymi w rzeczywistej ramce LoRaWAN.

3. **Nakładanie wymian jednego ED.** Nowe zdarzenie aplikacyjne mogło
   anulować wcześniejszy timeout. Zgłoszenia są teraz kolejkowane do końca
   bieżącej wymiany MAC. Nie zmieniono losowania terminów z E-002: losowanie
   opisuje powstanie zgłoszenia, natomiast rzeczywista obsługa może rozpocząć
   się później. Kolejka nie usuwa kolizji pomiędzy różnymi urządzeniami.
   Timeout jest uruchamiany po faktycznym zakończeniu nadawania, a nie po
   samym przekazaniu ramki do kolejki radia.

4. **Nieprawidłowe okna i opóźnienia.** RX2 zaczynało się 3 s po UL,
   otwierało się również po udanym RX1, a Join korzystał z czasowania danych.
   Odpowiedź LNS planowano w środku RX1. Skorygowano domyślne RX1/RX2 na
   1/2 s i Join na 5/6 s od końca nadawania. Prawidłowa odpowiedź w RX1
   odwołuje RX2. Dla TS011 zastosowano 50 ms odstępów WOR ACK, kolejnego UL
   i przekazania przez RL oraz RXR po 18 s; wcześniej odpowiednie wartości
   wynosiły 1 s i 7 s. EP zachowuje scenariuszowe RXR po 7 s, które nie jest
   parametrem normatywnym TS011. Ramki z przekroczonym terminem nadawania
   nie są bezterminowo wysyłane z kolejki poza właściwym oknem. Zajęte radio
   nie przesuwa nominalnego końca okna na późniejszy termin. Rozdzielczość
   symulacji pozostaje równa 1 ms.

5. **Utożsamianie braku ACK z koniecznością ponownego Join.** Oddzielono
   istnienie sesji od wyboru drogi. Dołączone ED zachowuje sesję po utracie
   ACK i może zmienić drogę danych bez zbędnego Join. W wybranym profilu TS011
   pierwszy brak oczekiwanego ACK uruchamia jedną dodatkową próbę tej samej
   potwierdzanej wiadomości przez relay, po 1-3 s losowego odstępu. Brak
   odpowiedzi na ramkę niepotwierdzaną nie jest zdarzeniem utraty ACK.
   EP zachowuje scenariuszowy próg sześciu braków oczekiwanych odpowiedzi.
   Nie odwzorowano osobno czasu decyzji serwera i desygnacji relay EP;
   samego progu K nie można nazywać czasem tej procedury.

6. **Niepełna obsługa WOR, Join i informacji Forward.** WOR poprzedzające
   Join nie wymaga WOR ACK i nie zwiększa licznika jego braków. Filtr Join
   oraz dostępność limitów są sprawdzane przed dopuszczeniem kolejnego UL;
   kontekst WOR ma ograniczony czas ważności i musi odpowiadać właściwej
   wiadomości oraz próbie. Przy przekazaniu następuje ponowna kontrola
   limitu. ED respektuje Forward: wartości 1 i 2 wstrzymują używanie relay
   przez odpowiednio 30 i 60 minut, a 3 oznacza wyłączenie przekazywania.
   W tym okresie profil zachowuje bezpośrednie okazje transmisji; nie
   przechowuje raportu godzinę w oczekiwaniu na token i nie ponawia Join.
   Jest to jawna polityka modelu. Dynamiczna rekonfiguracja MAC pozostaje
   poza jego zakresem, więc status 3 nie wygasa automatycznie po godzinie.

7. **Wspólny narzut dla różnych formatów ramek.** Do WOR ACK o długości
   7 B błędnie dodawano narzut zwykłej ramki LoRaWAN. Rozdzielono kompletne
   długości ramek sterujących od długości danych aplikacyjnych: Join-Request
   23 B, Join-Accept 17 B, pusty ACK 12 B, WOR ACK 7 B. Dla danych 51 B
   zachowano narzut 13 B; przekazanie TS011 obejmuje wewnętrzną ramkę,
   metadane 6 B oraz zewnętrzny narzut. WOR ACK przy SF9 trwa według
   zastosowanej funkcji 123,904 ms, czyli 124 ms po dyskretyzacji, zamiast
   około 185 ms. Nie zmieniano preambuły WOR ACK na skróconą: już wcześniej
   miała osiem symboli. Uproszczenia PHY pozostałe poza audytem nie oznaczają
   pełnej zgodności certyfikacyjnej ze standardem.

### Join a utrata ACK po przyłączeniu

Dla urządzenia jeszcze niedołączonego przyjęto TS011 przez relay przy co
czwartym Join-Request (próby 4, 8, 12...), zgodnie z przykładem informacyjnym
w dodatku do TS011. Jest to osobny parametr `ts011JoinRelayEvery=4`.
Natomiast `ts011MissedAcksToRelay=1` dotyczy danych po przyłączeniu. Przykładowo,
gdy pierwsze zgłoszenie zakończy się udanym Join, zgłoszenia 2-5 niosą dane
niepotwierdzane, a zgłoszenie 6 niesie piątą ramkę danych, wymagającą ACK.
Dopiero brak tego oczekiwanego ACK uruchamia natychmiastowy fallback. Nie
oznacza to oczekiwania na sześć utraconych ramek.

Parametry są jawnie konfigurowalne. Profil z progiem jednego braku ACK nie
powinien być utożsamiany z normatywnym trybem kontrolowanym TS011 z progami
8/16/32/64 UL bez poprawnego downlinku (nie są to progi brakujących ACK).
Określenie „legacy” w wynikach oznacza zachowany wariant koszyków
8 ramek/h na RL, a nie ponowne użycie błędnej implementacji.

### Jednostka pomiaru i ostrożność interpretacyjna

Ponowienie zachowuje identyfikator wiadomości. LNS może wysłać odpowiedź na
nową próbę, ale zalicza doręczenie wiadomości tylko raz. P_UL nadal opisuje
udział doręczonych **unikatowych UL**, obejmujących także Join; po dodaniu
fallbacku nie jest prawdopodobieństwem sukcesu pierwszej próby fizycznej.
WOR i WOR ACK nie powiększają mianownika. Ich transmisje i powtórzenia
pozostają natomiast w bilansie energii. Podczas Join harmonogram zgłoszeń
uruchamia próby przyłączenia, zamiast tworzyć niezależną kolejkę próbek
sensorycznych; zachowano to dotychczasowe uproszczenie modelu.

Zmieniono jednocześnie kilka mechanizmów. Różnica między wynikiem E-002
i E-003 jest łącznym skutkiem poprawek i zadeklarowanych zmian polityki.
Nie wyznacza oddzielnie udziału każdego błędu. Także kontrola bez koszyków
nie mierzy Duty Cycle: włącza lub wyłącza ograniczenie liczby przekazań.
Nie zmieniano topologii, parametrów propagacji, modelu wybudzenia EP,
kryptografii ani nie dodawano pełnego stosu MAC.

### Weryfikacja i reprodukowalność

Testy małych sieci potwierdziły ACK dla danych 5 i 10 po Join, wykonanie
30 gęstych zgłoszeń bez kasowania poprzedniej wymiany, anulowanie RX2 po RX1,
TS Join przez relay przy próbie 4, EP przy próbie 7 dla K=6, brak oczekiwania
na WOR ACK podczas Join, działanie filtru Join i blokady Forward. W teście
z tłumieniem wystąpiło 51 prób dla 50 unikatowych UL: dodatkowa próba danych
nie wywołała ponownego Join. Ślady potwierdzają 5 s do odpowiedzi Join,
18 s do RXR TS011, 7 s w EP i 124 ms WOR ACK przy SF9.

Seria N07 E-003 obejmuje 90 nowych przebiegów: po 30 dla EP, TS011 legacy
i kontroli bez koszyków, po 15 dni, bez rozgrzewki. Sprawdzono zgodność
topologii każdej repliki z E-002 i dokładnie 23 100 unikatowych UL na przebieg
(385 ED × 60). Ponowiono również pozostałe 2380 przebiegów aktualnego zakresu
rozdziału 1 tą samą wersją kodu: łącznie 2470 przebiegów w 98 wariantach.
Nie powtarzano wycofanych scenariuszy N07 z wcześniejszą osią dobową.
Archiwa wcześniejszych wyników pozostają zachowane;
nowe obliczenia zastępują je w analizie, ale nie zwiększają liczby
niezależnych replik jednego punktu.

Opublikowany program główny odtworzył trzy warianty N07 dla ziarna 0.
Rekordy UL i wyniki liczbowe są zgodne z kopią obliczeniową; pominięto
wyłącznie daty i godziny zapisu raportu. Wynik kontroli: `production_check/PASS.json`.

Materiały tej korekty zapisano w
`C:/Omnet/simulation_results/review_chapter1/E003_20260919/`:
`AUDYT_ACK_JOIN_TS011.md`, źródła i binarium `simulator/`,
`checks_v2/<scenariusz>/evidence.json`, `checks_v2/PASS.json`,
`n07/analysis.json`, `chapter1/chapter1_summary_exact.json`, raporty B11,
źródła LaTeX, wykresy oraz manifesty i skróty SHA-256.

### Wpływ na bieżące odpowiedzi recenzentom

W rozdziale 1 zastąpiono wcześniejsze wykresy, tabele i interpretację wynikami
E-003, bez modyfikowania tekstu złożonej rozprawy. Wyników E-002 podanych wyżej
nie należy traktować jako aktualnej wersji rozdziału: pozostają historycznym
zapisem etapu poprawki harmonogramu.

- N07, dni 5-15, cała sieć: P_UL wynosi 70,070% dla EP, 68,509% dla TS legacy
  i 81,102% dla TS bez koszyków. Po zastosowaniu polityki ACK 1:5 lokalne
  wzrosty przy UL 6, 11 i 16 odpowiadają częstszym ponowieniom potwierdzanych
  danych, a nie oczekiwaniu na sześć niepotwierdzonych porażek.
- B11, 385 ED: P_UL EP/TS/TS0 = 88,361/84,740/88,529%; odczyt dobowy
  99,671/98,346/98,562%. Sparowana różnica EP-TS0 w P_UL ma 95% PU obejmujący
  zero. Wcześniejszy wniosek o bezwarunkowej przewadze EP jest wycofany.
- W tym samym punkcie średni czas doręczonych UL wynosi 2,767/1,828/1,336 s.
  To czas od pierwszego zgłoszenia UL do radia, nie pełny czas od powstania
  danych. Późniejsze ponowienie jest już częścią tego pomiaru.
- Prąd RL dla EP/TS legacy: 72,86/168,78 µA; prąd ED: 28,825/28,633 µA.
  Przeliczono pełny bilans 30-dniowy, wraz z inicjalizacją i próbami utraconymi.

Przy aktualizacji opisu ujawniono także dwa ograniczenia pozostawione bez
zmiany modelu: osobne losowania współrzędnych x/y nie dają jednorodnego dysku
ani pierścienia (P16), a okresowo losowana poprawka progu odbioru nie jest
szeregiem z ANN ani walidacją długookresowej korelacji propagacji (P05/P06).
Odpowiednie wcześniejsze twierdzenia tekstu zostały sprostowane. Ewentualne
zmiany tych modeli wymagają osobno oznaczonych badań, nie dopasowywania
symulatora do oczekiwanej kolejności wyników.

Dokumenty normatywne: [TS011-1.0.1](https://resources.lora-alliance.org/technical-specifications/ts011-1-0-1-relay),
[TS001-1.0.4](https://lora-alliance.org/wp-content/uploads/2021/11/LoRaWAN-Link-Layer-Specification-v1.0.4.pdf),
[RP002-1.0.4](https://resources.lora-alliance.org/technical-specifications/rp002-1-0-4-regional-parameters).

## E-004 — odczyt dobowy musi oznaczać odebranie danych, nie Join

### Błąd i zakres korekty

W bieżącym symulatorze do zbioru udanych par urządzenie–doba trafiała każda
odebrana wiadomość UL, także Join-Request. Zawyżało to odczyt w okresie
przyłączania: odebrana prośba o przyłączenie nie zawiera pomiaru. Potwierdzono
to w N07, gdzie w pierwszej dobie model EP z desygnacją I+II wykazywał
98,38% sukcesu tych grup po uwzględnieniu Join, lecz tylko 29,81% ED
dostarczyło dane. Błąd jest w liczniku, nie w samym rysowaniu wykresu.

Znaczenie odczytu w §2.4 i §6.9 rozprawy wymaga co najmniej jednej ramki
danych odebranej w LNS w danej dobie. Mianownik obejmuje wszystkie ED,
także jeszcze niedołączone. Join oraz WOR nie zaliczają odczytu.
Brak ACK nie unieważnia danych już odebranych w LNS; odczyt UL i sukces
wymiany UL/DL są odrębnymi miarami. Dobę przypisuje się według odbioru.

W `Keeper` oddzielono identyfikatory danych od wszystkich UL i skorygowano
liczniki dobowe, zapis topologii, raport oraz skalar sukcesu. Klasyfikacja
działa również bez eksportu szczegółowych ramek. Mianownik skalara wyklucza
zadeklarowane doby rozgrzewki, zgodnie z licznikiem par urządzenie–doba.
P_UL pozostaje udziałem doręczonych unikatowych UL, łącznie z Join, po
dopuszczonych próbach; nie zmieniono jego definicji ani zdarzeń radiowych.

Nie przypisuje się automatycznie błędu wszystkim historycznym rysunkom
złożonej rozprawy bez sprawdzenia użytej rewizji i definicji. W tej korekcie
ponowiono N07 i zmieniono punkt 1.3 odpowiedzi. Pozostałe wykresy E-003
zachowano; ich stara miara dobowa obejmuje Join i nie jest tożsama z
odczytem danych. Ograniczenie ujawniono we wspólnej metodyce odpowiedzi.

### Pięć wariantów końcowego N07

Po 30 replik na wariant, 50 dni bez rozgrzewki, 385 ED (350+18+17),
jedna GW, dwa RL, dwa okna 12 h, po dwa zgłoszenia w oknie, ACK co piąte
dane. Topologie odpowiadają tym samym ziarnom co wcześniej.

- EP K=3, wszystkie ED uprawnione do desygnacji.
- EP K=3, desygnacja tylko grup I i II; pozostałe ED po nieudanym Join
  przez relay wracają do bezpośredniego Join.
- TS011 z dotychczasowymi limitami liczby przekazań.
- TS011 z tymi samymi limitami oraz statyczną listą dopuszczeń grup I i II.
- TS011 bez koszyków Join/trusted/global jako wariant kontrolny.

Lista TS011 blokuje przekazywanie Join i danych centralnych ED. Odmowa
Forward=3 dla poprawnego WOR danych przełącza kolejne dane ED na istniejącą
ścieżkę bezpośrednią. Odmowy listy mają osobny licznik, nie są wykazywane
jako wyczerpanie koszyka. Reguła opiera się na grupach konfiguracji,
nie na tym, czy punkt przypadkowo leży wewnątrz prostokąta na mapie.
To jawna polityka badawcza; nie jest pełną implementacją zdalnego
zarządzania listą ani nowym wymaganiem normatywnym TS011. Nie zmieniono
modelu kanałów. K=3 jest parametrem nowych scenariuszy EP, nie globalną
zmianą bazowej konfiguracji K=6. Domyślnie obie listy ograniczające są wyłączone.

### Weryfikacja i aktualne wyniki

W 120 odtworzeniach czterech wcześniejszych wariantów wszystkie 14 pól
rekordów UL zgadza się dokładnie z etapem 05. Korekta licznika nie zmienia
przebiegów radiowych. Dodatkowych 30 replik sprawdza nową listę TS011;
w żadnej nie występuje faktyczne nadanie RL dla centralnego ED.
Niezależnie sprawdzono zerowy odczyt przy samych Join, działanie licznika
bez eksportu ramek we wszystkich pięciu wariantach, odrzucenie doby
rozgrzewki oraz zaliczanie odebranych danych niepotwierdzanych.

Średnie za pełne 50 dni, grupy I i II: P_UL EP/EP-I+II/TS/TS-I+II/TS0
= 90,644/90,782/67,496/93,452/94,375%; odczyt dobowy danych
= 98,253/98,223/96,215/98,512/98,518%. Różnica P_UL po ograniczeniu
listy TS011 wynosi 25,956 p.p. (sparowany 95% PU: [25,308; 26,604]).
Wynik pokazuje znaczenie dopuszczania ED do relay, nie izoluje osobno
udziału każdego mechanizmu strat radiowych.

Pełne dane, źródła, binarium, polecenia, testy, manifesty oraz wykresy:
`C:/Omnet/simulation/_results/2026-09-20_06_N07_final_DATA_whitelist/`.
Na rys. 1.9 zachowano 20 kolejnych UL z oznaczeniem dni 1–5 i pięcioma
wariantami. Osobno przedstawiono 50 kolejnych dób i mapy obu miar.
Starszych archiwów ani plików złożonej rozprawy nie nadpisywano.

## E-005 — izolacja częstotliwości i pojedynczy odbiornik relay

### Rozbieżność w implementacji

Podczas trwającego odbioru `Radio` dopuszczał do porównania mocy także
ramki o innym identyfikatorze kanału. Mogły one spowodować kolizję albo
przejęcie odbioru mimo rozłącznych częstotliwości. Ponadto pierwszy kanał
WOR miał identyfikator 1, wspólny z transmisjami bezpośrednimi, a przekazanie
RL–GW miało identyfikator 3. Same numery nie zapewniały jednak separacji
interferencji. Odbiór WOR rezerwowano już od początku preambuły, na podstawie
przewidywanego przyszłego CAD, zamiast od rzeczywistej chwili skanu.

### Zmiana modelu

Na życzenie autora przyjęto scenariusz dwóch oddzielnych częstotliwości WOR,
rozłącznych także z używanym kanałem zwykłego LoRaWAN. Logiczne identyfikatory
to: zwykły LoRaWAN=1, pierwszy WOR=10, drugi WOR=11. Nie są to numery kanałów
planu regionalnego ani wartości częstotliwości w MHz. Wyłączenie drugiego
kanału nadal pozostawia pierwszy WOR oddzielony od ruchu do GW.

- Kolizję lub przejęcie trwającego odbioru może wywołać tylko ta sama
  częstotliwość; pozostawiono dotychczasowy próg przejęcia 6 dB.
- Relay ma jeden odbiornik i nadajnik half-duplex. Nasłuch danych/okna
  Class A, odbiór oraz nadawanie uniemożliwiają równoległy skan drugiej
  częstotliwości. Brak dostępnego odbiornika nie jest kolizją radiową.
- Skan pierwszego WOR odbywa się w chwilach nT, drugiego w (n+1/2)T.
  Odbiór zaczyna zdarzenie CAD, nie nadejście początku długiej preambuły.
  Zajętość nie przesuwa siatki skanowania. Preambuła może przetrwać zajętość
  i zostać wykryta później, jeśli pozostaje wymagany zapas symboli.
- Koniec WOR pozostaje końcem rzeczywistej emisji; oczekiwanie na CAD nie
  wydłuża emisji ani nie przesuwa następującej wymiany. TOffset wyznaczany
  jest względem chwili rozpoczęcia odbioru.
- Ustawienie częstotliwości późniejszego UL pozostaje niezależne od WOR.
  Dane ED–RL oraz przekazanie RL–GW domyślnie używają wspólnego kanału 1,
  tak jak bezpośrednie ED–GW. Nie przyznano relay odrębnego, wolnego
  kanału backhaul tylko z powodu wcześniejszej wartości parametru 3.
- Skorygowano powrót radia do stanu bezczynnego po wzajemnym zniszczeniu
  dwóch ramek; poprzednio mogło pozostawać zajęte bez zdarzenia zakończenia.
  Po utracie synchronizacji wskutek dryfu już pierwszy WOR wraca na kanał
  domyślny. Nie zmieniono progów dołączania, limitów ani whitelist.

TS011-1.0.1: §3.2.1 określa przesunięcie T/2 i wznowienie siatki po zajętości;
§3.2.3 dopuszcza drugi kanał różniący się częstotliwością **lub** szybkością
transmisji. Nie należy zatem twierdzić, że każda zgodna konfiguracja TS011
musi mieć dwie rozłączne częstotliwości. §3.4 i §5.3 oddzielają WOR od
następującego zwykłego UL, możliwego również do odbioru przez GW.
Źródło: [TS011-1.0.1](https://resources.lora-alliance.org/technical-specifications/ts011-1-0-1-relay).

### Granice i wpływ na wcześniejsze wyniki

Jest to model logicznych, nieinterferujących częstotliwości i punktowych
zdarzeń CAD, nie pełny model toru RF: brak przecieku sąsiednich kanałów,
pełnych ośmiu kanałów bramy i wielu równoległych demodulatorów. Nie dodano
ortogonalności SF. Zachowano uproszczenie WOR ACK na częstotliwości
odpowiedniego WOR; regionalne przesunięcie częstotliwości ACK nie jest tu
modelowane. Wybór najbliższego relay pozostaje dotychczasowym uproszczeniem.

Nie skalibrowano czasu sprzętowego CAD ani kosztu pojedynczego skanu.
`ts011ChannelScanSymbols` pozostaje parametrem zgodności dawnych konfiguracji,
ale nie daje już wstecznego marginesu przyjmowania preambuły: odbiornik nie
może zacząć odbierać przed aktualnym zdarzeniem CAD. W stanie WOR nadal
naliczany jest stały prąd zastępczy. Przebiegów 20 ms/2 kanały nie należy
traktować jako dowodu wykonalności sprzętowej. To oddzielne, otwarte zadanie.

E-005 zmienia zdarzenia radiowe, a nie tylko etykiety wykresów. Przed dalszą
interpretacją porównań TS/EP trzeba ponowić serie na wspólnej nowej wersji;
poprawka odzyskania stanu po kolizji dotyczy wspólnej klasy `Radio`.
Nie przypisuje się automatycznie tej wady wszystkim wynikom złożonej pracy
bez ustalenia rewizji. Nie zmieniano LaTeX, PDF ani zamkniętych archiwów.

### Sprawdzenie

- 21 deterministycznych asercji na rzeczywistej klasie radia: oba kierunki
  izolacji, kolizje na tej samej częstotliwości, przejęcie odbioru, siatka CAD,
  zajęty odbiornik/nadajnik, wyłączony drugi kanał i wznowienie po zajętości.
- Siedem dotychczasowych scenariuszy regresji MAC/Join/ACK/limitów: PASS.
- Izolowana pełna wymiana z dwoma kanałami: odebrane 80/80 ramek DATA;
  faktyczne akwizycje WOR występują na obu kanałach.
- 12 testów po 15 dni, 385 ED: sześć okresów CAD 20–1000 ms razy jeden/dwa
  kanały. Są to testy uruchomienia, nie nowe serie statystyczne do wykresów.
- Konfiguracja dwóch jednakowych identyfikatorów WOR zostaje odrzucona
  jako niezgodna z zakresem tej implementacji rozłącznych częstotliwości.

Kod, testy i archiwum sprzed zmiany:
`C:/Omnet/simulation/_results/2026-09-20_09_E005_kanaly_WOR/`.
