# Palacze tytoniu

## Instrukcja uruchomienia

W celu uruchomienia programu:

1. Należy użyć poniższej komendy, aby skompilować program:

`make palacze`

2. Wykonać plik `palacze.out`:

`./palacze.out`


## Opis problemu

Program rozwiązuje zmodyfikowany problem palaczy tytoniu ([Cigarette smokers problem](https://en.wikipedia.org/wiki/Cigarette_smokers_problem)) za pomocą mechanizmów IPC. Dodatkowe wymagania:
1. palacze rozliczają się między sobą za udostępniane składniki po aktualnym kursie giełdowym;
2. rola agenta sprowadza się do ustalania kursu giełdowego składników;
3. palacze nie mogą mieć debetu, więc jeśli palacza nie stać na zakup składników, musi poczekać na zmianę kursu lub przypływ gotówki za sprzedaż własnego składnika (początkowy stan konta palacza jest większy od 0);
4. palacz nabywa jednocześnie oba składniki;
5. należy użyć kolejki komunikatów do przekazywania składników między palaczami.

## Opis realizacji

Problem realizowany jest za pomocą dwóch funkcji – `agent` oraz `smoker`.

Agent odpowiada za zmianę cen składników. Generuje on nowe ceny wszystkich składników (tytoń, papier, zapałki), a następnie sprawdza czy któregoś z palaczy stać na zakup dwóch składników (poza swoim własnym). Jeśli palacz może zakupić składniki, zostaje on zasygnalizowany. W tym czasie, agent oczekuje na zakończenie palenia.

Każdy z trzech palaczy ma dwa procesy – potomny (kupuje składniki) oraz macierzysty (sprzedaje składniki).
Proces potomny oczekuje na sygnał od agenta, a następnie kupuje składniki od dwóch pozostałych palaczy. 
Proces macierzysty oczekuje na komunikat od kupującego, zwiększa swoje saldo i zatwierdza zakończenie transakcji.
Po zakończeniu obu transakcji, palacz pali papierosa.


## Mechanizmy synchronizacji

### Pamięć współdzielona

Obszary pamięci współdzielonej:
1. `prices` – tablica o rozmiarze 3, która przechowuje aktualne ceny składników (indeks: 0 – tytoń, 1 – papier, 2 – zapałki)
2. `balances` – tablica o rozmiarze 3, która przechowuje aktualne salda palaczy

### Semafory

Utworzone semafory:
1. `semid_agent` 
Semafor ma wartość początkową 1 (agent może zmieniać ceny). 
Jest to semafor, który kontroluje dostęp agenta do zmiany cen składników. Uniemożliwia zmianę cen składników, podczas gdy palacz dokonuje transakcji oraz pali papierosa. Jest on podnoszony po zakończeniu palenia przez palacza.

2. `semid_smokers`
Tablica semaforów o rozmiarze 3 (każdy odpowiada jednemu palaczowi), przy czym każdy z semaforów ma wartość początkową równą 0 (palacze czekają na sygnał od agenta).
Semafor dla konkretnego palacza jest używany do sygnalizowania, że dany palacz może kupić składniki, a następnie zapalić papierosa.

3. `semid_price_change`
Semafor ma wartość początkową 1. 
Uniemożliwia odczytywanie cen przez palaczy, gdy agent ustala nowe ceny składników. Gdy agent zmienia ceny, semafor ten jest opuszczany, aby żaden palacz nie mógł w tym czasie odczytywać cen. Po zakończeniu zmian cen jest on podnoszony.

4. `semid_balance`
Tablica semaforów o rozmiarze 3 (każdy odpowiada dostępowi do salda jednego palacza), przy czym każdy z semaforów ma wartość początkową równą 1.
Semafory chronią `balances[i]` podczas operacji na saldach palaczy. Semafor dla danego palacza jest opuszczany przed odczytem i modyfikacją jego salda, a następnie podnoszony po zakończeniu.

5. `semid_transaction`
Semafor ma wartość początkową 0.
Semafor ten kontroluje transakcje między palaczami. Zapewnia on, że wszyscy sprzedający otrzymają komunikat i zakończą transakcję sprzedaży, zanim kupujący palacz rozpocznie palenie papierosa.

### Kolejka komunikatów

Płatność pomiędzy palaczami odbywa się przez wysyłanie (kupujący) oraz odbieranie (sprzedający) komunikatów.

Komunikat `Message`:
1. `mtype` – odbiorca komunikatu (sprzedający) + 1 
2. `mvalue[0]` – wartość przekazywanej kwoty
3. `mvalue[1]` – nadawca komunikatu (kupujący)
