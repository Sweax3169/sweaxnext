#pragma once
// SWEAX NEXT — PluginEditor.h — UI v2 "stüdyo konsolu" (eski SWEAX MIX&MASTERING görsel dili, yeni mimari).
// Ekran akışı korunur: RoleSelect → Ready → Listening → MixReady. Kural değişmez: editör HİÇBİR değeri
// tek başına tutmaz; her şey SESSION/APVTS'den okunur, timer ile tazelenir.
// MixReady düzeni (yatay 1180×780): başlık (stil/kalite/LUFS) · rol satırı + ANALİZ · büyük spektrum +
// G/Ç metreleri · A/B–EŞİT SES–DELTA + rapor + GERİ AL/YİNELE/PRESET · 7 makro knob + AI DÖN ·
// sekmeler: ZİNCİR (kartlar, 2 sütun) / EQ / DİNAMİK / TUNE / EFEKTLER / ÇIKIŞ (knob şeritleri).
#include "PluginProcessor.h"
#include "UI/SweaxLnF.h"
#include "UI/SpektrumEkrani.h"
#include "UI/ModulKarti.h"
#include "UI/ModulSeridi.h"
#include "UI/SeviyeMetre.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace sweax
{

class SweaxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SweaxEditor (SweaxProcessor&);
    ~SweaxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    enum Sekme { SekmeZincir = 0, SekmeEq, SekmeDinamik, SekmeTune, SekmeFx, SekmeCikis, SekmeSayisi };
    void sekmeSec (int s);                                 // testler de kullanır (PNG dökümü)
    void presetSecimIsle (int secim);                      // preset menü seçimi (SafePointer sonrası güvenli yol)

private:
    void timerCallback() override;
    void ekranaGoreDuzenle();
    void raporuGoster();
    void kartlariTazele();
    void rolSec (state::Role);
    void sayfalariKur();

    SweaxProcessor& proc;
    ui::SweaxLnF gorunum;
    juce::TooltipWindow ipucuPencere { this, 400 };

    // başlık
    juce::Label baslik, altBaslik, lufsEtiket, scDurum, girisSeviye;
    juce::ComboBox kaliteKutu, stilKutu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kaliteBag;

    // rol + analiz
    juce::Label soru;
    juce::TextButton rolAna, rolBack, rolAdlib, rolMaster, analizEt;
    std::array<juce::Label, 4> rolAciklama;              // RoleSelect ekranı alt satırları

    // spektrum + metreler
    std::unique_ptr<ui::SpektrumEkrani> spektrum;
    std::unique_ptr<ui::SeviyeMetre> girisMetre, cikisMetre;

    // kontrol satırı
    juce::TextButton abIslenmis, abOrijinal, geriAl, yinele, presetBtn;
    juce::ToggleButton esitSes, delta;
    juce::Label ilerleme, ipucu, notEtiket;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> esitSesBag, deltaBag;
    std::unique_ptr<juce::FileChooser> dosyaSecici;

    // makrolar (7) + AI DÖN
    static constexpr int MAKRO_SAYISI = 7;
    std::array<juce::Slider, MAKRO_SAYISI> makroKnob;
    std::array<juce::Label, MAKRO_SAYISI> makroEtiket;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, MAKRO_SAYISI> makroBag;
    juce::TextButton aiDon;

    // sekmeler + içerik
    std::array<juce::TextButton, SekmeSayisi> sekmeButon;
    int aktifSekme { SekmeZincir };
    juce::Viewport kartAlani;                              // ZİNCİR: kartlar 2 sütun
    juce::Component kartIcerik;
    std::vector<std::unique_ptr<ui::ModulKarti>> kartlar;
    std::array<std::unique_ptr<ui::SeritSayfasi>, SekmeSayisi> sayfa;   // [SekmeZincir] boş
    std::vector<std::pair<ui::ModulSeridi*, int>> grSeritleri;          // canlı GR eşlemesi (metering indeksi)

    float girisDb { -100.0f };
    state::Screen sonEkran { state::Screen::RoleSelect };
    uint64_t sonRapor { 0 };
    uint32_t sonPlanId { 0xffffffff };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SweaxEditor)
};

} // namespace sweax
