// Makro ROOT rysujace rozklady N, pT, eta, phi z pliku analysis.root.
// Uzycie: root -l -b -q draw_plots.C

#include <iostream>
#include <string>

#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TLegend.h"
#include "TPad.h"
#include "TStyle.h"
#include "TSystem.h"

TH1D* loadHist(TFile* file, const std::string& name, Color_t color, Style_t lineStyle = 1, Width_t lineWidth = 2) {
    TH1D* h = file->Get<TH1D>(name.c_str());
    if (!h) {
        std::cerr << "Blad: Brak histogramu " << name << std::endl;
        return nullptr;
    }
    h->SetLineColor(color);
    h->SetLineStyle(lineStyle);
    h->SetLineWidth(lineWidth);
    h->SetStats(0);
    return h;
}

TCanvas* drawObservable(TFile* file, const std::string& prefix, const std::string& canvasName,
                        const std::string& titleAll, const std::string& titleId,
                        const std::string& xTitle, const std::string& yTitle,
                        bool logY, double xMaxId = -1.0) {
    TH1D* h_all = loadHist(file, prefix + "_all", kBlack, 1, 2);
    TH1D* h_ch = loadHist(file, prefix + "_ch", kBlue + 1, 1, (prefix == "hN" ? 1 : 2));
    TH1D* h_pip = loadHist(file, prefix + "_pip", kRed + 1, 1, 2);
    TH1D* h_pim = loadHist(file, prefix + "_pim", kRed + 1, 2, 2);
    TH1D* h_Kp = loadHist(file, prefix + "_Kp", kGreen + 2, 1, 2);
    TH1D* h_Km = loadHist(file, prefix + "_Km", kGreen + 2, 2, 2);
    TH1D* h_p = loadHist(file, prefix + "_p", kAzure + 2, 1, 2);
    TH1D* h_pbar = loadHist(file, prefix + "_pbar", kAzure + 2, 2, 2);

    if (!h_all || !h_ch || !h_pip || !h_pim || !h_Kp || !h_Km || !h_p || !h_pbar) {
        return nullptr;
    }

    TCanvas* c = new TCanvas(canvasName.c_str(), titleAll.c_str(), 1400, 600);
    c->Divide(2, 1);

    c->cd(1);
    gPad->SetLeftMargin(0.13);
    gPad->SetRightMargin(0.05);
    gPad->SetGrid();
    if (logY) gPad->SetLogy();

    h_all->SetTitle((titleAll + ";" + xTitle + ";" + yTitle).c_str());
    h_all->SetMaximum(h_all->GetMaximum() * (logY ? 3.5 : 1.35));
    h_all->SetMinimum(logY ? 0.8 : 0.0);

    h_all->Draw("HIST");
    h_ch->Draw("HIST SAME");
    if (prefix == "hN") h_all->Draw("HIST SAME");

    TLegend* leg1 = new TLegend(0.58, 0.75, 0.92, 0.88);
    leg1->SetBorderSize(1);
    leg1->SetFillColor(kWhite);
    leg1->AddEntry(h_all, "Wszystkie (all)", "l");
    leg1->AddEntry(h_ch, "Naladowane (ch)", "l");
    leg1->Draw();

    c->cd(2);
    gPad->SetLeftMargin(0.13);
    gPad->SetRightMargin(0.05);
    gPad->SetGrid();
    if (logY) gPad->SetLogy();

    h_pip->SetTitle((titleId + ";" + xTitle + ";" + yTitle).c_str());
    h_pip->SetMaximum(h_pip->GetMaximum() * (logY ? 4.0 : 1.50));
    h_pip->SetMinimum(logY ? 0.8 : 0.0);
    if (xMaxId > 0.0) {
        h_pip->GetXaxis()->SetRangeUser(0.0, xMaxId);
    }

    h_pip->Draw("HIST");
    h_pim->Draw("HIST SAME");
    h_Kp->Draw("HIST SAME");
    h_Km->Draw("HIST SAME");
    h_p->Draw("HIST SAME");
    h_pbar->Draw("HIST SAME");

    TLegend* leg2 = new TLegend(0.65, 0.68, 0.92, 0.88);
    leg2->SetNColumns(2);
    leg2->SetBorderSize(1);
    leg2->SetFillColor(kWhite);
    leg2->AddEntry(h_pip, "#pi^{+}", "l");
    leg2->AddEntry(h_pim, "#pi^{-}", "l");
    leg2->AddEntry(h_Kp, "K^{+}", "l");
    leg2->AddEntry(h_Km, "K^{-}", "l");
    leg2->AddEntry(h_p, "p", "l");
    leg2->AddEntry(h_pbar, "#bar{p}", "l");
    leg2->Draw();

    c->Update();
    return c;
}

void draw_plots(const char* inputFileName = "analysis.root") {
    gStyle->SetOptStat(0);
    gStyle->SetTitleFont(42, "t");
    gStyle->SetTitleFontSize(0.045);

    TFile* file = TFile::Open(inputFileName, "READ");
    if (!file || file->IsZombie()) {
        std::cerr << "Blad: Nie mozna otworzyc pliku " << inputFileName << std::endl;
        return;
    }

    gSystem->mkdir("plots", kTRUE);

    TCanvas* cN = drawObservable(file, "hN", "cN",
        "Krotnosc czastek na zdarzenie (wszystkie vs naladowane)",
        "Krotnosc zidentyfikowanych hadronow", "Multiplicity N", "Liczba zdarzen", true, 100.0);

    TCanvas* cPt = drawObservable(file, "hPt", "cPt",
        "Rozklad pedu poprzecznego pT (wszystkie vs naladowane)",
        "Rozklad pedu poprzecznego pT (hadrony)", "p_{T} [GeV]", "Liczba czastek", true);

    TCanvas* cEta = drawObservable(file, "hEta", "cEta",
        "Rozklad pseudorapidity (wszystkie vs naladowane)",
        "Rozklad pseudorapidity (hadrony)", "#eta", "Liczba czastek", false);

    TCanvas* cPhi = drawObservable(file, "hPhi", "cPhi",
        "Rozklad kata azymutalnego (wszystkie vs naladowane)",
        "Rozklad kata azymutalnego (hadrony)", "#phi [rad]", "Liczba czastek", false);

    if (cN && cPt && cEta && cPhi) {
        cN->SaveAs("plots/dist_N.png");
        cPt->SaveAs("plots/dist_pT.png");
        cEta->SaveAs("plots/dist_eta.png");
        cPhi->SaveAs("plots/dist_phi.png");

        cN->Print("plots/all_distributions.pdf(");
        cPt->Print("plots/all_distributions.pdf");
        cEta->Print("plots/all_distributions.pdf");
        cPhi->Print("plots/all_distributions.pdf)");

        std::cout << "Zapisano wykresy w katalogu plots/" << std::endl;
    }

    file->Close();
    delete file;
}
