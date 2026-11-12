// Generacja zderzen pp @ sqrt(s) = 13.6 TeV w Pythii 8 (wersja wielowatkowa, PythiaParallel).
// Uzycie: ./generate [liczba_zdarzen] [liczba_watkow]

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
    using Clock = std::chrono::steady_clock;
    auto tStart = Clock::now();

    long nEvents = (argc > 1) ? std::stol(argv[1]) : 100000;
    int seed = 12345;

    unsigned int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 1;
    if (argc > 2) {
        int argThreads = std::stoi(argv[2]);
        if (argThreads > 0) nThreads = argThreads;
    }

    ROOT::EnableThreadSafety();
    Pythia8::PythiaParallel pythia;

    // pp @ 13.6 TeV, minimum bias
    pythia.readString("Beams:idA = 2212");
    pythia.readString("Beams:idB = 2212");
    pythia.readString("Beams:eCM = 13600.");
    pythia.readString("SoftQCD:all = on");

    // Staly seed: watek k dostaje seed + k. Przy balanceLoad = on kazdy watek
    // generuje N/nThreads zdarzen, wiec zbior zdarzen jest reprodukowalny.
    pythia.readString("Random:setSeed = on");
    pythia.readString("Random:seed = " + std::to_string(seed));
    pythia.readString("Parallelism:numThreads = " + std::to_string(nThreads));
    pythia.readString("Parallelism:balanceLoad = on");

    pythia.readString("Next:numberCount = 0");
    pythia.readString("Next:numberShowInfo = 0");
    pythia.readString("Next:numberShowProcess = 0");
    pythia.readString("Next:numberShowEvent = 0");

    if (!pythia.init()) {
        std::cerr << "Blad inicjalizacji Pythii!" << std::endl;
        return 1;
    }
    auto tInit = Clock::now();

    TFile* outFile = new TFile("events.root", "RECREATE");
    TTree* tree = new TTree("events", "pp collisions at 13.6 TeV");

    std::vector<int> p_id, p_charge;
    std::vector<double> p_pt, p_eta, p_phi;

    tree->Branch("id", &p_id);
    tree->Branch("charge", &p_charge);
    tree->Branch("pt", &p_pt);
    tree->Branch("eta", &p_eta);
    tree->Branch("phi", &p_phi);

    std::cout << "Generowanie " << nEvents << " zdarzen (" << nThreads << " watki)..." << std::endl;

    long nFilled = 0;
    long printEvery = std::max(1L, nEvents / 10);

    pythia.run(nEvents, [&](Pythia8::Pythia* pyPtr) {
        const Pythia8::Event& event = pyPtr->event;

        p_id.clear();
        p_charge.clear();
        p_pt.clear();
        p_eta.clear();
        p_phi.clear();

        for (int i = 0; i < event.size(); ++i) {
            const auto& p = event[i];
            if (!p.isFinal()) continue;

            p_id.push_back(p.id());
            p_charge.push_back(static_cast<int>(std::lround(p.charge())));
            p_pt.push_back(p.pT());
            p_eta.push_back(p.eta());
            p_phi.push_back(p.phi());
        }

        tree->Fill();
        ++nFilled;

        if (nFilled % printEvery == 0) {
            std::cout << "Wygenerowano " << nFilled << " / " << nEvents << std::endl;
        }
    });
    auto tGen = Clock::now();

    pythia.stat();
    outFile->cd();
    tree->Write("", TObject::kOverwrite);

    std::string prodInfo = "generator=generate (PythiaParallel); seed=" + std::to_string(seed) +
                           "; threads=" + std::to_string(nThreads) +
                           "; events=" + std::to_string(nEvents) +
                           "; reproduce: ./generate " + std::to_string(nEvents) + " " + std::to_string(nThreads);
    TNamed("production_info", prodInfo.c_str()).Write();

    outFile->Close();
    delete outFile;
    auto tEnd = Clock::now();

    double initTime = std::chrono::duration<double>(tInit - tStart).count();
    double genTime = std::chrono::duration<double>(tGen - tInit).count();
    double writeTime = std::chrono::duration<double>(tEnd - tGen).count();
    double totalTime = std::chrono::duration<double>(tEnd - tStart).count();

    std::cout << std::fixed << std::setprecision(2); 
    std::cout << "\nZapisano " << nFilled << " / " << nEvents << " zdarzen (watki: " << nThreads << ").\n"
              << "Czas: inicjalizacja " << initTime << " s, generacja " << genTime
              << " s, zapis " << writeTime << " s, razem " << totalTime << " s." << std::endl;

    if (nFilled < nEvents) {
        std::cout << "Uwaga: " << (nEvents - nFilled) << " zdarzen nie wygenerowalo sie poprawnie." << std::endl;
    }
    return 0;
}
