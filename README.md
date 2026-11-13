# Symulacja i analiza rozkładów cząstek dla pp @ √s = 13.6 TeV (Pythia 8 + ROOT)

Projekt w ramach zadania wstępnego z analizy danych (treść: `Zadanie wstępne z analizy danych.pdf`). Generujemy 100 000 zderzeń proton–proton przy √s = 13.6 TeV w Pythii 8, w C++/ROOT liczymy histogramy krotności, p_T, η i φ dla cząstek stanu końcowego (`analysis.root`), a na koniec rysujemy rozkłady makrem `draw_plots.C`.

## Spis plików

| Plik | Co robi |
|---|---|
| `generate.cc` | generator wielowątkowy (`Pythia8::PythiaParallel`) → `events.root` |
| `generate_single_thread.cc` | ten sam generator w wersji jednowątkowej (zwykła pętla `pythia.next()`) → `events.root` |
| `analyze.cc` | czyta `events.root`, wypełnia histogramy i zapisuje je do `analysis.root` |
| `draw_plots.C` | makro ROOT rysujące histogramy z `analysis.root` i eksportujące je do `plots/` (`.png` + `.pdf`) |
| `Makefile` | kompilacja, uruchamianie i benchmark |
| `analysis.root` | gotowy wynik analizy dla 100 000 zdarzeń (seed 12345) |
| `plots/` | wygenerowane wykresy (`dist_N.png`, `dist_pT.png`, `dist_eta.png`, `dist_phi.png`, `all_distributions.pdf`) |

`events.root` nie jest w repozytorium (ok. 350 MB dla 100k zdarzeń, nie mieścił się na github)

## Wymagania

- kompilator C++17 (`g++` / `clang++`)
- ROOT 6 (testowane na 6.40)
- Pythia 8 (testowane na 8.312), do wersji wielowątkowej potrzebna **≥ 8.309** (klasa `PythiaParallel`)
- `make`

Programy `root-config` i `pythia8-config` muszą być w `PATH`, bo `Makefile` bierze z nich flagi kompilacji. Najprościej postawić wszystko przez condę:

```bash
conda create -n hep -c conda-forge root pythia8
conda activate hep
pythia8-config --version   # sprawdzenie wersji Pythii
root-config --version
```

## Uruchamianie

### Szybki start (pełny przebieg)

```bash
make              # kompiluje generate, generate_single_thread i analyze
make run          # etap 1: generuje 100 000 zdarzeń (wielowątkowo) -> events.root
make analysis     # etap 2: analiza events.root -> analysis.root
make plots        # etap 3: rysuje wykresy z analysis.root -> plots/
```

Na Apple M5 (10 wątków) generacja 100k zdarzeń trwa ok. pół minuty, analiza kilka sekund, a wygenerowanie wykresów poniżej 2 sekund.

### Wszystkie cele `Makefile`

| Komenda | Co robi | Odpowiednik bez `make` |
|---|---|---|
| `make` | kompiluje wszystkie 3 programy | – |
| `make run` | generacja wielowątkowa | `./generate 100000` |
| `make run_single` | generacja jednowątkowa | `./generate_single_thread 100000` |
| `make analysis` | analiza | `./analyze events.root analysis.root` |
| `make plots` | rysowanie wykresów PNG i PDF | `root -l -b -q 'draw_plots.C("analysis.root")'` |
| `make benchmark` | porównanie czasów obu generatorów | – |
| `make clean` | usuwa binarki i logi benchmarku (pliki `.root` i `plots/` zostają) | – |

Liczbę zdarzeń zmienia się zmienną `N` (**bez spacji** wokół `=`), np. szybki test:

```bash
make run N=1000
make benchmark N=10000
```

Programy i makro można też wołać bezpośrednio:

```bash
./generate [liczba_zdarzen] [liczba_watkow]   # domyślnie 100000 i wszystkie wątki CPU
./generate_single_thread [liczba_zdarzen]     # domyślnie 100000
./analyze [plik_wejsciowy] [plik_wyjsciowy]   # domyślnie events.root i analysis.root
root -l -b -q draw_plots.C                    # domyślnie czyta analysis.root, zapisuje do plots/
```

