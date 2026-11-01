// SWEAX NEXT — PluginEditor.cpp — UI v2 "stüdyo konsolu"
#include "PluginEditor.h"
#include "UI/Metin.h"
#include "Analysis/Report.h"
#include "Decision/PlanPublisher.h"

namespace sweax
{
namespace
{
    using namespace ui::palet;

    constexpr int KENAR = 18;          // dış pay
    constexpr int HDR_H = 58;          // başlık yüksekliği

    void butonStil (juce::TextButton& b, juce::Colour zemin, juce::Colour yazi)
    {
        b.setColour (juce::TextButton::buttonColourId, zemin);
        b.setColour (juce::TextButton::buttonOnColourId, ALTIN);
        b.setColour (juce::TextButton::textColourOffId, yazi);
        b.setColour (juce::TextButton::textColourOnId, ZEMIN);
    }
    void etiketStil (juce::Label& l, float boy, juce::Colour renk, juce::Justification j = juce::Justification::centred, bool kalin = false)
    {
        l.setFont (juce::Font (boy, kalin ? juce::Font::bold : juce::Font::plain));
        l.setColour (juce::Label::textColourId, renk);
        l.setJustificationType (j);
    }
}

SweaxEditor::SweaxEditor (SweaxProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&gorunum);
    setResizable (true, true);
    setResizeLimits (980, 660, 1920, 1280);
    setSize (1180, 780);

    for (auto* b : { &rolAna, &rolBack, &rolAdlib, &rolMaster, &analizEt, &geriAl, &yinele, &presetBtn, &abIslenmis, &abOrijinal, &aiDon }) addAndMakeVisible (*b);
    for (auto* l : { &baslik, &altBaslik, &soru, &scDurum, &girisSeviye, &ipucu, &notEtiket, &ilerleme, &lufsEtiket }) addAndMakeVisible (*l);
    addAndMakeVisible (kaliteKutu);
    addAndMakeVisible (stilKutu);
    addAndMakeVisible (esitSes);
    addAndMakeVisible (delta);
    addAndMakeVisible (kartAlani);
    kartAlani.setViewedComponent (&kartIcerik, false);
    kartAlani.setScrollBarsShown (true, false);

    spektrum = std::make_unique<ui::SpektrumEkrani> (proc);
    addAndMakeVisible (*spektrum);
    girisMetre = std::make_unique<ui::SeviyeMetre> ([this] { return proc.inputPeakDb(); }, metin::GIRIS);
    cikisMetre = std::make_unique<ui::SeviyeMetre> ([this] { return proc.outputPeakDb(); }, metin::CIKIS);
    addAndMakeVisible (*girisMetre);
    addAndMakeVisible (*cikisMetre);

    // ---- makrolar (7) ----
    {
        static const params::Id mIds[MAKRO_SAYISI] = { params::MacroTemizlik, params::MacroDenge, params::MacroOnde,
                                                       params::MacroParlaklik, params::MacroAlan, params::MacroGenislik,
                                                       params::MacroCikis };
        static const char* mAd[MAKRO_SAYISI] = { "TEM\xc4\xb0ZL\xc4\xb0K", "DENGE", "\xc3\x96NDE", "PARLAKLIK",
                                                 "ALAN", "GEN\xc4\xb0\xc5\x9eL\xc4\xb0K", "\xc3\x87IKI\xc5\x9e" };
        for (int i = 0; i < MAKRO_SAYISI; ++i)
        {
            auto& k = makroKnob[(size_t) i];
            k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            k.setDoubleClickReturnValue (true, 0.5);
            addAndMakeVisible (k);
            const auto& t = params::TABLO[(size_t) mIds[i]];
            makroBag[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvts(), juce::String (t.id.data(), t.id.size()), k);
            auto& e = makroEtiket[(size_t) i];
            e.setText (juce::String (juce::CharPointer_UTF8 (mAd[i])), juce::dontSendNotification);
            e.setFont (juce::Font (10.5f, juce::Font::bold));
            e.setJustificationType (juce::Justification::centred);
            e.setColour (juce::Label::textColourId, SOLGUN);
            addAndMakeVisible (e);
        }
    }
    aiDon.setButtonText (metin::AI_DON_TUMU);
    aiDon.onClick = [this] { decision::PlanPublisher::returnAllToAi (proc.paramCache(), proc.session(), proc.undoManager()); };

    // ---- başlık ----
    baslik.setText (metin::URUN, juce::dontSendNotification);          etiketStil (baslik, 32.0f, METIN, juce::Justification::centredLeft, true);
    altBaslik.setText (metin::ALT_BASLIK, juce::dontSendNotification); etiketStil (altBaslik, 12.0f, SOLGUN, juce::Justification::centredLeft);
    etiketStil (lufsEtiket, 13.0f, TURKUAZ, juce::Justification::centredRight, true);
    etiketStil (scDurum, 11.5f, SOLGUN, juce::Justification::centredRight);
    etiketStil (girisSeviye, 11.5f, METIN, juce::Justification::centredRight, true);
    soru.setText (metin::NE_UZERINDE, juce::dontSendNotification);     etiketStil (soru, 22.0f, METIN);
    ipucu.setText (metin::KAYIT_IPUCU, juce::dontSendNotification);    etiketStil (ipucu, 12.5f, SOLGUN); ipucu.setMinimumHorizontalScale (0.8f);
    notEtiket.setText (metin::ANALIZ_YOK_HENUZ, juce::dontSendNotification); etiketStil (notEtiket, 12.0f, SOLGUN); notEtiket.setMinimumHorizontalScale (0.8f);
    etiketStil (ilerleme, 12.5f, METIN); ilerleme.setMinimumHorizontalScale (0.7f);

