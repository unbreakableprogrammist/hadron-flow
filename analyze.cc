// Analiza zderzen pp @ sqrt(s) = 13.6 TeV: histogramy N, pT, eta, phi dla kategorii czastek.
// Uzycie: ./analyze [plik_wejsciowy] [plik_wyjsciowy]

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TMath.h"

namespace binning {
    constexpr int nBinsN = 400;
    constexpr double minN = 0.0;
    constexpr double maxN = 400.0;

    constexpr int nBinsPt = 100;
    constexpr double minPt = 0.0;
    constexpr double maxPt = 10.0;

    constexpr int nBinsEta = 100;
    constexpr double minEta = -5.0;
    constexpr double maxEta = 5.0;

    constexpr int nBinsPhi = 64;
    constexpr double minPhi = 0.0;
    const double maxPhi = TMath::Pi();
}

struct CategoryHistos {
    TH1D* hN = nullptr;
    TH1D* hPt = nullptr;
    TH1D* hEta = nullptr;
    TH1D* hPhi = nullptr;
    int countInEvent = 0;

    CategoryHistos(const std::string& suffix, const std::string& label) {
        using namespace binning;
        hN = new TH1D(("hN_" + suffix).c_str(), ("Multiplicity (" + label + ");Multiplicity N;Events").c_str(),
                      nBinsN, minN, maxN);
        hPt = new TH1D(("hPt_" + suffix).c_str(), ("Transverse momentum (" + label + ");p_{T} [GeV];Particles").c_str(),
                       nBinsPt, minPt, maxPt);
        hEta = new TH1D(("hEta_" + suffix).c_str(), ("Pseudorapidity (" + label + ");#eta;Particles").c_str(),
                        nBinsEta, minEta, maxEta);
        hPhi = new TH1D(("hPhi_" + suffix).c_str(), ("Azimuthal angle (" + label + ");#phi [rad];Particles").c_str(),
                        nBinsPhi, minPhi, maxPhi);
    }

    void resetEventCounter() { countInEvent = 0; }

    void fillParticle(double pt, double eta, double phi) {
        ++countInEvent;
        hPt->Fill(pt);
        hEta->Fill(eta);
        hPhi->Fill(phi);
    }

    void fillEventMultiplicity() { hN->Fill(countInEvent); }

    void write() {
        hN->Write();
        hPt->Write();
        hEta->Write();
        hPhi->Write();
    }
};

// Pythia zwraca phi w (-pi, pi], a w zadaniu zakres to [0, pi].
// Skladamy rozklad przez |phi|, a wartosc rowna dokladnie pi przesuwamy minimalnie w dol,
// zeby nie wpadla do overflow ostatniego binu w ROOT.
inline double foldPhi(double phi) {
    double a = std::fabs(phi);
    if (a >= TMath::Pi()) a = std::nextafter(TMath::Pi(), 0.0);
    return a;
}

