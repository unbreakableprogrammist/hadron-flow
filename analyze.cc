// ============================================================================
// Program analizujacy zderzenia pp @ sqrt(s) = 13.6 TeV (etap 2 i 3 zadania).
//
// Co robi ten program:
//   1. Otwiera plik ROOT (domyslnie "events.root") wygenerowany w etapie 1
//      przez program `generate` lub `generate_single_thread`.
//   2. Wczytuje z niego drzewo TTree o nazwie "events", w ktorym kazdy wpis
//      odpowiada jednemu zderzeniu proton-proton i zawiera wylacznie czastki
//      stanu koncowego (odfiltrowane przez p.isFinal() juz w Pythii).
//   3. Dla 8 kategorii czastek tworzy po 4 histogramy:
//        - hN_*   : krotnosc czastek w zdarzeniu (ile czastek na zderzenie)
//        - hPt_*  : rozklad pedu poprzecznego p_T [GeV]
//        - hEta_* : rozklad pseudorapidity eta
//        - hPhi_* : rozklad kata azymutalnego phi [rad] w zakresie [0, pi]
//   4. Zapisuje wszystkie 32 histogramy do pliku wyjsciowego ("analysis.root").
//
// Uzycie z terminala:
//   ./analyze                                 -> czyta events.root, zapisuje analysis.root
//   ./analyze inny_plik.root wynik.root       -> podanie wlasnych nazw plikow
// ============================================================================

// --- Standardowe biblioteki C++ ---
#include <iostream>   // wypisywanie komunikatow na ekran (std::cout, std::cerr)
#include <string>     // typ tekstowy std::string
#include <vector>     // dynamiczna tablica std::vector (tak sa zapisane czastki w drzewie)
#include <cmath>      // funkcje matematyczne: std::fabs (wartosc bezwzgledna), std::nextafter
#include <algorithm>  // std::max

// --- Naglowki biblioteki ROOT ---
#include "TFile.h"    // obsluga plikow .root (odczyt i zapis)
#include "TTree.h"    // obsluga drzewa TTree (tabelaryczna struktura danych w ROOT)
#include "TH1D.h"     // histogram 1D o podwojnej precyzji (Double_t w kazdym binie)
#include "TMath.h"    // stale i funkcje matematyczne ROOT, m.in. TMath::Pi()

// Przestrzen nazw grupujaca stale opisujace osie histogramow (liczba binow i zakresy).
// Dzieki temu wszystkie 8 kategorii korzysta z dokladnie tych samych ustawien z zadania.
namespace binning {
    // constexpr = stala znana juz podczas kompilacji programu
    constexpr int    nBinsN   = 400;  constexpr double minN   = 0.0;  constexpr double maxN   = 400.0;
    constexpr int    nBinsPt  = 100;  constexpr double minPt  = 0.0;  constexpr double maxPt  = 10.0;
    constexpr int    nBinsEta = 100;  constexpr double minEta = -5.0; constexpr double maxEta = 5.0;
    constexpr int    nBinsPhi = 64;   constexpr double minPhi = 0.0;
    // TMath::Pi() jest funkcja, dlatego tu uzywamy `const` zamiast `constexpr`
    const     double maxPhi = TMath::Pi();
}

// Struktura (paczka danych i metod) reprezentujaca komplet 4 histogramow dla JEDNEJ kategorii
// czastek (np. tylko dla pionow pi+ albo dla wszystkich czastek naladowanych).
// Zamiast powtarzac deklaracje 32 zmiennych recznie, tworzymy 8 obiektow tej struktury.
struct CategoryHistos {
    // Wskazniki (`*`) na histogramy ROOT. `nullptr` oznacza "na razie nie wskazuje na zaden obiekt".
    TH1D* hN   = nullptr; // histogram krotnosci N w zderzeniu
    TH1D* hPt  = nullptr; // histogram pedu poprzecznego pT
    TH1D* hEta = nullptr; // histogram pseudorapidity eta
    TH1D* hPhi = nullptr; // histogram kata azymutalnego phi

    // Licznik tymczasowy: zlicza ile czastek TEJ kategorii pojawilo sie w biezacym zderzeniu.
    // Na poczatku kazdego zderzenia go zerujemy, w trakcie petli po czastkach zwiekszamy,
    // a na koniec zderzenia jego wartosc wrzucamy do histogramu hN.
    int countInEvent = 0;