Oba generatory zapisują do `events.root` w tym samym formacie, więc `analyze` działa z każdym z nich. Pliki wyjściowe otwierane są w trybie `RECREATE`, czyli kolejne uruchomienie po prostu je nadpisuje. Wyjątkiem jest `make benchmark`, który liczy w tymczasowym katalogu i **nie rusza** głównego `events.root`.


### Sprawdzenie wyników

```bash
root -l analysis.root
root [1] .ls                 // lista 32 histogramów
root [2] hPt_pip->Draw()
root [3] new TBrowser        // albo przeglądanie w GUI
```

Parametry produkcji zapisane są w `events.root` w obiekcie `production_info`:

```bash
root -l -b -q -e 'TFile f("events.root"); f.Get("production_info")->Print();'
```

## Jak rozwiązujemy zadanie

### Etap 1 – generacja (`generate.cc`, `generate_single_thread.cc`)

Ustawienia Pythii wzięte prosto z treści zadania:

```
Beams:idA = 2212, Beams:idB = 2212   # proton–proton
Beams:eCM = 13600.                    # √s = 13.6 TeV
SoftQCD:all = on                      # minimum bias
Random:setSeed = on, Random:seed = 12345
```

Dla każdego zdarzenia przechodzimy po `pythia.event` i zapisujemy **tylko cząstki z `isFinal()`** – w rekordzie zdarzenia są też partony, rezonanse i inne cząstki pośrednie, które by się podwójnie liczyły. Ponieważ filtr jest już przy generacji, w `events.root` w ogóle nie ma cząstek pośrednich.

Format `events.root` – drzewo `TTree` o nazwie `events`, jeden wpis = jedno zderzenie, każda gałąź to wektor po cząstkach końcowych:

| Gałąź | Typ | Opis |
|---|---|---|
| `id` | `vector<int>` | kod PDG |
| `charge` | `vector<int>` | ładunek (zaokrąglony do liczby całkowitej) |
| `pt` | `vector<double>` | p_T [GeV] |
| `eta` | `vector<double>` | η |
| `phi` | `vector<double>` | φ [rad], zakres (−π, π] |

Do pliku dopisujemy też `production_info` (`TNamed`) z nazwą programu, seedem, liczbą wątków, liczbą zdarzeń i gotową komendą do odtworzenia produkcji – żeby po czasie było wiadomo, skąd dany plik się wziął.

### Etap 2 i 3 – analiza (`analyze.cc`)

`analyze` czyta drzewo zdarzenie po zdarzeniu i dla 8 kategorii wypełnia po 4 histogramy `TH1D` (razem 32):

| Sufiks | Kategoria | Warunek |
|---|---|---|
| `all` | wszystkie cząstki końcowe | – |
| `ch` | wszystkie naładowane | `charge != 0` |
| `pip` / `pim` | π⁺ / π⁻ | PDG `211` / `-211` |
| `Kp` / `Km` | K⁺ / K⁻ | PDG `321` / `-321` |
| `p` / `pbar` | p / p̄ | PDG `2212` / `-2212` |

| Histogram | Zmienna | Biny i zakres |
|---|---|---|
| `hN_*` | krotność w zdarzeniu | 400, [0, 400) |
| `hPt_*` | p_T [GeV] | 100, [0, 10) |
| `hEta_*` | η | 100, [−5, 5) |
| `hPhi_*` | φ [rad] | 64, [0, π] |

Kilka rzeczy, na które trzeba było uważać:

- **Krotność `hN_*`** – licznik cząstek danej kategorii zerujemy przed każdym zderzeniem i dopisujemy do histogramu na końcu pętli po cząstkach. Dzięki temu uwzględniamy też zdarzenia, w których dana cząstka nie pojawiła się ani razu (`N = 0`).
- **Wiele kategorii naraz** – jedna cząstka może należeć do kilku grup jednocześnie (np. `π⁺` wpada do `all`, `ch` oraz `pip`), dlatego warunki sprawdzane są osobno, a nie przez `else if`.
- **Zakres kąta φ** – Pythia zwraca φ w przedziale `(−π, π]`, a w zadaniu histogram ma zakres `[0, π]`. Zderzenia są symetryczne wokół wiązki, więc bierzemy wartość bezwzględną `|φ|` (funkcja `foldPhi`), dzięki czemu nie odrzucamy połowy danych z `φ < 0`.
- **Zdarzenia powyżej `N = 400` (`overflow`)** – dla `hN_all` w ok. 10% zderzeń łączna liczba wszystkich cząstek przekracza 400 (dla samych naładowanych `hN_ch` to tylko kilkadziesiąt przypadków na 100k). Zakres `0–400` jest narzucony w zadaniu, więc go nie zmieniamy, a program tylko wypisuje na końcu informację o takich zdarzeniach.

### Etap 4 – wizualizacja (`draw_plots.C`)

Makro `draw_plots.C` otwiera `analysis.root` i dla każdej z 4 zmiennych (`N`, `p_T`, `η`, `φ`) tworzy osobny `TCanvas` podzielony na dwa panele:
- **lewy panel**: porównanie wszystkich cząstek końcowych (`all`) i naładowanych (`ch`),
- **prawy panel**: porównanie zidentyfikowanych hadronów (`π⁺`, `π⁻`, `K⁺`, `K⁻`, `p`, `p̄` – pary ciemniejszy/jaśniejszy odcień dla cząstki i antycząstki).

Wykresy zapisywane są do katalogu `plots/` jako osobne pliki PNG (`dist_N.png`, `dist_pT.png`, `dist_eta.png`, `dist_phi.png`) oraz wspólny wielostronicowy PDF (`all_distributions.pdf`).

Krótki opis otrzymanych wykresów:

- **Krotność (`dist_N.png`)** – ponieważ zderzamy dwa protony (łączny ładunek `+2`), z zasady zachowania ładunku liczba wszystkich wyprodukowanych cząstek naładowanych `N_ch` w zdarzeniu jest zawsze parzysta (`2, 4, 6, ...`), a nieparzyste biny mają wartość `0` (dlatego na wykresie logarytmicznym linia `ch` spada co 2 biny do zera). Wysoki pik dla `N_ch = 2` pochodzi ze zderzeń elastycznych, w których dwa protony po prostu odbijają się od siebie.
- **Pęd poprzeczny (`dist_pT.png`)** – większość cząstek powstaje z małym pędem poprzecznym (poniżej 1–2 GeV), a dla wyższych pędów rozkład szybko opada. Widać też, że najwięcej produkuje się najlżejszych pionów `π±`, mniej kaonów `K±`, a najmniej cięższych protonów i antyprotonów.
- **Pseudorapidity (`dist_eta.png`)** – rozkład jest symetryczny względem `η = 0`, bo zderzamy dwa takie same protony o równych energiach. Lekkie obniżenie na samym środku wokół `η = 0` to typowy kształt rozkładu pseudorapidity dla cząstek mających masę.
- **Kąt azymutalny (`dist_phi.png`)** – rozkład jest płaski, bo żaden kierunek wokół osi wiązki nie jest wyróżniony. Na prawym panelu widać, że protonów `p` jest trochę więcej niż antyprotonów `p̄` (i minimalnie więcej `π⁺` niż `π⁻`), co wynika z dodatniego ładunku dwóch zderzanych protonów na wejściu.

## Skąd wielowątkowość i benchmark

Na początku był tylko `generate_single_thread.cc` – najprostsza wersja, wzorowana na przykładach z Pythii. Przy pierwszych testach wyszło, że **wąskim gardłem jest sama generacja w Pythii**: jeden wątek daje ok. 1600 zdarzeń/s na M5, a na starszym laptopie dużo mniej, więc 100k zdarzeń to od ponad minuty do kilku minut. Każda poprawka w generatorze oznaczała kolejne czekanie. Sama analiza 100k zdarzeń trwa kilka sekund, więc tam nie było czego przyspieszać.