int main(int argc, char* argv[]) {
    const std::string inFileName = (argc > 1) ? argv[1] : "events.root";
    const std::string outFileName = (argc > 2) ? argv[2] : "analysis.root";

    TFile* inFile = TFile::Open(inFileName.c_str(), "READ");
    if (!inFile || inFile->IsZombie()) {
        std::cerr << "Blad: Nie mozna otworzyc pliku " << inFileName << std::endl;
        return 1;
    }

    TTree* tree = inFile->Get<TTree>("events");
    if (!tree) {
        std::cerr << "Blad: Brak drzewa 'events' w pliku " << inFileName << std::endl;
        return 1;
    }

    // Wskazniki na wektory musza byc zainicjalizowane jako nullptr przed SetBranchAddress
    std::vector<int>* p_id = nullptr;
    std::vector<int>* p_charge = nullptr;
    std::vector<double>* p_pt = nullptr;
    std::vector<double>* p_eta = nullptr;
    std::vector<double>* p_phi = nullptr;

    for (const char* br : {"id", "charge", "pt", "eta", "phi"}) {
        if (!tree->GetBranch(br)) {
            std::cerr << "Blad: Brak galezi '" << br << "' w drzewie 'events'" << std::endl;
            return 1;
        }
    }
    tree->SetBranchAddress("id", &p_id);
    tree->SetBranchAddress("charge", &p_charge);
    tree->SetBranchAddress("pt", &p_pt);
    tree->SetBranchAddress("eta", &p_eta);
    tree->SetBranchAddress("phi", &p_phi);

    TFile* outFile = TFile::Open(outFileName.c_str(), "RECREATE");
    if (!outFile || outFile->IsZombie()) {
        std::cerr << "Blad: Nie mozna utworzyc pliku " << outFileName << std::endl;
        return 1;
    }
    outFile->cd();

    CategoryHistos cat_all("all", "all final-state particles");
    CategoryHistos cat_ch("ch", "charged particles");
    CategoryHistos cat_pip("pip", "#pi^{+}");
    CategoryHistos cat_pim("pim", "#pi^{-}");
    CategoryHistos cat_Kp("Kp", "K^{+}");
    CategoryHistos cat_Km("Km", "K^{-}");
    CategoryHistos cat_p("p", "p");
    CategoryHistos cat_pbar("pbar", "#bar{p}");

    std::vector<CategoryHistos*> allCategories = {
        &cat_all, &cat_ch, &cat_pip, &cat_pim, &cat_Kp, &cat_Km, &cat_p, &cat_pbar
    };

    const Long64_t nEntries = tree->GetEntries();
    const Long64_t printEvery = std::max<Long64_t>(1, nEntries / 10);
    std::cout << "Analiza " << nEntries << " zdarzen z pliku " << inFileName << std::endl;

    for (Long64_t i = 0; i < nEntries; ++i) {
        tree->GetEntry(i);

        for (auto* cat : allCategories) cat->resetEventCounter();

        const size_t nParticles = p_id->size();
        for (size_t j = 0; j < nParticles; ++j) {
            const int id = (*p_id)[j];
            const int charge = (*p_charge)[j];
            const double pt = (*p_pt)[j];
            const double eta = (*p_eta)[j];
            const double phi = foldPhi((*p_phi)[j]);

            cat_all.fillParticle(pt, eta, phi);
            if (charge != 0) cat_ch.fillParticle(pt, eta, phi);

            switch (id) {
                case 211: cat_pip.fillParticle(pt, eta, phi); break;
                case -211: cat_pim.fillParticle(pt, eta, phi); break;
                case 321: cat_Kp.fillParticle(pt, eta, phi); break;
                case -321: cat_Km.fillParticle(pt, eta, phi); break;
                case 2212: cat_p.fillParticle(pt, eta, phi); break;
                case -2212: cat_pbar.fillParticle(pt, eta, phi); break;
                default: break;
            }
        }

        // Zapis krotnosci na koniec zdarzenia (rowniez gdy N = 0)
        for (auto* cat : allCategories) cat->fillEventMultiplicity();

        if ((i + 1) % printEvery == 0) {
            std::cout << "Przeanalizowano " << (i + 1) << " / " << nEntries << " zdarzen." << std::endl;
        }
    }

    for (auto* cat : allCategories) {
        const double ovf = cat->hN->GetBinContent(cat->hN->GetNbinsX() + 1);
        if (ovf > 0) {
            std::cout << "Uwaga: " << cat->hN->GetName() << " ma " << ovf
                      << " zdarzen z N >= " << binning::maxN << " (overflow)." << std::endl;
        }
    }

    outFile->cd();
    for (auto* cat : allCategories) cat->write();

    outFile->Close();
    inFile->Close();
    delete outFile;
    delete inFile;

    std::cout << "Zapisano histogramy w " << outFileName << std::endl;
    return 0;
}