    // Konstruktor: uruchamia sie automatycznie przy tworzeniu obiektu CategoryHistos.
    // Parametry:
    //   suffix - krotka nazwa do identyfikatora histogramu (np. "pip" -> da hN_pip, hPt_pip...)
    //   label  - ladny opis do tytulu wykresu w skladni ROOT TLatex (np. "#pi^{+}")
    CategoryHistos(const std::string& suffix, const std::string& label) {
        using namespace binning; // pozwala pisac nBinsN zamiast binning::nBinsN

        // `new TH1D(...)` tworzy histogram w pamieci.
        // Drugi argument w ROOT ma specjalny format oddzielony srednikami:
        //   "Tytul glowny wykresu;Podpis osi X;Podpis osi Y"
        // Znaki `#` i `{}` to oznaczenia matematyczne ROOT (np. `#phi` -> grecka litera phi,
        // `p_{T}` -> p z indeksem dolnym T, `#bar{p}` -> p z kreska nad litera).
        hN   = new TH1D(("hN_"   + suffix).c_str(), ("Multiplicity (" + label + ");Multiplicity N;Events").c_str(),
                        nBinsN, minN, maxN);
        hPt  = new TH1D(("hPt_"  + suffix).c_str(), ("Transverse momentum (" + label + ");p_{T} [GeV];Particles").c_str(),
                        nBinsPt, minPt, maxPt);
        hEta = new TH1D(("hEta_" + suffix).c_str(), ("Pseudorapidity (" + label + ");#eta;Particles").c_str(),
                        nBinsEta, minEta, maxEta);
        hPhi = new TH1D(("hPhi_" + suffix).c_str(), ("Azimuthal angle (" + label + ");#phi [rad];Particles").c_str(),
                        nBinsPhi, minPhi, maxPhi);
    }

    // Wywolywane przed analizowaniem kolejnego zderzenia
    void resetEventCounter() { countInEvent = 0; }

    // Wywolywane dla kazdej pojedynczej czastki pasujacej do tej kategorii:
    // podbija licznik czastek w tym zdarzeniu i dopisuje jej pT, eta, phi do histogramow.
    void fillParticle(double pt, double eta, double phi) {
        ++countInEvent;
        hPt->Fill(pt);
        hEta->Fill(eta);
        hPhi->Fill(phi);
    }

    // Wywolywane raz na koniec zderzenia: zapisuje ostateczna liczbe czastek tej kategorii
    // w tym konkretnym zderzeniu (rowniez gdy countInEvent == 0, co jest wazne!).
    void fillEventMultiplicity() { hN->Fill(countInEvent); }

    // Zapisuje wszystkie 4 histogramy tej kategorii do aktualnie wybranego pliku ROOT.
    void write() {
        hN->Write();
        hPt->Write();
        hEta->Write();
        hPhi->Write();
    }
};

// Funkcja pomocnicza dopasowujaca kat phi do zakresu [0, pi] wymaganego przez zadanie.
//
// Dlaczego to robimy:
//   - Pythia zapisuje phi w przedziale (-pi, pi] (pelne kolo 360 stopni).
//   - W zadaniu histogram hPhi ma miec zakres 0 - pi (polkola).
//   - Poniewaz zderzenia proton-proton sa symetryczne wokol osi wiazki, rozklad dla
//     phi < 0 i phi > 0 jest taki sam. Zamiast wyrzucac polowe danych, bierzemy
//     wartosc bezwzgledna |phi| ("skladamy" (-pi, 0) na (0, pi)).
//   - Dodatkowy niuans ROOT: biny histogramu sa przedzialami lewostronnie domknietymi
//     [min, max), tzn. wartosc rowna dokladnie max (tutaj pi) wpadlaby juz poza ostatni
//     bin (do tzw. overflow). Funkcja std::nextafter(pi, 0.0) zwraca najwieksza liczbe
//     double minimalnie mniejsza od pi, dzieki czemu czastki z phi == pi trafiaja
//     do ostatniego binu histogramu zamiast do overflow.
inline double foldPhi(double phi) {
    double a = std::fabs(phi);
    if (a >= TMath::Pi()) a = std::nextafter(TMath::Pi(), 0.0);
    return a;
}

