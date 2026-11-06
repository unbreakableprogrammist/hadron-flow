# Symulacja i analiza rozkładów cząstek dla pp @ √s = 13.6 TeV (Pythia 8 + ROOT)

Kompletny pipeline: generacja zderzeń proton–proton przy √s = 13.6 TeV generatorem Pythia 8,
analiza cząstek stanu końcowego w C++/ROOT oraz wizualizacja wyników.
Treść zadania: `Zadanie wstępne z analizy danych.pdf`.

**Stan prac:** zrobiony jest punkt 1 (generacja danych). Analiza (`analyze.cc` → `analysis.root`)
i makro rysujące (`draw_plots.C`) będą dodane później.

---

## Wymagania

- kompilator z obsługą **C++17** (g++)
- **ROOT 6** (testowano na 6.40), z dostępnym `root-config`
- **Pythia 8** w wersji ≥ 8.309 (testowano na 8.312), z dostępnym `pythia8-config`.
  Wersja 8.309 jest wymagana ze względu na `PythiaParallel`, używane w `generate.cc`.
- `make`

`pythia8-config` i `root-config` muszą być w `PATH`, np. po aktywacji środowiska conda z ROOT i Pythią
(`conda activate hep-env`).

---

## Opis plików

| Plik | Opis |
|---|---|
| `generate.cc` | Generator zdarzeń, **wersja wielowątkowa** (`Pythia8::PythiaParallel`, wszystkie dostępne wątki procesora). Wynik: `events.root`. |
| `generate_single_thread.cc` | Generator zdarzeń, **wersja jednowątkowa** (klasyczna pętla `pythia.next()`). Wynik: `events.root`. |
| `Makefile` | Kompilacja i uruchamianie programów. |
| `Zadanie wstępne z analizy danych.pdf` | Treść zadania. |

Oba generatory są równoważne fizycznie i zapisują dane w **tym samym formacie do tego samego pliku
`events.root`**. Można użyć dowolnego z nich, a dalsza analiza zawsze czyta `events.root`.
Każde uruchomienie nadpisuje poprzedni plik.

### Konfiguracja generacji (wspólna dla obu programów)

| Ustawienie Pythii | Wartość | Znaczenie |
|---|---|---|
| `Beams:idA`, `Beams:idB` | `2212` | zderzenia proton–proton |
| `Beams:eCM` | `13600.` | √s = 13.6 TeV |
| `SoftQCD:all` | `on` | tryb *minimum bias* |
| `Random:setSeed`, `Random:seed` | `on`, `12345` | stały seed, czyli produkcja reprodukowalna |
| liczba zdarzeń | `100000` (domyślnie) | zgodnie z zadaniem |

Zapisywane są **wyłącznie cząstki stanu końcowego** (`particle.isFinal()`), więc w pliku
nie ma cząstek pośrednich.

### Format pliku `events.root`

Drzewo `TTree` o nazwie `events`. **Jeden wpis odpowiada jednemu zderzeniu.** Gałęzie to wektory,
w których element `i` opisuje `i`-tą cząstkę końcową:

| Gałąź | Typ | Opis |
|---|---|---|
| `id` | `vector<int>` | kod PDG (±211 piony, ±321 kaony, ±2212 protony, ...) |
| `charge` | `vector<int>` | ładunek elektryczny (−1, 0, +1) |
| `pt` | `vector<double>` | pęd poprzeczny p_T [GeV] |
| `eta` | `vector<double>` | pseudorapidity η |
| `phi` | `vector<double>` | kąt azymutalny φ [rad], zakres (−π, π] |

Oprócz drzewa plik zawiera obiekt `production_info` (`TNamed`) z parametrami produkcji:
programem, seedem, liczbą wątków, liczbą zdarzeń i komendą, którą można ją odtworzyć.
Podgląd w ROOT:

```cpp
TFile f("events.root"); f.Get("production_info")->Print();
```

---

## Kompilacja i uruchamianie

```bash
make                  # kompiluje oba generatory
make run              # generator wielowątkowy  -> events.root (100 000 zdarzeń)
make run_single       # generator jednowątkowy  -> events.root (100 000 zdarzeń)
make clean            # usuwa skompilowane programy
```

