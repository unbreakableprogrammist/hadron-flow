# Symulacja i analiza rozkładów cząstek dla pp @ √s = 13.6 TeV (Pythia 8 + ROOT)

Pipeline do generacji zderzeń proton–proton przy √s = 13.6 TeV w Pythii 8, analizy cząstek stanu końcowego w C++/ROOT oraz wizualizacji wyników. Treść zadania znajduje się w pliku `Zadanie wstępne z analizy danych.pdf`.

Stan prac: zrealizowany punkt 1 (generacja danych). Program analizujący (`analyze.cc` → `analysis.root`) i makro rysujące (`draw_plots.C`) zostaną dodane w kolejnych krokach.

## Wymagania

- kompilator z obsługą C++17 (`g++`)
- ROOT 6 (testowano na 6.40), z dostępnym `root-config`
- Pythia 8 (testowano na 8.312), z dostępnym `pythia8-config`
- `make`

Wersja Pythii ≥ 8.309 jest potrzebna do `generate.cc`, ponieważ korzysta on z klasy `PythiaParallel`. Na starszych wersjach Pythii 8 samo `make` nie skompiluje `generate.cc` — należy wtedy użyć wersji jednowątkowej (`make run_single`), która korzysta wyłącznie ze standardowego API Pythii i daje ten sam wynik w pliku `events.root`. Wersję można sprawdzić poleceniem `pythia8-config --version`.

 Narzędzia `pythia8-config` i `root-config` muszą być dostępne w `PATH` (np. po aktywacji środowiska `conda activate hep-env`).

## Opis plików

- `generate.cc` – generator zdarzeń w wersji wielowątkowej (`Pythia8::PythiaParallel`, domyślnie wszystkie wątki procesora). Zapisuje wynik do `events.root`.
- `generate_single_thread.cc` – generator zdarzeń w wersji jednowątkowej (standardowa pętla `pythia.next()`). Zapisuje wynik do `events.root`.
- `Makefile` – reguły kompilacji i uruchamiania.
- `Zadanie wstępne z analizy danych.pdf` – treść zadania.

Oba generatory zapisują dane w tym samym formacie do pliku `events.root`, z którego korzysta dalsza analiza.

### Konfiguracja Pythii

- `Beams:idA = 2212`, `Beams:idB = 2212` – zderzenia proton–proton
- `Beams:eCM = 13600.` – energia w układzie środka masy √s = 13.6 TeV
- `SoftQCD:all = on` – procesy minimum bias
- `Random:setSeed = on`, `Random:seed = 12345` – stały seed zapewniający reprodukowalność
- domyślna liczba zdarzeń: `100000`

Do pliku zapisywane są wyłącznie cząstki stanu końcowego (`p.isFinal()`).

### Struktura pliku `events.root`

Drzewo `TTree` o nazwie `events` (jeden wpis = jedno zderzenie). Gałęzie są wektorami odpowiadającymi kolejnym cząstkom końcowym w danym zdarzeniu:

| Gałąź | Typ | Opis |
|---|---|---|
| `id` | `vector<int>` | kod PDG (±211 piony, ±321 kaony, ±2212 protony, ...) |
| `charge` | `vector<int>` | ładunek elektryczny (−1, 0, +1) |
| `pt` | `vector<double>` | pęd poprzeczny p_T [GeV] |
| `eta` | `vector<double>` | pseudorapidity η |
| `phi` | `vector<double>` | kąt azymutalny φ [rad], zakres (−π, π] |

Dodatkowo w pliku zapisywany jest obiekt `production_info` (`TNamed`) z informacją o użytym programie, seedzie, liczbie wątków, liczbie zdarzeń i komendzie pozwalającej odtworzyć produkcję:

```cpp
TFile f("events.root");
f.Get("production_info")->Print();
```

## Kompilacja i uruchamianie

```bash
make                  # kompiluje oba generatory (wymaga Pythii >= 8.309)
make run              # uruchamia generator wielowątkowy -> events.root
make run_single       # uruchamia generator jednowątkowy -> events.root
make clean            # usuwa pliki wykonywalne i logi
```

Liczbę zdarzeń można zmienić parametrem `N`, np. `make run N=1000`, albo uruchamiając programy bezpośrednio:

```bash
./generate [liczba_zdarzen] [liczba_watkow]
./generate_single_thread [liczba_zdarzen]
```

Cel `make benchmark` uruchamia obie wersje po kolei i wypisuje zestawienie czasów wykonania.

## Porównanie wersji jedno- i wielowątkowej

Ponieważ zderzenia są od siebie niezależne, `generate.cc` zrównolegla generację za pomocą `PythiaParallel`. Dla każdego wątku `k` tworzona jest osobna instancja Pythii z seedem `12345 + k`, a opcja `Parallelism:balanceLoad = on` przydziela każdemu wątkowi stałą liczbę zdarzeń (`N / liczba_watkow`). Callback wypełniający `TTree` jest wywoływany przez Pythię sekwencyjnie (`Parallelism:processAsync = off`), dzięki czemu zapis do drzewa ROOT nie powoduje wyścigów danych.

Pomiar na procesorze Intel Core i7-4510U (2 rdzenie / 4 wątki) dla 100 000 zdarzeń:

| | 1 wątek | 4 wątki |
|---|---|---|
| Inicjalizacja Pythii | ~36 s | ~68 s |
| Wydajność generacji | ~280 zdarzeń/s | ~650 zdarzeń/s |

Inicjalizacja wersji wielowątkowej trwa dłużej (każda instancja inicjalizuje się osobno), ale sama generacja jest ok. 2.3× szybsza, co przy 100 000 zdarzeń daje wyraźny zysk czasowy.

### Reprodukowalność

- `generate_single_thread` przy stałym seedzie daje zawsze identyczny plik `events.root` (włącznie z kolejnością zdarzeń).
- `generate` przy tej samej liczbie wątków generuje dokładnie ten sam zbiór zdarzeń (każdy wątek ma własny seed i stałą liczbę zdarzeń do wygenerowania). Kolejność wpisów w `TTree` może się różnić między uruchomieniami, ale wynikowe histogramy w `analysis.root` są identyczne. Aby odtworzyć ten sam zbiór zdarzeń na komputerze z inną liczbą rdzeni, należy przekazać liczbę wątków jako drugi argument (np. `./generate 100000 4`).

## Uwagi techniczne

- W `Makefile` ustawiono `-std=c++17`, jednak `root-config --cflags` może dopisać flagę standardu, z jakim zbudowano ROOT (np. `-std=c++20`), aby zachować zgodność ABI z nagłówkami ROOT.
- `SoftQCD:all` obejmuje również zderzenia elastyczne (dwa protony o bardzo dużym |η|, poza zakresem histogramów).
- Kąt φ z Pythii jest w przedziale (−π, π]. Wymagany w zadaniu zakres (0, π) będzie obsługiwany na etapie analizy.
