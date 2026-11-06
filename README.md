# Symulacja i analiza rozkładów cząstek dla pp @ √s = 13.6 TeV (Pythia 8 + ROOT)

Projekt to łańcuch kroków od wygenerowania zderzeń proton–proton przy energii 13.6 TeV
generatorem Pythia 8, przez analizę cząstek stanu końcowego, po histogramy w ROOT.
Treść zadania jest w pliku `Zadanie wstępne z analizy danych.pdf`.

**Stan prac:** zrobiony jest punkt 1 zadania (generacja danych). Analiza (`analyze.cc`)
i makro rysujące (`draw_plots.C`) będą dodane później.

---

## Wymagania

- kompilator C++17 (g++)
- **Pythia 8** w wersji ≥ 8.309 (ze względu na `PythiaParallel`; testowano na 8.312), z dostępnym `pythia8-config`
- **ROOT 6** (testowano na 6.40), z dostępnym `root-config`
- `make`

Używane środowisko: conda (`miniforge3`, env `hep-env`). Przed kompilacją trzeba je aktywować:

```bash
conda activate hep-env
```

---

## Opis plików

| Plik | Opis |
|---|---|
| `generate.cc` | **Główny generator, wielowątkowy.** Używa `Pythia8::PythiaParallel` na wszystkich dostępnych wątkach procesora. Zapisuje wynik do `events.root`. |
| `generate_single_thread.cc` | **Generator jednowątkowy**: zwykła pętla `pythia.next()`. W pełni deterministyczny, czyli przy tym samym seedzie daje identyczny plik. Zapisuje wynik do `events_single.root`. |
| `Makefile` | Kompilacja, uruchamianie i benchmark obu generatorów. |
| `Zadanie wstępne z analizy danych.pdf` | Treść zadania. |
| `.gitignore` | Wyklucza pliki binarne, `*.root` (poza `analysis.root`, który trzeba oddać) i logi benchmarku. |

### Konfiguracja fizyki (wspólna dla obu generatorów)

| Ustawienie | Wartość | Znaczenie |
|---|---|---|
| `Beams:idA`, `Beams:idB` | `2212` | dwie wiązki protonów |
| `Beams:eCM` | `13600.` | √s = 13.6 TeV |
| `SoftQCD:all` | `on` | tryb *minimum bias* |
| `Random:setSeed` / `Random:seed` | `on` / `12345` | stały seed, żeby produkcję dało się odtworzyć |

Zapisywane są **wyłącznie cząstki stanu końcowego** (`particle.isFinal()`).

### Format pliku wyjściowego

Plik `events.root` (lub `events_single.root`) zawiera drzewo `TTree` o nazwie `events`.
**Jeden wpis odpowiada jednemu zderzeniu**, a gałęzie to wektory z danymi kolejnych cząstek końcowych:

| Gałąź | Typ | Opis |
|---|---|---|
| `id` | `vector<int>` | kod cząstki PDG (np. ±211 piony, ±321 kaony, ±2212 protony) |
| `charge` | `vector<int>` | ładunek elektryczny (−1, 0, +1) |
| `pt` | `vector<double>` | pęd poprzeczny p_T [GeV] |
| `eta` | `vector<double>` | pseudorapidity η |
| `phi` | `vector<double>` | kąt azymutalny φ [rad], zakres (−π, π] |

> Uwaga na potrzeby analizy: Pythia zwraca φ w zakresie (−π, π], a zadanie wymaga histogramu
> φ w zakresie 0–π. Trzeba to uwzględnić w `analyze.cc`.

---

## Kompilacja i uruchamianie

```bash
make                       # kompiluje oba generatory
make run                   # generator wielowątkowy     -> events.root
make run_single            # generator jednowątkowy     -> events_single.root
make benchmark             # uruchamia oba i wypisuje porównanie czasów
make clean                 # usuwa pliki binarne i logi benchmarku
```

Domyślnie generowanych jest **100 000 zdarzeń**. Liczbę można zmienić zmienną `N`:

```bash
make run N=1000
make benchmark N=5000
```

