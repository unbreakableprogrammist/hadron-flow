// Generator zderzen pp @ sqrt(s) = 13.6 TeV (Pythia 8) - wersja wielowatkowa.
// Uzywa wszystkich dostepnych watkow procesora (PythiaParallel).
// Reprodukowalnosc: przy tej samej liczbie watkow generowany jest dokladnie ten sam
// zbior zdarzen (watek k ma seed 12345+k i zawsze generuje N/liczba_watkow zdarzen),
// zmienia sie jedynie kolejnosc wierszy w TTree -> histogramy sa identyczne.
// Liczba watkow jest zapisywana w events.root; aby odtworzyc produkcje na innej
// maszynie, podaj ja jako drugi argument.
// Uzycie:  ./generate [liczba_zdarzen] [liczba_watkow]
//          (domyslnie 100 000 zdarzen, wszystkie dostepne watki)
// Wynik:   events.root (TTree "events")

#include "Pythia8/Pythia.h"
#include "Pythia8/PythiaParallel.h"
#include "TFile.h"
#include "TTree.h"
#include "TNamed.h"
#include "TROOT.h"
#include <vector>
#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>

int main(int argc, char* argv[]) {
    // liczenie czasu wykonania programu (benchmark)
    using Clock = std::chrono::steady_clock; 
    const auto tStart = Clock::now();

    const long nEvents = (argc > 1) ? std::stol(argv[1]) : 100000;
    const int  seed    = 12345;  // Staly seed -> kazdy watek dostaje seed, seed+1, ...

    // Liczba dostepnych watkow sprzetowych. Na Linuksie std::thread::hardware_concurrency()
    // korzysta pod spodem z sysconf(_SC_NPROCESSORS_ONLN) / sched_getaffinity.
    unsigned int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 1;  // Gdy system nie potrafi tego okreslic
    // Opcjonalne nadpisanie (odtworzenie produkcji wykonanej na innej liczbie watkow)
    if (argc > 2) nThreads = static_cast<unsigned int>(std::max(1, std::stoi(argv[2])));

    // ROOT bedzie wolany z roznych watkow (choc nigdy jednoczesnie)
    ROOT::EnableThreadSafety();

    // 1. Inicjalizacja rownoleglego generatora Pythia 8
    //    (kazdy watek ma wlasna, niezalezna instancje Pythii)
    Pythia8::PythiaParallel pythia;

    // 2. Konfiguracja fizyki zderzenia
    pythia.readString("Beams:idA = 2212");       // Wiazka A: proton (PDG 2212)
    pythia.readString("Beams:idB = 2212");       // Wiazka B: proton (PDG 2212)
    pythia.readString("Beams:eCM = 13600.");     // sqrt(s) = 13.6 TeV (13600 GeV)
    pythia.readString("SoftQCD:all = on");       // Procesy Minimum Bias (miekka chromodynamika)

    // Reprodukowalnosc: jawnie ustawiony seed generatora liczb losowych
    pythia.readString("Random:setSeed = on");
    pythia.readString("Random:seed = " + std::to_string(seed));

    // Liczba watkow = wszystkie dostepne
    pythia.readString("Parallelism:numThreads = " + std::to_string(nThreads));

    // Reprodukowalnosc: kazdy watek generuje stala liczbe zdarzen (N / liczba_watkow)
    // ze swoim seedem, niezaleznie od planisty OS (to jest wartosc domyslna,
    // ustawiamy ja jawnie, bo od niej zalezy powtarzalnosc produkcji).
    pythia.readString("Parallelism:balanceLoad = on");

    // Wyciszenie komunikatow diagnostycznych pojedynczych zdarzen
    pythia.readString("Next:numberCount = 0");
    pythia.readString("Next:numberShowInfo = 0");
    pythia.readString("Next:numberShowProcess = 0");
    pythia.readString("Next:numberShowEvent = 0");

    // Zatwierdzenie konfiguracji generatora
    if (!pythia.init()) {
        std::cerr << "Blad: Inicjalizacja Pythii nie powiodla sie!" << std::endl;
        return 1;
    }
    const auto tInit = Clock::now();

    // 3. Utworzenie binarnego pliku ROOT i drzewa kolumnowego TTree
    TFile* outFile = new TFile("events.root", "RECREATE");
    TTree* tree = new TTree("events", "pp collisions at 13.6 TeV");

    // Bufory na parametry czastek w jednym zdarzeniu
    std::vector<int>    p_id;      // Kody czastek wg standardu PDG
    std::vector<int>    p_charge;  // Ladunek elektryczny (-1, 0, +1)
    std::vector<double> p_pt;      // Ped poprzeczny pT [GeV]
    std::vector<double> p_eta;     // Pseudorapidity eta
    std::vector<double> p_phi;     // Kat azymutalny phi [rad], zakres (-pi, pi]

    // Podpiecie buforow pod galezie (kolumny) drzewa TTree
    tree->Branch("id",     &p_id);
    tree->Branch("charge", &p_charge);
    tree->Branch("pt",     &p_pt);
    tree->Branch("eta",    &p_eta);
    tree->Branch("phi",    &p_phi);

    // 4. Glowna petla generowania zderzen (rownolegle)
    std::cout << "Rozpoczynam generowanie " << nEvents << " zderzen ("
              << nThreads << " watkow)..." << std::endl;

    long nFilled = 0;
    const long printEvery = std::max(1L, nEvents / 10);

    // Zdarzenia generowane sa rownolegle, ale callback jest domyslnie
    // synchronizowany (Parallelism:processAsync = off), wiec w danej chwili
    // tylko jeden watek wypelnia bufory i TTree -> brak wyscigow.
    // Nieudane zdarzenia nie sa przekazywane do callbacku.
    pythia.run(nEvents, [&](Pythia8::Pythia* pyPtr) {
        const Pythia8::Event& event = pyPtr->event;

        // Czyszczenie wektorow przed przejsciem do nowej kolizji
        p_id.clear();
        p_charge.clear();
        p_pt.clear();
        p_eta.clear();
        p_phi.clear();

        // 5. Selekcja czastek powstalych w zderzeniu
        for (int i = 0; i < event.size(); ++i) {
            const auto& p = event[i];

            // Wymog fizyczny: bierzemy wylacznie czastki stanu koncowego
            if (!p.isFinal()) continue;

            p_id.push_back(p.id());
            p_charge.push_back(static_cast<int>(std::lround(p.charge())));
            p_pt.push_back(p.pT());
            p_eta.push_back(p.eta());
            p_phi.push_back(p.phi());
        }

        // Zapis calego zderzenia jako jednego wiersza w TTree
        tree->Fill();
        ++nFilled;

        if (nFilled % printEvery == 0) {
            std::cout << "Wygenerowano " << nFilled << " / " << nEvents << " zdarzen." << std::endl;
        }
    });
    const auto tGen = Clock::now();

    // 6. Finalizacja zapisu i zwolnienie zasobow
    pythia.stat();      // Podsumowanie przekrojow czynnych
    outFile->cd();
    tree->Write("", TObject::kOverwrite);  // Zrzut buforow TTree na dysk
    // Parametry produkcji zapisane w pliku -> mozna ja odtworzyc
    TNamed("production_info",
           "generator=generate (PythiaParallel); seed=" + std::to_string(seed) +
           "; threads=" + std::to_string(nThreads) +
           "; events=" + std::to_string(nEvents) +
           "; reproduce: ./generate " + std::to_string(nEvents) + " " + std::to_string(nThreads))
        .Write();
    outFile->Close();   // Zamkniecie pliku
    delete outFile;
    const auto tEnd = Clock::now();

    // 7. Podsumowanie czasu wykonania (benchmark)
    auto sec = [](Clock::duration d) { return std::chrono::duration<double>(d).count(); };
    const double genTime = sec(tGen - tInit);
    std::cout << std::fixed << std::setprecision(3);  // pythia.stat() zmienia format cout
    std::cout << "\n===== BENCHMARK (generate) =====\n"
              << "Watki:              " << nThreads << "\n"
              << "Zapisane zdarzenia: " << nFilled << " / " << nEvents << "\n"
              << "Inicjalizacja:      " << sec(tInit - tStart) << " s\n"
              << "Generacja:          " << genTime << " s\n"
              << "Zapis pliku:        " << sec(tEnd - tGen) << " s\n"
              << "CALKOWITY CZAS:     " << sec(tEnd - tStart) << " s\n"
              << "Wydajnosc:          " << (genTime > 0 ? nFilled / genTime : 0) << " zdarzen/s\n"
              << "================================" << std::endl;

    if (nFilled < nEvents) {
        std::cout << "Uwaga: " << (nEvents - nFilled) << " zdarzen nie wygenerowalo sie poprawnie." << std::endl;
    }
    std::cout << "Gotowe! Dane zapisano w events.root" << std::endl;
    return 0;
}