    // ---- rol + analiz ----
    rolAna.setButtonText (metin::ROL_ANA);     rolBack.setButtonText (metin::ROL_BACK);
    rolAdlib.setButtonText (metin::ROL_ADLIB); rolMaster.setButtonText (metin::ROL_MASTER);
    analizEt.setButtonText (metin::ANALIZ_ET);
    analizEt.getProperties().set ("primary", true);
    geriAl.setButtonText (metin::GERI_AL); yinele.setButtonText (metin::YINELE);
    abIslenmis.setButtonText (metin::AB_ISLENMIS); abOrijinal.setButtonText (metin::AB_ORIJINAL);
    esitSes.setButtonText (metin::AB_ESIT_SES);
    delta.setButtonText ("DELTA");
    presetBtn.setButtonText (metin::PRESET);
    for (auto* b : { &rolAna, &rolBack, &rolAdlib, &rolMaster, &geriAl, &yinele, &presetBtn, &abIslenmis, &abOrijinal, &aiDon })
        butonStil (*b, PANEL, METIN);
    esitSes.setColour (juce::ToggleButton::textColourId, SOLGUN);
    delta.setColour (juce::ToggleButton::textColourId, SOLGUN);

    {
        const juce::String aciklamalar[4] = { metin::ROL_ANA_A, metin::ROL_BACK_A, metin::ROL_ADLIB_A, metin::ROL_MASTER_A };
        for (int i = 0; i < 4; ++i)
        {
            auto& a = rolAciklama[(size_t) i];
            a.setText (aciklamalar[i], juce::dontSendNotification);
            etiketStil (a, 12.0f, SOLGUN);
            a.setMinimumHorizontalScale (0.75f);
            addAndMakeVisible (a);
        }
    }
    rolAna.onClick    = [this] { rolSec (state::Role::Lead); };
    rolBack.onClick   = [this] { rolSec (state::Role::Backing); };
    rolAdlib.onClick  = [this] { rolSec (state::Role::Adlib); };
    rolMaster.onClick = [this] { rolSec (state::Role::Master); };
    presetBtn.onClick = [this]
    {
        // denetim M3.2 (UI-1/UI-8): menü LnF'i hedef bileşenden miras alır (elle set yıkımda asılı kalıyordu);
        // silinme denetimi + SafePointer — menü açıkken editör yıkılırsa geç seçim use-after-free olmaz.
        juce::PopupMenu m;
        m.addItem (1, metin::PRESET_KAYDET);
        m.addItem (2, metin::PRESET_AC);
        juce::Component::SafePointer<SweaxEditor> guvenli (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn).withDeletionCheck (*this),
                         [guvenli] (int secim)
        {
            if (auto* ed = guvenli.getComponent())
                ed->presetSecimIsle (secim);
        });
    };
    analizEt.onClick = [this]
    {
        if (proc.session().getScreen() == state::Screen::Listening) proc.stopListening();
        else proc.startListening();
        ekranaGoreDuzenle();
    };
    geriAl.onClick = [this] { proc.undoManager().undo(); ekranaGoreDuzenle(); kartlariTazele(); };
    yinele.onClick = [this] { proc.undoManager().redo(); ekranaGoreDuzenle(); kartlariTazele(); };

    // A/B: AbMode parametresi (0 = işlenmiş, 1 = orijinal)
    auto abAyarla = [this] (int mod)
    {
        auto& c = proc.paramCache();
        c.param (params::AbMode)->setValueNotifyingHost (c.toNormalized (params::AbMode, (float) mod));
    };
    abIslenmis.onClick = [abAyarla] { abAyarla (0); };
    abOrijinal.onClick = [abAyarla] { abAyarla (1); };

    // DİKKAT (ölçüldü): StringArray{CharPointer_UTF8} çift kodlar — String() ile tek tek addItem
    kaliteKutu.addItem (juce::String (juce::CharPointer_UTF8 ("\x43\x61\x6e\x6c\xc4\xb1")), 1);
    kaliteKutu.addItem (juce::String (juce::CharPointer_UTF8 ("\x59\xc3\xbc\x6b\x73\x65\x6b")), 2);
    kaliteKutu.addItem ("Ultra", 3);
    kaliteBag = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts(), "swx.quality", kaliteKutu);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("\x44\x6f\xc4\x9f\x61\x6c")), 1);
    stilKutu.addItem ("Temiz", 2);
    stilKutu.addItem ("Trap", 3);
    stilKutu.addItem ("Agresif", 4);
    // sanatçı TARZI reçeteleri (itemId = stil indeksi + 1). Bunlar o sanatçıların kayıtlarında duyulan vokal
    // sound'unun tarifidir; sanatçılarla bağlantı ya da onay YOKTUR — başlık ve ipucu bunu açıkça söyler.
    stilKutu.addSeparator();
    stilKutu.addSectionHeading (juce::String (juce::CharPointer_UTF8 ("SANAT\xc3\x87I TARZI (resm\xc3\xae de\xc4\x9fil)")));
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Killoki tarz\xc4\xb1")), 5);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Re\xc3\xa7kol tarz\xc4\xb1")), 6);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Ati242 tarz\xc4\xb1")), 7);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Uzi tarz\xc4\xb1")), 8);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Motive tarz\xc4\xb1")), 9);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Kava tarz\xc4\xb1")), 10);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Ezhel tarz\xc4\xb1")), 11);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Ceza tarz\xc4\xb1")), 12);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Allame tarz\xc4\xb1")), 13);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("\xc5\x9e\x65hin\xc5\x9f\x61h tarz\xc4\xb1")), 14);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Sagopa Kajmer tarz\xc4\xb1")), 15);
    stilKutu.addItem (juce::String (juce::CharPointer_UTF8 ("Ben Fero tarz\xc4\xb1")), 16);
    stilKutu.setTooltip (metin::STIL_IPUCU);
    stilKutu.onChange = [this]
    {
        static const char* adlar[16] = { "Natural", "Clean", "Trap", "Aggressive",
                                         "Killoki", "Reckol", "Ati242", "Uzi", "Motive",
                                         "Kava", "Ezhel", "Ceza", "Allame", "Sehinsah", "Sagopa", "BenFero" };
        const int ix = juce::jlimit (0, 15, stilKutu.getSelectedId() - 1);
        if (ix < 0 || proc.session().getStyleIndex() == ix) return;
        proc.session().setStyle (adlar[ix]);
        // MixReady'de stil seçimi ANINDA uygulanır: dinlemeden, dondurulmuş yakalamadan yeniden plan
        if (proc.session().getScreen() == state::Screen::MixReady)
            ilerleme.setText (proc.replanFromCapture() ? metin::PLAN_GUNCELLENIYOR
                                                       : metin::REPLAN_YOK,   // denetim (K-11): sessiz hiçlik yerine dürüst mesaj
                              juce::dontSendNotification);
    };
    stilKutu.setSelectedId (proc.session().getStyleIndex() + 1, juce::dontSendNotification);
    esitSesBag = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts(), "swx.ab.match", esitSes);
    deltaBag = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts(), "swx.delta", delta);

    // ---- ZİNCİR kartları ----
    for (const char* m : state::modul::HEPSI)
    {
        auto k = std::make_unique<ui::ModulKarti> (proc, m);
        k->yukseklikDegisti = [this] { resized(); };
        kartIcerik.addAndMakeVisible (*k);
        kartlar.push_back (std::move (k));
    }

    // ---- sekmeler ----
    {
        const juce::String adlar[SekmeSayisi] = { metin::TAB_ZINCIR, metin::TAB_EQ, metin::TAB_DINAMIK,
                                                  metin::TAB_TUNE, metin::TAB_FX, metin::TAB_CIKIS };
        for (int i = 0; i < SekmeSayisi; ++i)
        {
            auto& b = sekmeButon[(size_t) i];
            b.setButtonText (adlar[i]);
            b.getProperties().set ("tab", true);
            b.setClickingTogglesState (false);
            b.onClick = [this, i] { sekmeSec (i); };
            addAndMakeVisible (b);
        }
    }
    sayfalariKur();
    sekmeSec (SekmeZincir);

    proc.setEditorAttached (true);
    ekranaGoreDuzenle();
    startTimerHz (20);
}