Programy można też uruchomić bezpośrednio, podając liczbę zdarzeń jako argument:

```bash
./generate 100000
./generate_single_thread 100000
```

`make benchmark` zapisuje pełne logi do `benchmark_single.log` i `benchmark_multi.log`.
Na ekranie wypisuje tylko zestawienie czasów.

Każdy program kończy pracę takim podsumowaniem:

```
===== BENCHMARK (generate) =====
Watki:              4
Zapisane zdarzenia: 500 / 500
Inicjalizacja:      67.565 s
Generacja:          0.765 s
Zapis pliku:        0.187 s
CALKOWITY CZAS:     68.517 s
Wydajnosc:          653.379 zdarzen/s
================================
```

---

## Przebudowa na wiele wątków: dlaczego i jak

### Dlaczego

Na początku generator był jednowątkowy. Przy 100 000 zdarzeń sama generacja trwa kilka minut,
a reszta rdzeni procesora w tym czasie nic nie robi. Zderzenia są **od siebie niezależne**,
więc można je generować równolegle (problem typu *embarrassingly parallel*).

### Jak

Pythia od wersji 8.309 ma klasę `Pythia8::PythiaParallel`, więc nie trzeba samemu tworzyć
wątków, mutexów ani łączyć plików:

- Liczbę wątków odczytuje się przez `std::thread::hardware_concurrency()`.
  Na Linuksie ta funkcja korzysta z `sysconf(_SC_NPROCESSORS_ONLN)` / `sched_getaffinity`.
- Każdy wątek dostaje **własną, niezależną instancję Pythii** z własnym seedem
  (`12345`, `12346`, ...), dzięki czemu wątki generują różne zdarzenia.
- Funkcja zapisująca zdarzenie do `TTree` (callback) jest przez Pythię **synchronizowana**
  (domyślnie `Parallelism:processAsync = off`). W danej chwili wywołuje ją tylko jeden wątek,
  więc wątki nie wchodzą sobie w drogę przy zapisie do `TTree`. Dodatkowo włączone jest
  `ROOT::EnableThreadSafety()`.

### Wyniki (Intel i7-4510U, 2 rdzenie / 4 wątki, N = 500)

| | 1 wątek | 4 wątki |
|---|---|---|
| Inicjalizacja | 36.3 s | 67.6 s |
| Generacja | 1.78 s (281 zdarzeń/s) | 0.77 s (653 zdarzeń/s) |
| Całość | 38.2 s | 68.5 s |

- **Generacja jest ~2.3× szybsza** na 4 wątkach. Procesor ma tylko 2 fizyczne rdzenie,
  dlatego przyspieszenie jest mniejsze niż 4×.
- **Inicjalizacja jest wolniejsza**, bo każda instancja Pythii inicjalizuje się osobno.
  Dlatego przy małej liczbie zdarzeń wersja wielowątkowa przegrywa. Przy docelowych
  100 000 zdarzeń szacunkowo: ~6.5 min (1 wątek) wobec ~3.7 min (4 wątki).

### Czy wynik da się odtworzyć?

- `generate_single_thread`: **tak, bit w bit.** Ten sam seed zawsze daje ten sam plik.
- `generate`: **statystycznie tak**, ale plik nie jest identyczny przy każdym uruchomieniu.
  Kolejność zdarzeń i to, który wątek wygeneruje ile zdarzeń, zależy od planisty systemu
  operacyjnego. Rozkłady fizyczne są równoważne.

Jeśli potrzebna jest pełna powtarzalność, użyj `generate_single_thread`.

---

## Uwagi techniczne

- `pythia8-config --cxxflags` dokłada `-std=c++11`, a `root-config --cflags` dokłada `-std=c++20`
  (ROOT w tym środowisku jest zbudowany w C++20). Ostatnia flaga wygrywa, więc kompilacja
  odbywa się w trybie C++20. Sam kod jest zgodny z C++17.
- `SoftQCD:all` obejmuje także zderzenia elastyczne. Dają one zdarzenia z dwoma protonami
  przy bardzo dużym |η|. Alternatywą jest `SoftQCD:inelastic`.
