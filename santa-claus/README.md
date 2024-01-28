# Święty Mikołaj

## Instrukcja uruchomienia

W celu uruchomienia programu:

1. Należy użyć poniższej komendy, aby skompilować program:

`make miki`

2. Wykonać plik `miki.out`:

`./miki.out`


## Opis problemu

Święty Mikołaj śpi w swojej chatce na biegunie północnym. Może go zbudzić jedynie przybycie dziewięciu reniferów lub trzy spośród dziesięciu skrzatów, chcących poinformować Mikołaja o problemach z produkcją zabawek (snu Mikołaja nie może przerwać mniej niż dziewięć reniferów ani mniej niż trzy skrzaty!). Gdy zbiorą się wszystkie renifery, Mikołaj zaprzęga je do sań, dostarcza zabawki grzecznym dzieciom (oraz studentom realizującym pilnie zadania z synchronizacji współbieżnych procesów), wyprzęga je i pozwala odejść na spoczynek. Mikołaj zbudzony przez skrzaty wprowadza je do biura, udziela konsultacji, a później żegna. Obsługa reniferów ma wyższy priorytet niż obsługa skrzatów.
Program rozwiązuje problem synchronizacji Świętego Mikołaja, reniferów i skrzatów za pomocą mechanizmów synchronizacji wątków w standardzie POSIX Threads.


## Opis realizacji

Problem realizowany jest za pomocą wątków: 

1. Wątek Świętego Mikołaja `santa_thread`

Odpowiada za obsługę Świętego Mikołaja. Funkcją główną wątku jest funkcja `santa`. Święty Mikołaj śpi, dopóki nie zostanie obudzony przez wystarczającą liczbę reniferów lub skrzatów. Po obudzeniu obsługuje jedną z tych grup (renifery obsługiwane są w pierwszej kolejności). Po zakończeniu obsługi, Święty Mikołaj oczekuje, aż wszystkie renifery zaczną odpoczywać (lub wszystkie skrzaty wrócą do pracy), a następnie w zależności od ilości oczekujących obsługuje następną grupę lub idzie spać.

2. Wątki reniferów `reindeer_threads`

Każdy wątek reprezentuje jednego renifera i działa w nieskończonej pętli. Renifery oczekują na Świętego Mikołaja, aż zbierze się ich wystarczająca ilość. Gdy osiągnięta jest ich wymagana ilość, Święty Mikołaj budzi się, zaprzęga renifery i dostarcza z nimi zabawki. Po zakończeniu dostarczania zabawek, renifery odpoczywają przez losowy czas (z zakresu od 1 do 5 sekund).

3. Wątki skrzatów `elves_threads`

Każdy wątek reprezentuje jednego skrzata i działa w nieskończonej pętli. Skrzaty oczekują na Świętego Mikołaja, aż zbierze się ich wystarczająca ilość. Gdy osiągnięta jest ich wymagana ilość, Święty Mikołaj budzi się, wprowadza do swojego biura i przeprowadza z nimi konsultację. Po zakończeniu konsultacji, skrzaty wracają do pracy i pracują przez losowy czas (z zakresu od 1 do 5 sekund).


## Mechanizmy synchronizacji wątków

### Mechanizm wzajemnego wykluczania – mutex `pthread_mutex_t mtx`

Zamek `mtx` pozwala na ograniczenie przeplotów operacji pochodzących z różnych wątków, w sekcjach krytycznych kodu. 

Aby zapewnić możliwość wstrzymywania wykonywania instrukcji jednego wątku do momentu uzyskania określonego stanu przez inny wątek, użyto zmiennych warunkowych.

### Zmienna warunkowa `pthread_cond_t cnd_santa`

Zmienna jest związana z obudzeniem Świętego Mikołaja. Jest używana w pętli `while` funkcji `santa`, gdzie oczekuje na spełnienie warunku (wystarczająca ilość oczekujących reniferów lub skrzatów). Gdy warunek zostaje spełniony, następuje wysłanie sygnału. Warunek sprawdzany jest ponownie i jeśli jest spełniony, Święty Mikołaj przechodzi do obsługi reniferów/skrzatów.

### Zmienna warunkowa `pthread_cond_t cnd_reindeer`

Zmienna warunkowa, która jest niezbędna do wstrzymania wykonywania instrukcji wątków reniferów, podczas gdy renifery dostarczają zabawki wraz ze Świętym Mikołajem. Po zakończeniu dostarczania zabawek, wątek Świętego Mikołaja sygnalizuje zmienną `cnd_reindeer`, że wątki reniferów mogą kontynuować wykonywanie instrukcji funkcji (renifery przechodzą w odpoczynek).

### Zmienna warunkowa `pthread_cond_t cnd_elves`

Zmienna, która służy do wstrzymania wykonywania instrukcji wątków skrzatów. Działa analogicznie to zmiennej `cnd_reindeer`.

### Zmienna warunkowa `pthread_cond_t cnd_wait`

Jest to zmienna warunkowa `cnd_wait` zabezpiecza przed rozpoczęciem obsługi kolejnej grupy przez Świętego Mikołaja lub jego powrotem do snu przed przejściem w odpoczynek wszystkich reniferów (lub powrotem do pracy wszystkich elfów). Po wyprzężeniu reniferów (lub pożegnaniu elfów) wątek Świętego Mikołaja wstrzymuje wykonywanie instrukcji, oczekując na sygnał od wszystkich reniferów, które dostarczały zabawki lub od wszystkich skrzatów, które uczestniczyły w konsultacjach.