Zderzenia są od siebie całkowicie niezależne, więc generacja to idealny kandydat do zrównoleglenia. Pythia od wersji 8.309 ma do tego gotową klasę `PythiaParallel` (dokumentacja „Parallelism”), więc nie trzeba było pisać własnych wątków:

- każdy wątek `k` ma własną instancję Pythii z seedem `12345 + k`,
- `Parallelism:balanceLoad = on` daje każdemu wątkowi stałą liczbę zdarzeń, więc zbiór zdarzeń zależy tylko od seeda i liczby wątków,
- callback zapisujący do `TTree` jest domyślnie wywoływany synchronicznie (tylko jeden naraz), więc zapis do drzewa jest bezpieczny bez własnych mutexów; dodatkowo włączamy `ROOT::EnableThreadSafety()`.

Wersję jednowątkową zostawiliśmy celowo: działa też na starszych Pythiach (bez `PythiaParallel`), daje identyczny plik przy każdym uruchomieniu i służy jako punkt odniesienia. Żeby uczciwie sprawdzić, czy wielowątkowość w ogóle się opłaca, oba programy mierzą czas inicjalizacji, generacji i zapisu, a `make benchmark` uruchamia je jeden po drugim na tej samej liczbie zdarzeń i wypisuje podsumowanie (pełne logi w `benchmark_single.log` i `benchmark_multi.log`).

### Wyniki benchmarku (100 000 zdarzeń, `make benchmark`)

**MacBook Air M5** – 10 rdzeni CPU (4 wydajne + 6 energooszczędnych), 10 wątków:

| | `generate_single_thread` (1 wątek) | `generate` (10 wątków) | Przyspieszenie |
|---|---:|---:|---:|
| Inicjalizacja Pythii | 8.25 s | 14.18 s | 0.6× |
| Generacja | 61.26 s | 16.12 s | **3.8×** |
| Zapis `events.root` | 0.10 s | 0.04 s | – |
| **Razem** | **69.61 s** | **30.34 s** | **2.3×** |
| Wydajność generacji | ~1630 zdarzeń/s | ~6200 zdarzeń/s | |

**Wnioski:**

- Inicjalizacja wersji wielowątkowej trwa dłużej, bo każda instancja Pythii (po jednej na wątek) inicjalizuje się osobno. Przy małej liczbie zdarzeń (np. `N=1000`) wersja jednowątkowa jest przez to nawet szybsza.
- Przy docelowych 100k zdarzeń zysk jest wyraźny: cały przebieg skraca się z ~70 s do ~30 s.
- Przyspieszenie generacji (3.8×) jest mniejsze niż liczba wątków (10). Tylko 4 rdzenie to rdzenie wydajne, a pozostałe 6 to wolniejsze rdzenie energooszczędne. Do tego MacBook Air nie ma wentylatora i przy dłuższym obciążeniu najpewniej zbija taktowanie (dla `N=10000` wyszło ~7600 zdarzeń/s, a dla 100k ~6200 zdarzeń/s).

## Reprodukowalność

- Stały seed (`12345`) w obu generatorach.
- `generate_single_thread N` daje za każdym razem identyczny `events.root`, łącznie z kolejnością zdarzeń.
- `generate N T` przy tej samej liczbie wątków `T` daje ten sam zbiór zdarzeń – może się zmienić tylko kolejność wpisów w `TTree`, co nie ma wpływu na histogramy. Na innej maszynie trzeba podać liczbę wątków jawnie, np. `./generate 100000 10`.
- Pełna komenda do odtworzenia jest zapisana w `production_info` w `events.root`.
- Dołączony `analysis.root` powstał z `make run` (czyli `./generate 100000 10` na Apple M5) + `make analysis`.