Liczbę zdarzeń można zmienić zmienną `N`, np. `make run N=1000`.

Programy można też uruchomić bezpośrednio:

```bash
./generate [liczba_zdarzen] [liczba_watkow]   # domyślnie: 100000, wszystkie wątki
./generate_single_thread [liczba_zdarzen]     # domyślnie: 100000
```

Po zakończeniu każdy program wypisuje czas inicjalizacji, czas generacji, czas zapisu
i wydajność (zdarzenia/s). Do porównania obu wersji służy pomocniczy cel `make benchmark`.

---

## Dwie wersje generatora

### Po co wersja wielowątkowa

Zderzenia są od siebie **niezależne**, więc można je generować równolegle. Wersja jednowątkowa
przez kilka minut obciąża tylko jeden rdzeń, a pozostałe stoją bezczynnie.
`generate.cc` wykorzystuje wszystkie wątki procesora.

### Jak to działa

- Liczba wątków jest pobierana z systemu przez `std::thread::hardware_concurrency()`.
  Na Linuksie ta funkcja korzysta z `sysconf(_SC_NPROCESSORS_ONLN)` / `sched_getaffinity`.
- `PythiaParallel` tworzy **osobną instancję Pythii dla każdego wątku**, z seedem
  `12345 + k` dla wątku `k`.
- Przy `Parallelism:balanceLoad = on` każdy wątek generuje **stałą liczbę zdarzeń** (N / liczba wątków).
- Zapis do `TTree` odbywa się w funkcji wywoływanej przez Pythię **po kolei**, nigdy przez dwa
  wątki naraz (`Parallelism:processAsync = off`), więc nie ma wyścigów i nie trzeba mutexów.

### Wpływ na szybkość

Pomiar na Intel Core i7-4510U (2 rdzenie / 4 wątki):

| | 1 wątek | 4 wątki |
|---|---|---|
| Inicjalizacja Pythii | ~36 s | ~68 s |
| Wydajność generacji | ~280 zdarzeń/s | ~650 zdarzeń/s |

- Sama **generacja jest ~2.3× szybsza**. Procesor ma tylko 2 fizyczne rdzenie,
  dlatego przyspieszenie jest mniejsze niż 4×.
- **Inicjalizacja jest dłuższa**, bo każda instancja Pythii inicjalizuje się osobno.
  Przy małej liczbie zdarzeń (rzędu tysięcy) wersja jednowątkowa jest więc szybsza.
  Przy docelowych 100 000 zdarzeń wersja wielowątkowa wyraźnie wygrywa.

### Reprodukowalność produkcji

| | Co jest powtarzalne |
|---|---|
| `generate_single_thread` | **Wszystko.** Ten sam seed daje identyczny plik, łącznie z kolejnością zdarzeń. |
| `generate` | **Zbiór zdarzeń**, przy tej samej liczbie wątków. Każdy wątek ma stały seed i generuje stałą liczbę zdarzeń, więc powstają dokładnie te same zdarzenia. Zmienia się tylko **kolejność wierszy** w `TTree`, bo zależy od planisty systemu operacyjnego. Histogramy (`analysis.root`) wychodzą więc identyczne. |

Wynik wersji wielowątkowej zależy od **liczby wątków**. Na komputerze z inną liczbą wątków
powstanie inny, statystycznie równoważny zbiór zdarzeń. Aby dokładnie odtworzyć produkcję,
podaj liczbę wątków zapisaną w `production_info`:

```bash
./generate 100000 4
```

---

## Uwagi techniczne

- Kod jest zgodny z C++17 (`-std=c++17` w `Makefile`). `root-config --cflags` dokłada jednak
  `-std=c++20`, jeśli ROOT został zbudowany w C++20, i wtedy ta flaga ma pierwszeństwo.
  Mieszanie standardów z bibliotekami ROOT nie jest zalecane, dlatego zostawiono flagi z `root-config`.
- `SoftQCD:all` obejmuje także zderzenia elastyczne. Dają one zdarzenia z dwoma protonami
  przy bardzo dużym |η|, poza zakresem histogramów.
- Pythia zwraca φ w zakresie (−π, π]. Zadanie wymaga histogramu φ w zakresie 0–π,
  co zostanie uwzględnione w programie analizującym.