// Glowna funkcja programu.
// `argc` = liczba argumentow z linii polecen (lacznie z nazwa programu).
// `argv` = tablica napisow z tymi argumentami: argv[0] to "./analyze", argv[1] opcjonalny plik wejsciowy, itd.
int main(int argc, char* argv[]) {
    // Operator trojargumentowy `warunek ? gdy_prawda : gdy_falsz`:
    // jesli uzytkownik podal nazwe pliku przy uruchomieniu, uzywamy jej; w przeciwnym razie domyslnej.
    const std::string inFileName  = (argc > 1) ? argv[1] : "events.root";
    const std::string outFileName = (argc > 2) ? argv[2] : "analysis.root";

    // ========================================================================
    // KROK 1: Otwarcie pliku wejsciowego i pobranie drzewa TTree "events"
    // ========================================================================
    TFile* inFile = TFile::Open(inFileName.c_str(), "READ");
    // IsZombie() zwraca true, jesli plik jest uszkodzony albo nie istnieje
    if (!inFile || inFile->IsZombie()) {
        std::cerr << "Blad: Nie mozna otworzyc pliku " << inFileName << std::endl;
        return 1; // niezerowy kod powrotu oznacza blad programu
    }

    // Wyciagamy z pliku obiekt typu TTree o nazwie "events"
    TTree* tree = inFile->Get<TTree>("events");
    if (!tree) {
        std::cerr << "Blad: Brak drzewa 'events' w pliku " << inFileName << std::endl;
        return 1;
    }

    // ========================================================================
    // KROK 2: Podpiecie zmiennych pod galezie (kolumny) drzewa TTree
    // ========================================================================
    // W drzewie "events" kazdy wiersz to 1 zderzenie, a kolumny ("galezie") to wektory
    // o dlugosci rownej liczbie czastek koncowych w tym zderzeniu.
    //
    // ROOT przy odczycie wektorow wymaga wskaznikow na std::vector zainicjalizowanych
    // jako `nullptr` — wtedy przy pierwszym wywolaniu GetEntry() sam utworzy wektory
    // w pamieci i bedzie je wypelnial dla kolejnych zdarzen.
    std::vector<int>*    p_id     = nullptr; // kody PDG czastek
    std::vector<int>*    p_charge = nullptr; // ladunki elektryczne (-1, 0, +1)
    std::vector<double>* p_pt     = nullptr; // pedy poprzeczne [GeV]
    std::vector<double>* p_eta    = nullptr; // pseudorapidity
    std::vector<double>* p_phi    = nullptr; // katy azymutalne (-pi, pi]

    // Bezpieczenstwo: upewniamy sie, ze plik faktycznie posiada wszystkie wymagane galezie
    for (const char* br : {"id", "charge", "pt", "eta", "phi"}) {
        if (!tree->GetBranch(br)) {
            std::cerr << "Blad: Brak galezi '" << br << "' w drzewie 'events'" << std::endl;
            return 1;
        }
    }

    // Laczymy nazwy galezi w pliku z adresami naszych wskaznikow (`&p_id` to adres wskaznika)
    tree->SetBranchAddress("id",     &p_id);
    tree->SetBranchAddress("charge", &p_charge);
    tree->SetBranchAddress("pt",     &p_pt);
    tree->SetBranchAddress("eta",    &p_eta);
    tree->SetBranchAddress("phi",    &p_phi);

    // ========================================================================
    // KROK 3: Utworzenie pliku wyjsciowego i 8 zestawow histogramow
    // ========================================================================
    // Tryb "RECREATE" tworzy nowy plik (lub nadpisuje istniejacy o tej samej nazwie).
    TFile* outFile = TFile::Open(outFileName.c_str(), "RECREATE");
    if (!outFile || outFile->IsZombie()) {
        std::cerr << "Blad: Nie mozna utworzyc pliku " << outFileName << std::endl;
        return 1;
    }

    // W ROOT istnieje pojecie "aktualnego katalogu" (gDirectory).
    // Histogramy stworzone po wywolaniu `outFile->cd()` zostaja automatycznie przypisane
    // do tego pliku wyjsciowego (nie do pliku wejsciowego ani do pamieci globalnej).
    outFile->cd();

    // Tworzymy 8 kategorii wymaganych przez zadanie (kazda ma 4 histogramy -> razem 32)
    CategoryHistos cat_all ("all",  "all final-state particles"); // wszystkie czastki koncowe
    CategoryHistos cat_ch  ("ch",   "charged particles");         // wszystkie naladowane (charge != 0)
    CategoryHistos cat_pip ("pip",  "#pi^{+}");                   // piony dodatnie  (PDG  211)
    CategoryHistos cat_pim ("pim",  "#pi^{-}");                   // piony ujemne    (PDG -211)
    CategoryHistos cat_Kp  ("Kp",   "K^{+}");                     // kaony dodatnie  (PDG  321)
    CategoryHistos cat_Km  ("Km",   "K^{-}");                     // kaony ujemne    (PDG -321)
    CategoryHistos cat_p   ("p",    "p");                         // protony         (PDG  2212)
    CategoryHistos cat_pbar("pbar", "#bar{p}");                   // antyprotony     (PDG -2212)

    // Zbieramy adresy wszystkich 8 obiektow do jednego wektora, zeby moc wykonywac
    // powtarzalne czynnosci (zerowanie, zapis krotnosci, zapis na dysk) w krotkiej petli.
    std::vector<CategoryHistos*> allCategories = {
        &cat_all, &cat_ch, &cat_pip, &cat_pim, &cat_Kp, &cat_Km, &cat_p, &cat_pbar
    };

    // ========================================================================
    // KROK 4: Glowna petla po wszystkich zdarzeniach (zderzeniach)
    // ========================================================================
    const Long64_t nEntries = tree->GetEntries(); // calkowita liczba zdarzen w drzewie
    // Chcemy wypisywac postep co 10% analizy; std::max(..., 1) chroni przed dzieleniem przez 0,
    // gdyby w pliku bylo mniej niz 10 zdarzen.
    const Long64_t printEvery = std::max<Long64_t>(1, nEntries / 10);
    std::cout << "Analiza " << nEntries << " zdarzen z pliku " << inFileName << std::endl;

    for (Long64_t i = 0; i < nEntries; ++i) {
        // Wczytuje i-te zdarzenie z pliku ROOT do naszych wektorow (*p_id, *p_pt itd.)
        tree->GetEntry(i);

        // Przed przejrzeniem czastek tego zderzenia zerujemy liczniki krotnosci dla kazdej kategorii
        for (auto* cat : allCategories) cat->resetEventCounter();

        // Liczba czastek koncowych powstalych w tym konkretnym zderzeniu
        const size_t nParticles = p_id->size();

        // Petla wewnetrzna: przechodzimy po kazdej czastce j w i-tym zderzeniu
        for (size_t j = 0; j < nParticles; ++j) {
            // `p_id` to wskaznik na wektor, wiec `(*p_id)[j]` oznacza j-ty element tego wektora
            const int    id     = (*p_id)[j];
            const int    charge = (*p_charge)[j];
            const double pt     = (*p_pt)[j];
            const double eta    = (*p_eta)[j];
            const double phi    = foldPhi((*p_phi)[j]); // skladamy phi z (-pi, pi] do [0, pi]

            // Uwaga: jedna czastka moze nalezec do kilku kategorii jednoczesnie!
            // Np. pion pi+ trafi do `all`, do `ch` (bo ma ladunek +1) oraz do `pip` (PDG 211).

            // 1) Wszystkie czastki stanu koncowego (isFinal() bylo juz sprawdzone przy generacji)
            cat_all.fillParticle(pt, eta, phi);

            // 2) Wszystkie czastki naladowane (neutralne, np. fotony czy neutrony, maja charge == 0)
            if (charge != 0) cat_ch.fillParticle(pt, eta, phi);

            // 3) Identyfikacja konkretnego gatunku czastki na podstawie jej numeru PDG
            switch (id) {
                case   211: cat_pip.fillParticle(pt, eta, phi);  break;
                case  -211: cat_pim.fillParticle(pt, eta, phi);  break;
                case   321: cat_Kp.fillParticle(pt, eta, phi);   break;
                case  -321: cat_Km.fillParticle(pt, eta, phi);   break;
                case  2212: cat_p.fillParticle(pt, eta, phi);    break;
                case -2212: cat_pbar.fillParticle(pt, eta, phi); break;
                default: break; // inne czastki (np. elektrony, miony, fotony, K0) nie maja osobnej kategorii
            }
        }

        // Po przejrzeniu wszystkich czastek w i-tym zderzeniu dopisujemy zliczona liczbe N
        // do histogramow krotnosci hN_* (nawet jesli w tym zderzeniu bylo np. 0 kaonow).
        for (auto* cat : allCategories) cat->fillEventMultiplicity();

        // Wypisanie postepu co ok. 10% zdarzen
        if ((i + 1) % printEvery == 0) {
            std::cout << "Przeanalizowano " << (i + 1) << " / " << nEntries << " zdarzen." << std::endl;
        }
    }

    // Sprawdzenie tzw. overflow w histogramach krotnosci:
    // W ROOT bin nr 0 to underflow (< min), biny 1..NbinsX to normalny zakres,
    // a bin nr NbinsX + 1 to overflow (>= max). Dla N >= 400 wypisujemy informacje.
    for (auto* cat : allCategories) {
        const double ovf = cat->hN->GetBinContent(cat->hN->GetNbinsX() + 1);
        if (ovf > 0) {
            std::cout << "Uwaga: " << cat->hN->GetName() << " ma " << ovf
                      << " zdarzen z N >= " << binning::maxN << " (overflow)." << std::endl;
        }
    }

    // ========================================================================
    // KROK 5: Zapis histogramow na dysk i posprzątanie zasobow
    // ========================================================================
    outFile->cd(); // upewniamy sie, ze zapisujemy do pliku wyjsciowego
    for (auto* cat : allCategories) cat->write();

    // Zamkniecie plikow (Close() zapisuje metadane ROOT i zwalnia powiazane histogramy z pamieci)
    outFile->Close();
    inFile->Close();
    delete outFile;
    delete inFile;

    std::cout << "Gotowe! Histogramy zapisano w " << outFileName << std::endl;
    return 0; // 0 oznacza pomyslne zakonczenie programu
}