SweaxEditor::~SweaxEditor()
{
    setLookAndFeel (nullptr);
    proc.setEditorAttached (false);
}

void SweaxEditor::sayfalariKur()
{
    using O = ui::ModulSeridi::Oge;
    using namespace params;
    auto ad = [] (const char* id) { return metin::modulAdi (id); };

    // EQ
    {
        auto s = std::make_unique<ui::SeritSayfasi>();
        std::vector<O> ogeler = { O::G ("HPF"), O::P (EqHpfOn), O::P (EqHpfHz), O::P (EqHpfSlope),
                                  O::G (metin::PATLAMA_KORUMASI), O::P (EqPlosOn), O::P (EqPlosHz), O::P (EqPlosRange) };
        for (int b = 0; b < EQ_BANT_SAYISI; ++b)
        {
            ogeler.push_back (O::G ("BANT " + juce::String (b + 1)));
            for (int alan : { 0, 1, 2, 3, 4, 5, 7 })       // On, Type, Hz, Gain, Q, DynOn, DynRange
                ogeler.push_back (O::P (eqBant (b, alan)));
        }
        auto& serit = s->seritEkle (proc, ad ("eq"), EqOn, ogeler);
        grSeritleri.push_back ({ &serit, 0 });
        // M3.5 TON EQ: preset ton eğrisi (düzeltici EQ'dan ayrı modül)
        std::vector<O> teqOgeler;
        for (int b = 0; b < TEQ_BANT_SAYISI; ++b)
        {
            teqOgeler.push_back (O::G ("BANT " + juce::String (b + 1)));
            for (int alan : { 0, 1, 2, 3, 4 })
                teqOgeler.push_back (O::P (teqBant (b, alan)));
        }
        s->seritEkle (proc, ad ("teq"), TeqOn, teqOgeler);
        addChildComponent (*s);
        sayfa[SekmeEq] = std::move (s);
    }
    // DİNAMİK
    {
        auto s = std::make_unique<ui::SeritSayfasi>();
        auto& g1 = s->seritEkle (proc, ad ("gate"), GateOn,
            { O::P (GateThresh), O::P (GateRange), O::P (GateAttack), O::P (GateRelease), O::P (GateHold), O::P (GateHyst) });
        auto& g2 = s->seritEkle (proc, ad ("lev"), LevOn,
            { O::P (LevTarget), O::P (LevMaxUp), O::P (LevMaxDown), O::P (LevSpeed) });
        auto& g3 = s->seritEkle (proc, ad ("compa"), CaOn,
            { O::P (CaThresh), O::P (CaRatio), O::P (CaKnee), O::P (CaAttack), O::P (CaRelease), O::P (CaMakeup), O::P (CaMix), O::P (CaCharacter) });
        auto& g4 = s->seritEkle (proc, ad ("compb"), CbOn,
            { O::P (CbThresh), O::P (CbRatio), O::P (CbAttack), O::P (CbRelease), O::P (CbMix) });
        auto& g5 = s->seritEkle (proc, ad ("ds"), DsOn,
            { O::P (DsMode), O::P (DsHz), O::P (DsThresh), O::P (DsRange), O::P (DsAttack), O::P (DsRelease),
              O::G (metin::SON_ASAMA), O::P (Ds2On), O::P (Ds2Range), O::P (DsPost) });
        s->seritEkle (proc, ad ("soo"), SooOn,
            { O::P (SooDepth), O::P (SooSharp), O::P (SooLo), O::P (SooHi), O::P (SooSpeed) });
        grSeritleri.push_back ({ &g1, 6 });
        grSeritleri.push_back ({ &g2, 1 });
        grSeritleri.push_back ({ &g3, 2 });
        grSeritleri.push_back ({ &g4, 4 });
        grSeritleri.push_back ({ &g5, 3 });
        addChildComponent (*s);
        sayfa[SekmeDinamik] = std::move (s);
    }
    // TUNE
    {
        auto s = std::make_unique<ui::SeritSayfasi>();
        s->seritEkle (proc, ad ("tune"), TuneOn,
            { O::P (TuneStrength), O::P (TuneSpeed), O::P (TuneKey), O::P (TuneScale) });
        s->seritEkle (proc, ad ("frm"), FrmOn, { O::P (FrmShift), O::P (FrmMix) });
        addChildComponent (*s);
        sayfa[SekmeTune] = std::move (s);
    }
    // EFEKTLER
    {
        auto s = std::make_unique<ui::SeritSayfasi>();
        s->seritEkle (proc, ad ("glow"), GlowOn, { O::P (GlowDepth), O::P (GlowTime) });
        s->seritEkle (proc, ad ("sat"), SatOn, { O::P (SatChar), O::P (SatDrive), O::P (SatTone), O::P (SatMix) });
        s->seritEkle (proc, ad ("dirt"), DirtOn, { O::P (DirtMode), O::P (DirtDrive), O::P (DirtLevel), O::P (DirtTone), O::P (DirtBits), O::P (DirtRate) });
        s->seritEkle (proc, ad ("air"), AirOn, { O::P (AirMid), O::P (AirHigh) });
        s->seritEkle (proc, ad ("layer"), LayerOn, { O::P (LayerA), O::P (LayerB), O::P (LayerLevel), O::P (LayerWidth), O::P (LayerTone), O::P (LayerGrit) });
        s->seritEkle (proc, ad ("dbl"), DblOn,
            { O::P (DblMix), O::P (DblSpreadMs), O::P (DblDetune), O::P (DblHpHz), O::P (DblLpHz), O::P (DblVoices) });
        s->seritEkle (proc, ad ("dly"), DlyOn,
            { O::P (DlyMix), O::P (DlySync), O::P (DlyTimeMs), O::P (DlyFeedback), O::P (DlyHpHz), O::P (DlyLpHz), O::P (DlyDuck), O::P (DlyWidth) });
        s->seritEkle (proc, ad ("rev"), RevOn,
            { O::P (RevType), O::P (RevMix), O::P (RevPre), O::P (RevDecay), O::P (RevDamp), O::P (RevHp), O::P (RevWidth), O::P (RevDuck) });
        s->seritEkle (proc, ad ("wid"), WidOn, { O::P (WidAmount) });
        addChildComponent (*s);
        sayfa[SekmeFx] = std::move (s);
    }
    // ÇIKIŞ
    {
        auto s = std::make_unique<ui::SeritSayfasi>();
        auto& serit = s->seritEkle (proc, metin::GENEL, params::SAYI,
            { O::P (InputTrim), O::P (OutputTrim), O::P (OutCeiling), O::P (OutRelease) });
        grSeritleri.push_back ({ &serit, 5 });
        addChildComponent (*s);
        sayfa[SekmeCikis] = std::move (s);
    }
}

