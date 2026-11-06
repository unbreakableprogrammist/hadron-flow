// Generator zderzen pp @ sqrt(s) = 13.6 TeV (Pythia 8) - wersja JEDNOWATKOWA.
// W pelni deterministyczna: przy tym samym seedzie daje identyczny plik bit-w-bit.
//
// Uzycie:  ./generate_single_thread [liczba_zdarzen]   (domyslnie 100 000)
// Wynik:   events_single.root (TTree "events")

#include "Pythia8/Pythia.h"
#include "TFile.h"
#include "TTree.h"
#include <vector>
#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <iomanip>

int main(int argc, char* argv[]) {
    using Clock = std::chrono::steady_clock;
    const auto tStart = Clock::now();

    const long nEvents = (argc > 1) ? std::stol(argv[1]) : 100000;
    const int  seed    = 12345;  // Staly seed -> produkcja reprodukowalna

    // 1. Inicjalizacja obiektu generatora Pythia 8
    Pythia8::Pythia pythia;

    // 2. Konfiguracja fizyki zderzenia
    pythia.readString("Beams:idA = 2212");       // Wiazka A: proton (PDG 2212)
    pythia.readString("Beams:idB = 2212");       // Wiazka B: proton (PDG 2212)
    pythia.readString("Beams:eCM = 13600.");     // sqrt(s) = 13.6 TeV (13600 GeV)
    pythia.readString("SoftQCD:all = on");       // Procesy Minimum Bias (miekka chromodynamika)

    // Reprodukowalnosc: jawnie ustawiony seed generatora liczb losowych
    pythia.readString("Random:setSeed = on");
    pythia.readString("Random:seed = " + std::to_string(seed));

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
    TFile* outFile = new TFile("events_single.root", "RECREATE");
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

    // 4. Glowna petla generowania zderzen
    std::cout << "Rozpoczynam generowanie " << nEvents << " zderzen (1 watek)..." << std::endl;

    long nFilled = 0;
    const long printEvery = std::max(1L, nEvents / 10);

    for (long iEvent = 0; iEvent < nEvents; ++iEvent) {
        // Losowanie i symulacja kolejnego zderzenia (nieudane zdarzenia pomijamy)
        if (!pythia.next()) continue;

        // Czyszczenie wektorow przed przejsciem do nowej kolizji
        p_id.clear();
        p_charge.clear();
        p_pt.clear();
        p_eta.clear();
        p_phi.clear();

        // 5. Selekcja czastek powstalych w zderzeniu
        for (int i = 0; i < pythia.event.size(); ++i) {
            const auto& p = pythia.event[i];

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
    }
    const auto tGen = Clock::now();

    // 6. Finalizacja zapisu i zwolnienie zasobow
    pythia.stat();      // Podsumowanie przekrojow czynnych
    outFile->cd();
    tree->Write("", TObject::kOverwrite);  // Zrzut buforow TTree na dysk
    outFile->Close();   // Zamkniecie pliku
    delete outFile;
    const auto tEnd = Clock::now();

    // 7. Podsumowanie czasu wykonania (benchmark)
    auto sec = [](Clock::duration d) { return std::chrono::duration<double>(d).count(); };
    const double genTime = sec(tGen - tInit);
    std::cout << std::fixed << std::setprecision(3);  // pythia.stat() zmienia format cout
    std::cout << "\n===== BENCHMARK (generate_single_thread) =====\n"
              << "Watki:              1\n"
              << "Zapisane zdarzenia: " << nFilled << " / " << nEvents << "\n"
              << "Inicjalizacja:      " << sec(tInit - tStart) << " s\n"
              << "Generacja:          " << genTime << " s\n"
              << "Zapis pliku:        " << sec(tEnd - tGen) << " s\n"
              << "CALKOWITY CZAS:     " << sec(tEnd - tStart) << " s\n"
              << "Wydajnosc:          " << (genTime > 0 ? nFilled / genTime : 0) << " zdarzen/s\n"
              << "==============================================" << std::endl;

    if (nFilled < nEvents) {
        std::cout << "Uwaga: " << (nEvents - nFilled) << " zdarzen nie wygenerowalo sie poprawnie." << std::endl;
    }
    std::cout << "Gotowe! Dane zapisano w events_single.root" << std::endl;
    return 0;
}
