#include "Pythia8/Pythia.h"
#include "TFile.h"
#include "TTree.h"
#include <vector>
#include <iostream>

int main() {
    // 1. Inicjalizacja obiektu generatora Pythia 8
    Pythia8::Pythia pythia;

    // 2. Konfiguracja fizyki zderzenia
    pythia.readString("Beams:idA = 2212");       // Wiazka A: proton (PDG 2212)
    pythia.readString("Beams:idB = 2212");       // Wiazka B: proton (PDG 2212)
    pythia.readString("Beams:eCM = 13600.");     // sqrt(s) = 13.6 TeV (13600 GeV)
    pythia.readString("SoftQCD:all = on");       // Procesy Minimum Bias (miekka chromodynamika)

    // Wyciszenie komunikatow diagnostycznych pojedynczych zdarzen
    pythia.readString("Next:numberShowInfo = 0");
    pythia.readString("Next:numberShowProcess = 0");
    pythia.readString("Next:numberShowEvent = 0");

    // Zatwierdzenie konfiguracji generatora
    if (!pythia.init()) {
        std::cerr << "Blad: Inicjalizacja Pythii nie powiodla sie!" << std::endl;
        return 1;
    }

    // 3. Utworzenie binarnego pliku ROOT i drzewa kolumnowego TTree
    TFile* outFile = new TFile("events.root", "RECREATE");
    TTree* tree = new TTree("events", "pp collisions at 13.6 TeV");

    // Bufory na parametry czastek w jednym zdarzeniu
    std::vector<int>    p_id;      // Kody czastek wg standardu PDG
    std::vector<int>    p_charge;  // Ladunek elektryczny (-1, 0, +1)
    std::vector<double> p_pt;      // Ped poprzeczny pT [GeV]
    std::vector<double> p_eta;     // Pseudorapidity eta
    std::vector<double> p_phi;     // Kat azymutalny phi [rad]

    // Podpiecie buforow pod galezie (kolumny) drzewa TTree
    tree->Branch("id",     &p_id);
    tree->Branch("charge", &p_charge);
    tree->Branch("pt",     &p_pt);
    tree->Branch("eta",    &p_eta);
    tree->Branch("phi",    &p_phi);

    // 4. Glowna petla generowania zderzen
    const int nEvents = 100000; // Testowo 1 000, docelowo 100 000
    std::cout << "Rozpoczynam generowanie " << nEvents << " zderzen..." << std::endl;

    for (int iEvent = 0; iEvent < nEvents; ++iEvent) {
        // Losowanie i symulacja kolejnego zderzenia
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
            p_charge.push_back(p.charge());
            p_pt.push_back(p.pT());
            p_eta.push_back(p.eta());
            p_phi.push_back(p.phi());
        }

        // Zapis calego zderzenia jako jednego wiersza w TTree
        tree->Fill();

        if ((iEvent + 1) % 500 == 0) {
            std::cout << "Wygenerowano " << (iEvent + 1) << " / " << nEvents << " zdarzen." << std::endl;
        }
    }

    // 6. Finalizacja zapisu i zwolnienie zasobow
    pythia.stat();      // Podsumowanie przekrojow czynnych
    tree->Write();      // Zrzut buforow TTree na dysk
    outFile->Close();   // Zamkniecie pliku

    std::cout << "Gotowe! Dane zapisano w events.root" << std::endl;
    delete outFile;
    return 0;
}