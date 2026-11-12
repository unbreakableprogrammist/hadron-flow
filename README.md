# Symulacja i analiza rozkładów cząstek dla pp @ √s = 13.6 TeV (Pythia 8 + ROOT)

Pipeline do generacji zderzeń proton–proton przy √s = 13.6 TeV w Pythii 8, analizy cząstek stanu końcowego w C++/ROOT i przygotowania histogramów. Treść zadania jest w pliku `Zadanie wstępne z analizy danych.pdf`.

Stan prac: zrobione punkty 1–3 (generacja danych do `events.root` oraz analiza i zapis 32 histogramów do `analysis.root`). Do zrobienia zostało makro rysujące `draw_plots.C` i prezentacja.

## Wymagania

- kompilator z obsługą C++17 (`g++`)
- ROOT 6 (testowano na 6.40) z `root-config` w `PATH`
- Pythia 8 (testowano na 8.312) z `pythia8-config` w `PATH`
- `make`

Do `generate.cc` potrzebna jest Pythia ≥ 8.309 (ze względu na `PythiaParallel`). Na starszej wersji Pythii 8 użyj wersji jednowątkowej (`make run_single`), która korzysta ze standardowego API Pythii i zapisuje taki sam format `events.root`. Wersję można sprawdzić przez `pythia8-config --version`.

## Pliki w repozytorium

- `generate.cc` – wielowątkowy generator zdarzeń (`PythiaParallel`, domyślnie wszystkie wątki CPU) → `events.root`
- `generate_single_thread.cc` – jednowątkowy generator (`pythia.next()`) → `events.root`
- `analyze.cc` – analiza drzewa z `events.root` i zapis 32 histogramów do `analysis.root`
- `Makefile` – kompilacja, uruchamianie i benchmark
- `Zadanie wstępne z analizy danych.pdf` – treść zadania

## Kompilacja i uruchamianie

Pełny przebieg (generacja 100 000 zdarzeń + analiza):

```bash
make                  # kompiluje generate i generate_single_thread
make analyze          # kompiluje analyze
make run              # generuje 100 000 zdarzeń (wielowątkowo) -> events.root
./analyze             # analizuje events.root -> analysis.root
```

Pozostałe cele w `Makefile` i uruchamianie z argumentami:

```bash
make run_single       # generacja jednowątkowa -> events.root
make run N=1000       # szybki test na mniejszej liczbie zdarzeń
make benchmark N=1000 # porównuje czasy generate_single_thread i generate
make clean            # usuwa binarki i logi benchmark_*.log (nie usuwa *.root)
```

Bezpośrednie wywołanie programów:

```bash
./generate [liczba_zdarzen] [liczba_watkow]
./generate_single_thread [liczba_zdarzen]
./analyze [plik_wejsciowy] [plik_wyjsciowy]
```

## Jak rozwiązujemy zadanie i dlaczego tak

1. **Rozdzielenie na dwa programy (`events.root` → `analysis.root`)**  
   Zamiast robić histogramy w trakcie symulacji, najpierw zapisujemy cząstki do drzewa `TTree` w `events.root`, a dopiero potem analizujemy je w `analyze.cc`. Generacja 100 tys. zdarzeń w Pythii trwa wyraźnie dłużej niż sama analiza (~kilka sekund), więc przy poprawkach w analizie czy histogramach nie trzeba za każdym razem powtarzać symulacji.

2. **Tylko cząstki stanu końcowego (`p.isFinal()`)**  
   W tablicy `pythia.event` znajdują się też cząstki pośrednie (partony, krótkożyciowe rezonanse przed rozpadem). Filtr `p.isFinal()` już na etapie generacji zapewnia, że do `events.root` trafiają wyłącznie cząstki końcowe i nie dochodzi do podwójnego zliczania.

3. **Zrównoleglenie i reprodukowalność**  
   Kolejne zderzenia pp są od siebie niezależne, więc `generate.cc` korzysta z `PythiaParallel`. Każdy wątek `k` dostaje osobną instancję Pythii z seedem `12345 + k` i stałą liczbą zdarzeń (`Parallelism:balanceLoad = on`), a callback wypełniający `TTree` wykonuje się sekwencyjnie, bez wyścigów danych. Wersja jednowątkowa (`generate_single_thread.cc`) daje za każdym uruchomieniem identyczną kolejność wpisów w `events.root` i działa też na starszej Pythii.