void SweaxEditor::presetSecimIsle (int secim)
{
    if (secim == 0) return;
    auto klasor = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("SWEAX NEXT").getChildFile ("Presets");
    klasor.createDirectory();
    const bool kaydet = secim == 1;
    dosyaSecici = std::make_unique<juce::FileChooser> (kaydet ? metin::PRESET_KAYDET : metin::PRESET_AC,
                                                       klasor, "*.sweax");
    const auto bayrak = kaydet ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting)
                               : juce::FileBrowserComponent::openMode;
    juce::Component::SafePointer<SweaxEditor> guvenli (this);
    dosyaSecici->launchAsync (bayrak | juce::FileBrowserComponent::canSelectFiles, [guvenli, kaydet] (const juce::FileChooser& fc)
    {
        auto* ed = guvenli.getComponent();
        if (ed == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (kaydet)
        {
            if (f.getFileExtension().isEmpty()) f = f.withFileExtension (".sweax");
            if (! ed->proc.savePresetTo (f))
                ed->ilerleme.setText (metin::PRESET_HATA, juce::dontSendNotification);   // denetim (UI-13): sessiz hata
        }
        else if (ed->proc.loadPresetFrom (f))
            ed->ekranaGoreDuzenle();
    });
}

void SweaxEditor::sekmeSec (int sek)
{
    aktifSekme = juce::jlimit (0, SekmeSayisi - 1, sek);
    for (int i = 0; i < SekmeSayisi; ++i)
        sekmeButon[(size_t) i].setToggleState (i == aktifSekme, juce::dontSendNotification);
    const bool mixHazir = proc.session().getScreen() == state::Screen::MixReady;
    kartAlani.setVisible (mixHazir && aktifSekme == SekmeZincir);
    for (int i = 0; i < SekmeSayisi; ++i)
        if (sayfa[(size_t) i] != nullptr)
            sayfa[(size_t) i]->setVisible (mixHazir && i == aktifSekme);
    resized();
}

void SweaxEditor::rolSec (state::Role r)
{
    proc.undoManager().beginNewTransaction ("Rol");
    if (proc.session().getScreen() == state::Screen::Listening)
        proc.cancelListening();                            // denetim M3.2 (UI-2): motor açık kalıp 90 s tavanı dolduruyordu
    const bool mixHazir = proc.session().getScreen() == state::Screen::MixReady;
    proc.session().setRole (r, &proc.undoManager());
    // MixReady'de rol değişimi de anında yeniden planlanır (ekran düşmüyor); yakalama yoksa eski akış
    if (mixHazir && proc.replanFromCapture())
        ilerleme.setText (metin::PLAN_GUNCELLENIYOR, juce::dontSendNotification);
    else
        proc.session().setScreen (state::Screen::Ready, &proc.undoManager());
    ekranaGoreDuzenle();
}

void SweaxEditor::kartlariTazele()
{
    // M3.5: kartlar preset'in GERÇEK işlem sırasıyla dizilir ve numaralanır (sahip: "her biri için özel sıralama")
    // — kapı başta, ekleme modülleri aktif sırayla, FX kuyruğu ve limiter sonda.
    auto ekOf = [] (const juce::String& id) -> int
    {
        static const std::pair<const char*, Ek> tablo[] = {
            { state::modul::TUNE, Ek::Tune }, { state::modul::EQ, Ek::Eq }, { state::modul::LEVELER, Ek::Lev },
            { state::modul::COMP_A, Ek::CompA }, { state::modul::DEESS, Ek::Ds }, { state::modul::COMP_B, Ek::CompB },
            { state::modul::GLOW, Ek::Glow }, { state::modul::SAT, Ek::Sat }, { state::modul::DIRT, Ek::Dirt },
            { state::modul::AIR, Ek::Air }, { state::modul::LAYER, Ek::Layer }, { state::modul::YUMUSAT, Ek::Yumusat },
            { state::modul::FORMANT, Ek::Formant }, { state::modul::TONEQ, Ek::TonEq } };
        for (const auto& [ad, e] : tablo) if (id == ad) return (int) e;
        return -1;
    };
    // sıra parametrelerden (atomik) — ses iş parçacığının durumuna değil (çalmıyorken de plan sırası görünür)
    std::array<float, EK_SAYISI> konumlar {};
    for (int i = 0; i < EK_SAYISI; ++i) konumlar[(size_t) i] = proc.paramCache().raw (EK_KONUM_PARAM[(size_t) i]);
    const Sira sira = siraHesapla (konumlar);
    auto konum = [&] (const juce::String& id) -> int
    {
        if (id == state::modul::GATE) return 0;
        const int e = ekOf (id);
        if (e >= 0) for (int i = 0; i < EK_SAYISI; ++i) if ((int) sira[(size_t) i] == e) return 1 + i;
        static const char* kuyruk[] = { state::modul::DOUBLER, state::modul::DELAY, state::modul::REVERB, state::modul::WIDTH, state::modul::OUTPUT };
        for (int i = 0; i < 5; ++i) if (id == kuyruk[i]) return 1 + EK_SAYISI + i;
        return 100;
    };
    std::stable_sort (kartlar.begin(), kartlar.end(), [&] (const auto& a, const auto& b) { return konum (a->moduleId()) < konum (b->moduleId()); });
    for (size_t i = 0; i < kartlar.size(); ++i) kartlar[i]->setSiraNo ((int) i + 1);
    for (auto& k : kartlar) k->refresh();
    resized();
}