4. **Składanie kąta φ do `[0, π]` (`foldPhi`)**  
   Pythia zwraca φ w zakresie `(−π, π]`, natomiast w zadaniu histogram `hPhi_*` ma zakres `[0, π]`. Z uwagi na symetrię azymutalną zderzeń proton–proton bierzemy `|φ|`, dzięki czemu nie wyrzucamy połowy danych (`φ < 0`). Wartość równą dokładnie `π` przycinamy minimalnie w dół przez `std::nextafter(TMath::Pi(), 0.0)`, bo w ROOT przedziały binów są lewostronnie domknięte `[min, max)` i dokładnie `π` wpadłoby do overflow.

5. **Poprawne zliczanie krotności `hN_*`**  
   Liczniki cząstek każdej kategorii zerujemy na początku zdarzenia i wpisujemy do `hN_*` po pętli po cząstkach — również wtedy, gdy w danym zderzeniu nie było np. żadnego kaonu (`N = 0`).

## Szczegóły formatu danych

### Konfiguracja Pythii i `events.root`

- `Beams:idA = 2212`, `Beams:idB = 2212`, `Beams:eCM = 13600.` (pp @ √s = 13.6 TeV)
- `SoftQCD:all = on` (minimum bias; obejmuje też zderzenia elastyczne o bardzo dużym `|η|`)
- `Random:setSeed = on`, `Random:seed = 12345`, domyślnie `100000` zdarzeń

Drzewo `TTree` o nazwie `events` (1 wpis = 1 zderzenie, gałęzie jako wektory po cząstkach końcowych):

| Gałąź | Typ | Opis |
|---|---|---|
| `id` | `vector<int>` | kod PDG (±211 piony, ±321 kaony, ±2212 protony, ...) |
| `charge` | `vector<int>` | ładunek elektryczny (−1, 0, +1) |
| `pt` | `vector<double>` | pęd poprzeczny p_T [GeV] |
| `eta` | `vector<double>` | pseudorapidity η |
| `phi` | `vector<double>` | kąt azymutalny φ [rad], zakres (−π, π] |

Dodatkowo zapisywany jest obiekt `production_info` (`TNamed`) z parametrami produkcji i komendą do jej odtworzenia:

```cpp
TFile f("events.root");
f.Get("production_info")->Print();
```

### Histogramy w `analysis.root`

Program `analyze.cc` tworzy 32 histogramy `TH1D` — po 4 dla każdej z 8 kategorii:
- `all` (wszystkie cząstki końcowe), `ch` (`charge != 0`),
- `pip` / `pim` (π⁺ / π⁻, PDG `±211`), `Kp` / `Km` (K⁺ / K⁻, PDG `±321`), `p` / `pbar` (p / p̄, PDG `±2212`).

Zakresy osi zgodne z treścią zadania:
- `hN_*`: 400 binów w `[0, 400)` (zdarzenia z `N >= 400` dla `all`/`ch` trafiają do overflow, program wypisuje o tym informację)
- `hPt_*`: 100 binów w `[0, 10)` GeV
- `hEta_*`: 100 binów w `[-5, 5)`
- `hPhi_*`: 64 biny w `[0, π]` rad

## Wydajność i reprodukowalność

Przykładowy pomiar dla 100 000 zdarzeń na CPU Intel Core i7-4510U (2 rdzenie / 4 wątki):

| | 1 wątek | 4 wątki |
|---|---|---|
| Inicjalizacja Pythii | ~36 s | ~68 s |
| Wydajność generacji | ~280 zdarzeń/s | ~650 zdarzeń/s |

Wersja wielowątkowa dłużej się inicjalizuje (osobna instancja Pythii na wątek), ale sama generacja jest ok. 2.3× szybsza.

- `generate_single_thread` dla stałego seeda daje bitowo powtarzalne wyniki i tę samą kolejność zdarzeń.
- `generate` dla tej samej liczby wątków generuje dokładnie ten sam zbiór zdarzeń (kolejność wpisów w `TTree` może się różnić między wątkami, ale wynikowe histogramy w `analysis.root` są identyczne). Na innej maszynie wystarczy podać liczbę wątków jako drugi argument, np. `./generate 100000 4`.

## Uwagi techniczne

- W `Makefile` ustawiono `-std=c++17`, a `root-config --cflags` może dopisać nowszy standard (np. `-std=c++20`), zależnie od tego, z jakim ABI zbudowano ROOT.