void SweaxEditor::raporuGoster()
{
    auto& eng = proc.analysisEngine();
    const auto pr = eng.progress();
    if (auto r = eng.latestReport(); r != nullptr && pr.capturedSec > 0.05)
    {
        juce::String t = metin::raporOzeti (r->voicedLufs, r->noiseFloorDb, r->sibilantPerMinute,
                                            r->confidence.overall(), r->confidence.enough());
        auto pn = proc.session().planNode (false);
        const bool tekSatir = proc.session().getScreen() == state::Screen::MixReady;   // denetim (UI-11): 30 px etikette maddeler kesiliyordu; maddeler kartlarda zaten var
        if (pn.isValid() && ! tekSatir)
        {
            int shown = 0;
            for (int i = 0; i < 4 && shown < 2; ++i)
            {
                const auto s = metin::ozetSatiri ((int) pn.getProperty ("sum" + juce::String (i), 0));
                if (s.isNotEmpty()) { t << "\n" << juce::String (juce::CharPointer_UTF8 ("• ")) << s; ++shown; }
            }
        }
        ilerleme.setText (t, juce::dontSendNotification);
        sonRapor = eng.reportSerial();
    }
    else
        ilerleme.setText (pr.capturedSec > 0.05 ? metin::OLCUM_BITTI + ":  " + metin::ilerlemeMetni (pr.capturedSec, pr.voicedSec, pr.sentences, pr.sibilantEvents, pr.droppedBlocks > 0, pr.storeFull)
                                                : juce::String(), juce::dontSendNotification);
}

void SweaxEditor::ekranaGoreDuzenle()
{
    const auto ekran = proc.session().getScreen();
    const auto rol = proc.session().getRole();
    const bool rolEkrani = ekran == state::Screen::RoleSelect;
    const bool mixHazir = ekran == state::Screen::MixReady;
    const bool dinliyor = ekran == state::Screen::Listening;

    soru.setVisible (rolEkrani);
    for (auto& a : rolAciklama) a.setVisible (rolEkrani);
    for (auto* b : { &rolAna, &rolBack, &rolAdlib, &rolMaster }) b->setVisible (true);
    rolAna.setToggleState (rol == state::Role::Lead && ! rolEkrani, juce::dontSendNotification);
    rolBack.setToggleState (rol == state::Role::Backing && ! rolEkrani, juce::dontSendNotification);
    rolAdlib.setToggleState (rol == state::Role::Adlib && ! rolEkrani, juce::dontSendNotification);
    rolMaster.setToggleState (rol == state::Role::Master && ! rolEkrani, juce::dontSendNotification);

    analizEt.setVisible (! rolEkrani);
    analizEt.setButtonText (dinliyor ? metin::YETER_BITIR
                          : mixHazir ? metin::YENIDEN_ANALIZ : metin::ANALIZ_ET);
    ilerleme.setVisible (! rolEkrani);

    // MixReady konsol bileşenleri
    presetBtn.setVisible (mixHazir);
    geriAl.setVisible (mixHazir); yinele.setVisible (mixHazir);
    abIslenmis.setVisible (mixHazir); abOrijinal.setVisible (mixHazir);
    esitSes.setVisible (mixHazir); delta.setVisible (mixHazir);
    aiDon.setVisible (mixHazir);
    for (auto& b : sekmeButon) b.setVisible (mixHazir);
    kartAlani.setVisible (mixHazir && aktifSekme == SekmeZincir);
    for (int i = 0; i < SekmeSayisi; ++i)
        if (sayfa[(size_t) i] != nullptr) sayfa[(size_t) i]->setVisible (mixHazir && i == aktifSekme);
    for (auto& k : makroKnob) k.setVisible (mixHazir);
    for (auto& e : makroEtiket) e.setVisible (mixHazir);

    ipucu.setVisible (! mixHazir && ! rolEkrani);
    notEtiket.setVisible (! mixHazir && ! rolEkrani);
    if (spektrum != nullptr) spektrum->setVisible (! rolEkrani);
    girisMetre->setVisible (! rolEkrani);
    cikisMetre->setVisible (! rolEkrani);
    lufsEtiket.setVisible (! rolEkrani);

    if (dinliyor) { ilerleme.setText (metin::HENUZ_SES_YOK, juce::dontSendNotification); notEtiket.setText (metin::DINLIYOR_IPUCU, juce::dontSendNotification); }
    else
    {
        raporuGoster();
        notEtiket.setText (metin::ANALIZ_YOK_HENUZ, juce::dontSendNotification);
    }
    stilKutu.setSelectedId (proc.session().getStyleIndex() + 1, juce::dontSendNotification);   // denetim (UI-5): preset/undo sonrası kutu bayat kalıyordu
    if (mixHazir) kartlariTazele();
    geriAl.setEnabled (proc.undoManager().canUndo());
    yinele.setEnabled (proc.undoManager().canRedo());
    sonEkran = ekran;
    resized();
}

void SweaxEditor::timerCallback()
{
    girisDb = proc.inputPeakDb();
    girisSeviye.setText (metin::seviyeMetni (girisDb), juce::dontSendNotification);
    girisSeviye.setColour (juce::Label::textColourId, girisDb >= -0.5f ? KIRMIZI : (girisDb < -24.0f ? SOLGUN : YESIL));
    scDurum.setText (proc.sidechainConnected() ? metin::SC_BAGLI : metin::SC_YOK, juce::dontSendNotification);
    {
        auto& mt = proc.vocalChain().metering();
        const float st = mt.shortTermLufs();
        float grTop = 0.0f;
        for (int gi = 0; gi <= 6; ++gi) grTop += mt.gr (gi);
        juce::String t;
        t << (st > -90.0f ? juce::String (st, 1) : juce::String ("--")) << " LUFS";
        if (grTop < -0.05f) t << "   GR " << juce::String (-grTop, 1) << " dB";
        lufsEtiket.setText (t, juce::dontSendNotification);
    }
    if (proc.session().getScreen() != sonEkran) ekranaGoreDuzenle();
    if (sonEkran == state::Screen::Listening)
    {
        const auto pr = proc.analysisEngine().progress();
        const float guven = std::min ({ pr.confLevel, pr.confNoise, pr.confTonal, pr.confDynamics, pr.confSibilance });
        ilerleme.setText (pr.capturedSec > 0.05 ? metin::ilerlemeMetni (pr.capturedSec, pr.voicedSec, pr.sentences, pr.sibilantEvents, pr.droppedBlocks > 0, pr.storeFull)
                                                  + "  ·  " + metin::GUVEN + " " + metin::yuzde (guven)
                                                : metin::HENUZ_SES_YOK, juce::dontSendNotification);
    }
    else if (proc.analysisEngine().reportSerial() != sonRapor) raporuGoster();
    if (sonEkran == state::Screen::MixReady)
    {
        auto& mtGr = proc.vocalChain().metering();
        for (auto& k : kartlar)
        {
            const auto id = k->moduleId();
            float v = 0.0f;
            if (id == state::modul::EQ) v = mtGr.gr (0);
            else if (id == state::modul::LEVELER) v = mtGr.gr (1);
            else if (id == state::modul::COMP_A) v = mtGr.gr (2);
            else if (id == state::modul::DEESS) v = mtGr.gr (3);
            else if (id == state::modul::COMP_B) v = mtGr.gr (4);
            else if (id == state::modul::OUTPUT) v = mtGr.gr (5);
            else if (id == state::modul::GATE) v = mtGr.gr (6);
            k->canliGrGuncelle (v);
        }
        for (auto& [serit, gi] : grSeritleri)
            serit->canliGrGuncelle (mtGr.gr (gi));
        const bool orijinal = proc.paramCache().choice (params::AbMode) == 1;
        abIslenmis.setToggleState (! orijinal, juce::dontSendNotification);
        abOrijinal.setToggleState (orijinal, juce::dontSendNotification);
        if (proc.session().getPlanId() != sonPlanId) { sonPlanId = proc.session().getPlanId(); kartlariTazele(); raporuGoster(); }
    }
    geriAl.setEnabled (proc.undoManager().canUndo());
    yinele.setEnabled (proc.undoManager().canRedo());
}

void SweaxEditor::paint (juce::Graphics& g)
{
    // eski reçete: köşegen koyu gradyan + başlık altı altın/turkuaz ince çizgi
    juce::ColourGradient grad (ZEMIN2, 0.0f, 0.0f, juce::Colour (0xff070a0e), (float) getWidth(), (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();
    const int cy = KENAR + HDR_H;
    g.setColour (ALTIN.withAlpha (0.55f));
    g.fillRect (0, cy, getWidth() / 2, 1);
    g.setColour (TURKUAZ.withAlpha (0.55f));
    g.fillRect (getWidth() / 2, cy, getWidth() - getWidth() / 2, 1);
}

void SweaxEditor::resized()
{
    auto r = getLocalBounds().reduced (KENAR);
    const bool mixHazir = proc.session().getScreen() == state::Screen::MixReady;
    const bool rolEkrani = proc.session().getScreen() == state::Screen::RoleSelect;

    // ---- başlık ----
    auto hdr = r.removeFromTop (HDR_H);
    auto sol = hdr.removeFromLeft (300);
    baslik.setBounds (sol.removeFromTop (36));
    altBaslik.setBounds (sol);
    auto sag = hdr.removeFromRight (240);
    lufsEtiket.setBounds (sag.removeFromTop (22));
    girisSeviye.setBounds (sag.removeFromTop (18));
    scDurum.setBounds (sag);
    auto kutular = hdr.removeFromRight (360).reduced (0, 14);
    stilKutu.setBounds (kutular.removeFromLeft (212).reduced (4, 0));
    kaliteKutu.setBounds (kutular.reduced (4, 0));
    r.removeFromTop (10);

    if (rolEkrani)
    {
        // ---- rol seçim ekranı: ortalanmış büyük butonlar ----
        auto orta = r.withSizeKeepingCentre (juce::jmin (560, r.getWidth()), juce::jmin (480, r.getHeight()));
        soru.setBounds (orta.removeFromTop (40)); orta.removeFromTop (12);
        juce::TextButton* rolB[4] = { &rolAna, &rolBack, &rolAdlib, &rolMaster };
        for (int i = 0; i < 4; ++i)
        {
            rolB[i]->setBounds (orta.removeFromTop (50));
            rolAciklama[(size_t) i].setBounds (orta.removeFromTop (20));
            orta.removeFromTop (10);
        }
        return;
    }

    // ---- rol satırı + analiz ----
    auto rolSatir = r.removeFromTop (34);
    auto analizAlan = rolSatir.removeFromRight (juce::jmax (220, rolSatir.getWidth() / 4));
    analizEt.setBounds (analizAlan.reduced (0, 0));
    rolSatir.removeFromRight (10);
    const int bw = (rolSatir.getWidth() - 18) / 4;
    rolAna.setBounds (rolSatir.removeFromLeft (bw)); rolSatir.removeFromLeft (6);
    rolBack.setBounds (rolSatir.removeFromLeft (bw)); rolSatir.removeFromLeft (6);
    rolAdlib.setBounds (rolSatir.removeFromLeft (bw)); rolSatir.removeFromLeft (6);
    rolMaster.setBounds (rolSatir);
    r.removeFromTop (8);

    // ---- spektrum + metreler ---- (kurucu setSize'ında henüz kurulmamış olabilirler — null korumalı)
    const int spekH = mixHazir ? juce::jmax (170, r.getHeight() / 4) : juce::jmax (220, r.getHeight() / 2);
    auto spekSatir = r.removeFromTop (spekH);
    auto metreAlan = spekSatir.removeFromRight (64);
    if (girisMetre != nullptr) girisMetre->setBounds (metreAlan.removeFromLeft (30).reduced (1, 0));
    metreAlan.removeFromLeft (4);
    if (cikisMetre != nullptr) cikisMetre->setBounds (metreAlan.reduced (1, 0));
    if (spektrum != nullptr) spektrum->setBounds (spekSatir);
    r.removeFromTop (8);

    if (! mixHazir)
    {
        // Ready / Listening: ilerleme + ipuçları
        ilerleme.setBounds (r.removeFromTop (52));
        r.removeFromTop (6);
        notEtiket.setBounds (r.removeFromTop (36));
        ipucu.setBounds (r.removeFromBottom (30));
        return;
    }

    // ---- kontrol satırı: A/B | rapor | undo+preset ----
    auto kontrol = r.removeFromTop (30);
    auto abAlan = kontrol.removeFromLeft (juce::jmin (430, kontrol.getWidth() / 2));
    const int abw = 92;
    abIslenmis.setBounds (abAlan.removeFromLeft (abw)); abAlan.removeFromLeft (4);
    abOrijinal.setBounds (abAlan.removeFromLeft (abw)); abAlan.removeFromLeft (10);
    esitSes.setBounds (abAlan.removeFromLeft (130));
    delta.setBounds (abAlan.removeFromLeft (86));
    auto islemAlan = kontrol.removeFromRight (270);
    geriAl.setBounds (islemAlan.removeFromLeft (84)); islemAlan.removeFromLeft (5);
    yinele.setBounds (islemAlan.removeFromLeft (84)); islemAlan.removeFromLeft (5);
    presetBtn.setBounds (islemAlan);
    ilerleme.setBounds (kontrol.reduced (8, 0));
    r.removeFromTop (6);

    // ---- makro satırı ----
    auto makroSatir = r.removeFromTop (104);
    auto aiAlan = makroSatir.removeFromRight (150);
    aiDon.setBounds (aiAlan.withSizeKeepingCentre (146, 30));
    const int mw = makroSatir.getWidth() / MAKRO_SAYISI;
    for (int i = 0; i < MAKRO_SAYISI; ++i)
    {
        auto hucre = makroSatir.removeFromLeft (mw);
        makroEtiket[(size_t) i].setBounds (hucre.removeFromBottom (14));
        makroKnob[(size_t) i].setBounds (hucre.withSizeKeepingCentre (juce::jmin (hucre.getWidth(), 86), hucre.getHeight()));
    }
    r.removeFromTop (4);

    // ---- sekme çubuğu ----
    auto sekmeSatir = r.removeFromTop (30);
    const int tw = sekmeSatir.getWidth() / SekmeSayisi;
    for (auto& b : sekmeButon)
        b.setBounds (sekmeSatir.removeFromLeft (tw).reduced (2, 0));
    r.removeFromTop (6);

    // ---- içerik ----
    if (r.getHeight() < 60) r.setHeight (60);
    kartAlani.setBounds (r);
    for (auto& s : sayfa)
        if (s != nullptr) s->setBounds (r);

    // ZİNCİR: kartlar 2 sütun (masonry); kaydırma çubuğu çıkıp görünür genişliği daraltırsa ikinci geçiş (denetim UI-4)
    for (int gecis = 0; gecis < 2; ++gecis)
    {
        const int kw = kartAlani.getMaximumVisibleWidth();
        const int sutunW = (kw - 8) / 2;
        int y0 = 0, y1 = 0;
        for (auto& k : kartlar)
        {
            const int kh = k->yukseklik();
            if (y0 <= y1) { k->setBounds (0, y0, sutunW, kh); y0 += kh + 6; }
            else          { k->setBounds (sutunW + 8, y1, sutunW, kh); y1 += kh + 6; }
        }
        kartIcerik.setSize (kw, juce::jmax (y0, y1));
        if (kartAlani.getMaximumVisibleWidth() == kw) break;
    }
}

} // namespace sweax
