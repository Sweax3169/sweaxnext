#pragma once
// SWEAX NEXT — Decision/Receteler.h
// M3.5 PRESET REÇETELERİ (sahip 2026-09-17: "birinden bir değeri değiştirip öbürüne koyma — her biri için özel
// sıralama, özel kalite"). M3.4'ün 49 alanlı StyleProfile tablosu yalnız stil "yönünü" taşıyordu; zincirin geri
// kalanı (Comp B oranı/zamanları, dedektör/karakter, de-esser zamanlaması, doubler yayılımı, delay süresi/filtreleri,
// reverb HP/genişlik, leveler aralığı/hızı, kapı zamanları) TÜM stillerde aynı sabitlerdi.
//  • Reçete = bir preset'in TAMAMI: ekleme modülleri SIRASI + her modülün stil tabanı. Analiz yine İNCELTİR
//    (kanıt kesimleri, derinlik = şiddet × güven, guardrail'ler Planner'da kalır); reçete tabanı ve karakteri verir.
//  • Varsayılan üye değerleri = M3.4'e kadar Planner'daki sabitler (jenerik stiller birebir korunur — test).
//    −1 işaretli alanlar "eski yol" demektir (ör. caKnee −1 → taban 4 / kanıt yolu 6).
//  • Tablo bir JSON tasarım dosyasından üretildi: yalnız varsayılandan FARKLI alanlar yazılır,
//    alan sırası bildirim sırasıdır (C++20 atanmış ilklendirici).
#include "../Chain/ZincirSirasi.h"
#include <algorithm>
#include <array>

namespace sweax::decision
{

// reçeteye özel ek EQ bandı (stil kimliği: ör. Kava 750 Hz çukuru, Reçkol "gülümseme" orta kesimi)
struct EkBant
{
    float tip { 0.0f };      // 0 Bell, 1 Low Shelf, 2 High Shelf
    float hz { 1000.0f };
    float db { 0.0f };       // 0 = yok. Kesim statik (≤ −4); yükseltme ölçülen eksikliğe göre yarı telafili (≤ +3)
    float q { 1.0f };
};

struct Recete
{
    const char* ad { "" };
    Sira sira { jenerikSira() };                    // ekleme modülleri işleme sırası
    // ---- GATE ----
    float gateHoldMs { 120.0f };
    float gateRelMs { 150.0f };                     // gürültülü kayıtta min(gateRelMs, 100)
    // ---- TUNE ----
    float tuneStr { 0.65f };
    float tuneSpd { 25.0f };
    // ---- EQ stil bantları ----
    float presHz { 4300.0f };
    float presenceDb { 2.0f };
    float bodyHz { 210.0f };
    float bodyDb { 1.0f };                          // + eksikse telafi / − stil kesimi
    float shelfDb { 2.0f };                         // + hava rafı (havaHz) / − koyu üst (koyuHz)
    float hpfMinHz { 0.0f };                        // stil alçak rafı −3 dB (0 = yok)
    float havaHz { 10500.0f };
    float koyuHz { 11000.0f };
    std::array<EkBant, 3> ek {};
    // ---- LEVELER ----
    float levAralik { 4.0f };                       // taban ± dB (kanıt yolu kendi hesabını yapar)
    float levHiz { 0.5f };
    // ---- COMP A ----
    float ratioA { 3.0f };
    float grA { 4.0f };
    float attackMs { 6.0f };
    float releaseMs { 90.0f };
    float caKnee { -1.0f };                         // −1: taban 4 / kanıt 6
    float caKarakter { -1.0f };                     // −1: oran ≥ 4 → Punch, değilse Clean; 0 Clean 1 Smooth 2 Punch 3 Opto
    float caDetector { 2.0f };                      // 0 Peak 1 RMS 2 Hybrid
    float caMix { 1.0f };
    float caLookahead { 2.0f };
    // ---- COMP B ----
    float grB { 2.0f };                             // 0 → Comp B kapalı
    float cbRatio { 2.0f };
    float cbKnee { 8.0f };
    float cbAttack { 25.0f };
    float cbRelease { 250.0f };
    float cbKarakter { 1.0f };
    float cbDetector { 1.0f };
    float cbMix { 1.0f };
    // ---- DE-ESSER ----
    float dsTaban { -1.0f };                        // kanıtsız taban range dB; −1: eski koşul (KİR/AIR/parlak sat → 3)
    float dsHzOran { 0.85f };                       // bant başlangıcı = s merkezi × oran
    float dsRelease { 60.0f };
    float dsMode { 0.0f };                          // 0 Split 1 Wide
    float ds2Range { 6.0f };                        // son aşama (gövdeye göreli) sigorta aralığı dB
    // ---- YUMUŞAT (dinamik rezonans bastırma) ----
    float sooDepth { 0.0f };                        // 0 = kapalı
    float sooSharp { 0.5f };
    float sooLo { 2000.0f };
    float sooHi { 12000.0f };
    float sooSpeed { 0.5f };
    // ---- GLOW ----
    float glowDepth { 0.12f };
    float glowTime { 1.0f };
    // ---- SAT ----
    float satDrive { 1.5f };
    float satToneDb { 0.0f };
    float satMix { 1.0f };
    float satKarakter { 0.0f };                     // 0 Tüp 1 Bant 2 Kırık
    // ---- KİR ----
    float dirtOn { 0.0f };
    float dirtMode { 0.0f };
    float dirtDrive { 18.0f };
    float dirtLevel { -18.0f };
    float dirtTone { 5000.0f };
    float dirtBits { 10.0f };
    float dirtRate { 2.0f };
    // ---- AIR ----
    float airMid { 0.15f };
    float airHigh { 0.15f };
    // ---- FORMANT ----
    float frmShift { 0.0f };                        // yarım ton; 0 = kapalı
    float frmMix { 1.0f };
    // ---- KATMAN ----
    float layerOn { 0.0f };
    float layerA { 0.0f };
    float layerB { 0.0f };
    float layerLevel { -12.0f };
    float layerWidth { 0.6f };
    float layerTone { 6000.0f };
    float layerGrit { 0.0f };
    // ---- DOUBLER ----
    float dblMix { 0.2f };
    float dblDetune { 9.0f };
    float dblVoices { 0.0f };
    float dblLpHz { 9000.0f };
    float dblSpreadMs { 18.0f };                    // back +6 ms
    float dblHpHz { 180.0f };                       // back +40 Hz
    // ---- DELAY ----
    float dlyMix { 0.12f };
    float dlySync { 0.0f };                         // 0 Slap 1 1/8 2 1/8. 3 1/4 4 1/16
    float dlyFb { 0.22f };
    float dlyDuck { 8.0f };
    float dlyTimeMs { 110.0f };
    float dlyHpHz { 250.0f };
    float dlyLpHz { 6500.0f };
    float dlyWidth { 1.0f };                        // 1 ping-pong, 0 ortada mono
    float dlyAdlibSync { 3.0f };
    // ---- REVERB ----
    float revMix { 0.12f };
    float revPre { 45.0f };
    float revDecay { 1.4f };
    float revDampHz { 7000.0f };
    float revDuck { 6.0f };
    float revHpHz { 220.0f };
    float revWidth { 1.0f };
    float revTip { 0.0f };                          // 0 Plate 1 Oda 2 Salon 3 Chamber
    // ---- WIDTH / ÇIKIŞ ----
    float leadWidth { 1.0f };
    float outTrimDb { 2.5f };
    float masterLufs { -11.0f };
};

// ======== REÇETE TABLOSU (üretilmiş; bir değeri değiştirirken "en çok 2 preset paylaşır" kuralını koru) ========
// @@RECETE_TABLO_BAS@@
inline constexpr int RECETE_SAYISI = 16;
inline const std::array<Recete, RECETE_SAYISI>& receteler()
{
    static const std::array<Recete, RECETE_SAYISI> t {{
        Recete { .ad = "Doğal", .tuneStr = 0.0f, .tuneSpd = 80.0f, .presenceDb = 0.5f, .bodyDb = 0.5f, .shelfDb = 1.0f, .ratioA = 2.5f, .grA = 2.5f, .grB = 1.5f, .glowDepth = 0.0f, .satDrive = 1.0f, .airMid = 0.0f, .airHigh = 0.0f, .dblMix = 0.0f, .dlyMix = 0.0f, .revMix = 0.08f, .outTrimDb = 1.0f, .masterLufs = -14.0f },   // 0 dogal
        Recete { .ad = "Temiz", .presenceDb = 2.5f, .bodyDb = 1.5f, .shelfDb = 3.0f, .satToneDb = 1.0f, .dblMix = 0.22f, .dlyMix = 0.14f },   // 1 temiz
        Recete { .ad = "Trap", .tuneStr = 1.0f, .tuneSpd = 4.0f, .presenceDb = 3.5f, .bodyDb = 1.5f, .shelfDb = 4.0f, .ratioA = 4.0f, .grA = 5.0f, .grB = 2.5f, .glowDepth = 0.22f, .satDrive = 4.0f, .satToneDb = 2.0f, .airMid = 0.2f, .dblMix = 0.4f, .dblDetune = 12.0f, .dblVoices = 1.0f, .dlyMix = 0.2f, .dlyDuck = 7.0f, .revMix = 0.14f, .revPre = 60.0f, .revDecay = 1.8f, .leadWidth = 1.1f, .outTrimDb = 3.5f, .masterLufs = -9.5f },   // 2 trap
        Recete { .ad = "Agresif", .tuneStr = 0.8f, .tuneSpd = 12.0f, .presenceDb = 4.0f, .bodyDb = 2.0f, .shelfDb = 3.5f, .ratioA = 4.0f, .grA = 6.0f, .grB = 3.0f, .glowDepth = 0.2f, .satDrive = 6.0f, .satToneDb = -1.0f, .airHigh = 0.08f, .dblMix = 0.35f, .dblDetune = 12.0f, .dblVoices = 1.0f, .dlyMix = 0.16f, .revMix = 0.1f, .revDecay = 1.2f, .outTrimDb = 4.0f, .masterLufs = -9.0f },   // 3 agresif
        Recete { .ad = "13 Killoki", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Formant, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::CompB, (uint8_t) Ek::Yumusat, (uint8_t) Ek::TonEq, (uint8_t) Ek::Ds, (uint8_t) Ek::Layer, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 70.0f, .gateRelMs = 80.0f, .tuneStr = 0.95f, .tuneSpd = 6.0f, .presHz = 5000.0f, .presenceDb = 4.0f, .bodyHz = 420.0f, .bodyDb = -1.0f, .shelfDb = -2.5f, .hpfMinHz = 120.0f, .havaHz = 9200.0f, .koyuHz = 9000.0f, .ek = {{ { 0.0f, 3200.0f, -3.0f, 1.0f } }}, .levAralik = 7.5f, .levHiz = 0.85f, .ratioA = 8.0f, .grA = 7.5f, .attackMs = 3.0f, .releaseMs = 60.0f, .caKnee = 1.0f, .caKarakter = 2.0f, .caDetector = 0.0f, .caLookahead = 0.0f, .grB = 4.0f, .cbRatio = 4.0f, .cbKnee = 7.0f, .cbAttack = 20.0f, .cbRelease = 280.0f, .cbKarakter = 0.0f, .dsTaban = 4.5f, .dsHzOran = 0.94f, .dsRelease = 25.0f, .sooDepth = 0.24f, .sooSharp = 0.2f, .sooLo = 3000.0f, .sooSpeed = 0.85f, .glowDepth = 0.28f, .glowTime = 0.6f, .satDrive = 8.5f, .satToneDb = -2.5f, .satMix = 0.9f, .satKarakter = 2.0f, .dirtOn = 1.0f, .dirtMode = 1.0f, .dirtDrive = 24.0f, .dirtLevel = -8.0f, .dirtTone = 4000.0f, .dirtBits = 12.0f, .dirtRate = 1.0f, .airMid = 0.0f, .airHigh = 0.0f, .layerOn = 1.0f, .layerA = 12.0f, .layerLevel = -26.0f, .layerWidth = 0.0f, .layerGrit = 0.5f, .dblMix = 0.08f, .dblDetune = 5.0f, .dblLpHz = 4000.0f, .dblSpreadMs = 6.0f, .dblHpHz = 450.0f, .dlyMix = 0.16f, .dlySync = 2.0f, .dlyFb = 0.38f, .dlyDuck = 14.0f, .dlyTimeMs = 321.0f, .dlyHpHz = 370.0f, .dlyLpHz = 4000.0f, .dlyAdlibSync = 1.0f, .revMix = 0.07f, .revPre = 5.0f, .revDecay = 2.3f, .revDampHz = 6000.0f, .revDuck = 16.0f, .revHpHz = 370.0f, .revTip = 2.0f, .leadWidth = 1.15f, .outTrimDb = 5.0f, .masterLufs = -9.5f },   // 4 killoki
        Recete { .ad = "Reçkol", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::Yumusat, (uint8_t) Ek::TonEq, (uint8_t) Ek::Ds, (uint8_t) Ek::Formant, (uint8_t) Ek::Dirt, (uint8_t) Ek::CompB, (uint8_t) Ek::Sat, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Layer, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 100.0f, .gateRelMs = 130.0f, .tuneStr = 1.0f, .tuneSpd = 9.0f, .presHz = 3500.0f, .presenceDb = 2.5f, .bodyDb = 3.0f, .shelfDb = 3.5f, .havaHz = 8500.0f, .koyuHz = 10200.0f, .ek = {{ { 0.0f, 1600.0f, -1.8f, 0.6f } }}, .levAralik = 7.0f, .levHiz = 0.75f, .ratioA = 4.5f, .grA = 6.0f, .attackMs = 8.0f, .releaseMs = 105.0f, .caKnee = 1.5f, .caKarakter = 0.0f, .caLookahead = 4.0f, .grB = 1.8f, .cbRatio = 1.6f, .cbKnee = 2.0f, .cbAttack = 95.0f, .cbRelease = 700.0f, .cbKarakter = 0.0f, .dsTaban = 5.0f, .dsHzOran = 0.76f, .dsRelease = 45.0f, .sooDepth = 0.38f, .sooLo = 2200.0f, .sooHi = 8000.0f, .sooSpeed = 0.8f, .glowDepth = 0.18f, .glowTime = 1.15f, .satDrive = 1.2f, .satToneDb = -0.8f, .satMix = 0.9f, .satKarakter = 1.0f, .dirtOn = 1.0f, .dirtMode = 1.0f, .dirtDrive = 26.0f, .dirtLevel = -26.0f, .dirtTone = 3000.0f, .dirtBits = 16.0f, .dirtRate = 1.0f, .airMid = 0.2f, .airHigh = 0.3f, .frmShift = -1.6f, .layerLevel = -24.0f, .layerWidth = 0.45f, .layerTone = 6200.0f, .dblMix = 0.34f, .dblDetune = 8.0f, .dblVoices = 1.0f, .dblLpHz = 6200.0f, .dblSpreadMs = 30.0f, .dblHpHz = 280.0f, .dlyMix = 0.11f, .dlySync = 2.0f, .dlyFb = 0.26f, .dlyDuck = 18.0f, .dlyTimeMs = 305.0f, .dlyHpHz = 450.0f, .dlyLpHz = 3600.0f, .revMix = 0.13f, .revPre = 20.0f, .revDecay = 1.3f, .revDampHz = 3800.0f, .revDuck = 7.0f, .revHpHz = 240.0f, .revTip = 3.0f, .leadWidth = 1.05f, .outTrimDb = 4.2f, .masterLufs = -10.2f },   // 5 reckol
        Recete { .ad = "Ati242", .sira = {{ (uint8_t) Ek::Eq, (uint8_t) Ek::Tune, (uint8_t) Ek::Formant, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::Sat, (uint8_t) Ek::Ds, (uint8_t) Ek::TonEq, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Layer, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 160.0f, .gateRelMs = 165.0f, .tuneStr = 0.9f, .tuneSpd = 16.0f, .presHz = 3700.0f, .presenceDb = 1.7f, .bodyHz = 420.0f, .bodyDb = -1.8f, .shelfDb = -3.0f, .havaHz = 10700.0f, .koyuHz = 9000.0f, .ek = {{ { 0.0f, 165.0f, 2.5f, 0.8f } }}, .levAralik = 4.5f, .levHiz = 0.4f, .ratioA = 3.4f, .grA = 4.6f, .attackMs = 7.0f, .releaseMs = 95.0f, .caKnee = 9.0f, .caKarakter = 0.0f, .caMix = 0.9f, .caLookahead = 0.8f, .grB = 2.2f, .cbKnee = 11.0f, .cbAttack = 28.0f, .cbRelease = 340.0f, .cbKarakter = 3.0f, .dsTaban = 3.0f, .dsHzOran = 0.84f, .dsRelease = 55.0f, .sooDepth = 0.26f, .sooSharp = 0.58f, .sooLo = 2400.0f, .sooHi = 8200.0f, .sooSpeed = 0.55f, .glowDepth = 0.18f, .glowTime = 1.7f, .satDrive = 2.2f, .satToneDb = -1.2f, .satMix = 0.9f, .dirtOn = 1.0f, .dirtDrive = 24.0f, .dirtLevel = -27.0f, .dirtTone = 2600.0f, .dirtBits = 14.0f, .dirtRate = 1.0f, .airMid = 0.16f, .airHigh = 0.12f, .layerLevel = -25.0f, .layerWidth = 0.15f, .layerTone = 2600.0f, .layerGrit = 0.35f, .dblMix = 0.22f, .dblDetune = 10.0f, .dblLpHz = 6400.0f, .dblSpreadMs = 17.0f, .dblHpHz = 310.0f, .dlyMix = 0.05f, .dlySync = 3.0f, .dlyFb = 0.16f, .dlyDuck = 13.0f, .dlyTimeMs = 260.0f, .dlyHpHz = 300.0f, .dlyLpHz = 3000.0f, .dlyWidth = 0.25f, .dlyAdlibSync = 2.0f, .revMix = 0.19f, .revPre = 48.0f, .revDecay = 2.0f, .revDampHz = 4600.0f, .revDuck = 13.0f, .revHpHz = 260.0f, .revWidth = 0.8f, .outTrimDb = 4.7f, .masterLufs = -9.7f },   // 6 ati242
        Recete { .ad = "Uzi", .sira = {{ (uint8_t) Ek::Eq, (uint8_t) Ek::Lev, (uint8_t) Ek::Sat, (uint8_t) Ek::CompA, (uint8_t) Ek::Ds, (uint8_t) Ek::CompB, (uint8_t) Ek::Tune, (uint8_t) Ek::TonEq, (uint8_t) Ek::Formant, (uint8_t) Ek::Glow, (uint8_t) Ek::Dirt, (uint8_t) Ek::Layer, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 140.0f, .gateRelMs = 190.0f, .tuneStr = 0.68f, .tuneSpd = 11.0f, .presHz = 3000.0f, .presenceDb = 2.8f, .bodyHz = 300.0f, .bodyDb = -1.0f, .shelfDb = -2.5f, .hpfMinHz = 190.0f, .havaHz = 10000.0f, .levAralik = 6.5f, .levHiz = 0.55f, .ratioA = 6.0f, .grA = 7.0f, .attackMs = 1.0f, .releaseMs = 50.0f, .caKnee = 0.5f, .caKarakter = 2.0f, .caDetector = 0.0f, .caLookahead = 0.0f, .grB = 4.0f, .cbRatio = 3.5f, .cbKnee = 6.0f, .cbAttack = 14.0f, .cbRelease = 140.0f, .cbKarakter = 0.0f, .cbDetector = 2.0f, .dsTaban = 4.5f, .dsHzOran = 0.7f, .dsRelease = 55.0f, .ds2Range = 8.0f, .sooDepth = 0.38f, .sooSharp = 0.8f, .sooLo = 2200.0f, .sooHi = 7500.0f, .sooSpeed = 0.85f, .glowDepth = 0.06f, .glowTime = 1.4f, .satDrive = 3.2f, .satToneDb = -1.2f, .satMix = 0.88f, .satKarakter = 1.0f, .dirtOn = 1.0f, .dirtDrive = 20.0f, .dirtLevel = -14.0f, .dirtTone = 4600.0f, .dirtBits = 12.0f, .airMid = 0.08f, .airHigh = 0.04f, .layerOn = 1.0f, .layerA = -12.0f, .layerB = 7.0f, .layerLevel = -18.0f, .layerWidth = 0.55f, .layerTone = 5800.0f, .layerGrit = 0.12f, .dblDetune = 7.0f, .dblLpHz = 6200.0f, .dblSpreadMs = 16.0f, .dblHpHz = 280.0f, .dlyMix = 0.11f, .dlySync = 3.0f, .dlyFb = 0.24f, .dlyDuck = 15.0f, .dlyTimeMs = 105.0f, .dlyHpHz = 320.0f, .dlyLpHz = 5200.0f, .revMix = 0.16f, .revPre = 30.0f, .revDecay = 1.7f, .revDampHz = 5600.0f, .revDuck = 16.0f, .revHpHz = 340.0f, .revWidth = 0.95f, .outTrimDb = 4.8f, .masterLufs = -9.5f },   // 7 uzi
        Recete { .ad = "Motive", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Formant, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Lev, (uint8_t) Ek::Glow, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::TonEq, (uint8_t) Ek::Ds, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Air, (uint8_t) Ek::Layer, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 170.0f, .gateRelMs = 210.0f, .tuneStr = 0.8f, .tuneSpd = 26.0f, .presHz = 4600.0f, .presenceDb = 2.3f, .bodyHz = 240.0f, .bodyDb = 1.8f, .shelfDb = 3.4f, .havaHz = 11000.0f, .koyuHz = 9600.0f, .ek = {{ { 0.0f, 460.0f, -3.0f, 2.2f } }}, .levAralik = 5.2f, .levHiz = 0.28f, .ratioA = 4.0f, .grA = 6.0f, .attackMs = 13.0f, .releaseMs = 45.0f, .caKnee = 1.0f, .caKarakter = 2.0f, .caDetector = 0.0f, .caLookahead = 0.0f, .grB = 3.8f, .cbRatio = 3.2f, .cbKnee = 11.0f, .cbAttack = 10.0f, .cbRelease = 290.0f, .cbKarakter = 3.0f, .dsTaban = 3.8f, .dsHzOran = 0.84f, .dsRelease = 80.0f, .sooDepth = 0.2f, .sooSharp = 0.58f, .sooLo = 1500.0f, .sooHi = 6500.0f, .sooSpeed = 0.42f, .glowDepth = 0.3f, .glowTime = 0.85f, .satDrive = 5.0f, .satMix = 0.8f, .dirtOn = 1.0f, .dirtDrive = 19.0f, .dirtLevel = -19.0f, .dirtTone = 2500.0f, .dirtBits = 12.0f, .dirtRate = 1.0f, .airMid = 0.0f, .airHigh = 0.24f, .frmShift = -0.7f, .layerLevel = -27.0f, .layerWidth = 0.3f, .layerTone = 7500.0f, .dblMix = 0.11f, .dblDetune = 2.0f, .dblLpHz = 7200.0f, .dblSpreadMs = 7.0f, .dblHpHz = 250.0f, .dlyMix = 0.08f, .dlySync = 1.0f, .dlyFb = 0.32f, .dlyDuck = 17.0f, .dlyTimeMs = 90.0f, .dlyHpHz = 420.0f, .dlyLpHz = 3400.0f, .revMix = 0.28f, .revPre = 30.0f, .revDecay = 2.9f, .revDampHz = 8000.0f, .revDuck = 13.0f, .revHpHz = 520.0f, .revTip = 3.0f, .outTrimDb = 2.6f, .masterLufs = -10.7f },   // 8 motive
        Recete { .ad = "Kava", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Formant, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::TonEq, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Ds, (uint8_t) Ek::Glow, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2, (uint8_t) Ek::Layer }}, .gateHoldMs = 130.0f, .gateRelMs = 140.0f, .tuneStr = 0.4f, .tuneSpd = 75.0f, .presHz = 4100.0f, .presenceDb = 2.2f, .bodyHz = 250.0f, .bodyDb = 0.0f, .shelfDb = 0.0f, .havaHz = 7000.0f, .koyuHz = 10800.0f, .ek = {{ { 1.0f, 105.0f, -3.0f, 0.7f }, { 0.0f, 750.0f, -3.0f, 1.1f }, { 0.0f, 7000.0f, 1.2f, 1.0f } }}, .levAralik = 5.5f, .levHiz = 0.6f, .grA = 4.5f, .attackMs = 10.0f, .releaseMs = 150.0f, .caKnee = 11.0f, .caKarakter = 3.0f, .caDetector = 1.0f, .caLookahead = 0.0f, .grB = 3.8f, .cbRatio = 2.8f, .cbAttack = 2.8f, .cbRelease = 305.0f, .cbKarakter = 0.0f, .cbDetector = 0.0f, .dsTaban = 5.0f, .dsHzOran = 0.83f, .dsRelease = 35.0f, .ds2Range = 7.0f, .sooDepth = 0.42f, .sooSharp = 0.35f, .sooLo = 300.0f, .sooHi = 2000.0f, .sooSpeed = 0.75f, .glowDepth = 0.28f, .glowTime = 0.55f, .satDrive = 6.0f, .dirtDrive = 0.0f, .dirtLevel = -36.0f, .dirtTone = 4200.0f, .dirtBits = 12.0f, .dirtRate = 1.0f, .airMid = 0.16f, .airHigh = 0.04f, .layerLevel = -28.0f, .layerWidth = 0.0f, .layerTone = 3000.0f, .dblMix = 0.0f, .dblDetune = 5.0f, .dblLpHz = 5500.0f, .dblSpreadMs = 5.0f, .dblHpHz = 320.0f, .dlyMix = 0.05f, .dlyFb = 0.08f, .dlyDuck = 16.0f, .dlyTimeMs = 42.0f, .dlyHpHz = 520.0f, .dlyLpHz = 3300.0f, .dlyWidth = 0.0f, .dlyAdlibSync = 4.0f, .revPre = 8.0f, .revDecay = 0.45f, .revDampHz = 3800.0f, .revDuck = 17.0f, .revHpHz = 420.0f, .revWidth = 0.6f, .revTip = 1.0f, .outTrimDb = 4.4f, .masterLufs = -10.2f },   // 9 kava
        Recete { .ad = "Ezhel", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Formant, (uint8_t) Ek::Eq, (uint8_t) Ek::Lev, (uint8_t) Ek::Glow, (uint8_t) Ek::CompA, (uint8_t) Ek::TonEq, (uint8_t) Ek::Ds, (uint8_t) Ek::Yumusat, (uint8_t) Ek::CompB, (uint8_t) Ek::Air, (uint8_t) Ek::Sat, (uint8_t) Ek::Layer, (uint8_t) Ek::Dirt, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 230.0f, .gateRelMs = 250.0f, .tuneStr = 0.9f, .tuneSpd = 1.8f, .presHz = 4100.0f, .presenceDb = 3.0f, .bodyHz = 250.0f, .bodyDb = -1.5f, .shelfDb = 0.0f, .havaHz = 12500.0f, .koyuHz = 11400.0f, .ek = {{ { 0.0f, 1250.0f, -1.5f, 2.0f } }}, .levAralik = 7.0f, .levHiz = 0.22f, .ratioA = 1.5f, .grA = 2.0f, .attackMs = 11.5f, .releaseMs = 30.0f, .caKnee = 0.0f, .caKarakter = 0.0f, .caDetector = 0.0f, .caMix = 0.8f, .caLookahead = 0.0f, .grB = 1.0f, .cbRatio = 1.4f, .cbKnee = 16.0f, .cbAttack = 55.0f, .cbRelease = 550.0f, .cbKarakter = 0.0f, .cbDetector = 2.0f, .dsTaban = 3.8f, .dsHzOran = 0.74f, .dsRelease = 65.0f, .sooDepth = 0.36f, .sooSharp = 0.7f, .sooLo = 700.0f, .sooHi = 3400.0f, .sooSpeed = 0.28f, .glowDepth = 0.2f, .glowTime = 0.55f, .satDrive = 3.6f, .satToneDb = -1.8f, .satKarakter = 1.0f, .dirtOn = 1.0f, .dirtMode = 2.0f, .dirtDrive = 4.0f, .dirtLevel = -30.0f, .dirtTone = 3100.0f, .dirtRate = 4.0f, .airMid = 0.14f, .airHigh = 0.26f, .frmShift = -1.2f, .layerOn = 1.0f, .layerA = 12.0f, .layerLevel = -26.0f, .layerWidth = 0.0f, .layerTone = 10000.0f, .dblMix = 0.3f, .dblDetune = 18.0f, .dblVoices = 1.0f, .dblLpHz = 12000.0f, .dblSpreadMs = 7.0f, .dblHpHz = 520.0f, .dlyMix = 0.16f, .dlySync = 2.0f, .dlyFb = 0.45f, .dlyDuck = 15.0f, .dlyTimeMs = 300.0f, .dlyHpHz = 500.0f, .dlyLpHz = 3600.0f, .dlyWidth = 0.45f, .dlyAdlibSync = 1.0f, .revMix = 0.1f, .revPre = 8.0f, .revDecay = 1.6f, .revDampHz = 4200.0f, .revDuck = 15.0f, .revHpHz = 480.0f, .revTip = 1.0f, .outTrimDb = 4.4f, .masterLufs = -9.7f },   // 10 ezhel
        Recete { .ad = "Ceza", .sira = {{ (uint8_t) Ek::Eq, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Tune, (uint8_t) Ek::Formant, (uint8_t) Ek::Lev, (uint8_t) Ek::Ds, (uint8_t) Ek::CompB, (uint8_t) Ek::CompA, (uint8_t) Ek::TonEq, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2, (uint8_t) Ek::Layer }}, .gateHoldMs = 110.0f, .gateRelMs = 160.0f, .tuneStr = 0.3f, .tuneSpd = 260.0f, .presHz = 4400.0f, .presenceDb = 2.2f, .bodyHz = 300.0f, .bodyDb = 2.2f, .shelfDb = 2.6f, .havaHz = 13500.0f, .koyuHz = 13500.0f, .ek = {{ { 0.0f, 540.0f, -2.5f, 2.0f } }}, .levAralik = 3.0f, .levHiz = 0.25f, .ratioA = 4.0f, .grA = 5.5f, .attackMs = 9.0f, .releaseMs = 55.0f, .caKnee = 7.0f, .caKarakter = 2.0f, .caLookahead = 0.0f, .grB = 1.0f, .cbRatio = 1.9f, .cbKnee = 16.0f, .cbAttack = 55.0f, .cbRelease = 240.0f, .cbKarakter = 3.0f, .dsTaban = 3.0f, .dsHzOran = 0.87f, .dsRelease = 30.0f, .sooDepth = 0.36f, .sooSharp = 0.85f, .sooLo = 2500.0f, .sooHi = 3900.0f, .sooSpeed = 0.8f, .glowDepth = 0.0f, .glowTime = 0.5f, .satDrive = 2.6f, .dirtDrive = 0.0f, .dirtLevel = -35.0f, .dirtTone = 3600.0f, .dirtBits = 16.0f, .dirtRate = 1.0f, .airMid = 0.0f, .airHigh = 0.06f, .layerLevel = -36.0f, .layerWidth = 0.0f, .layerTone = 3000.0f, .dblMix = 0.14f, .dblDetune = 4.0f, .dblLpHz = 5800.0f, .dblSpreadMs = 6.0f, .dblHpHz = 520.0f, .dlyMix = 0.06f, .dlyFb = 0.0f, .dlyDuck = 5.0f, .dlyTimeMs = 48.0f, .dlyHpHz = 500.0f, .dlyLpHz = 4000.0f, .dlyWidth = 0.0f, .dlyAdlibSync = 4.0f, .revMix = 0.08f, .revPre = 14.0f, .revDecay = 0.35f, .revDampHz = 4600.0f, .revDuck = 4.0f, .revHpHz = 500.0f, .revWidth = 0.6f, .revTip = 1.0f, .outTrimDb = 3.6f, .masterLufs = -10.9f },   // 11 ceza
        Recete { .ad = "Allame", .sira = {{ (uint8_t) Ek::Eq, (uint8_t) Ek::TonEq, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Ds, (uint8_t) Ek::Tune, (uint8_t) Ek::Formant, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Layer, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 175.0f, .gateRelMs = 290.0f, .tuneStr = 0.0f, .tuneSpd = 150.0f, .presHz = 4200.0f, .presenceDb = 2.7f, .bodyHz = 135.0f, .bodyDb = 2.5f, .shelfDb = 0.0f, .havaHz = 12800.0f, .koyuHz = 8800.0f, .ek = {{ { 0.0f, 2100.0f, 1.5f, 2.0f } }}, .levAralik = 2.5f, .levHiz = 0.45f, .ratioA = 3.2f, .grA = 2.6f, .attackMs = 10.5f, .releaseMs = 105.0f, .caKnee = 14.0f, .caKarakter = 1.0f, .caMix = 0.92f, .caLookahead = 1.2f, .grB = 1.4f, .cbRatio = 1.7f, .cbKnee = 12.0f, .cbAttack = 65.0f, .cbRelease = 520.0f, .dsTaban = 1.8f, .dsHzOran = 0.7f, .dsRelease = 90.0f, .sooDepth = 0.18f, .sooSharp = 0.75f, .sooLo = 360.0f, .sooHi = 2000.0f, .sooSpeed = 0.25f, .glowDepth = 0.06f, .glowTime = 2.2f, .satDrive = 0.0f, .dirtDrive = 0.0f, .dirtLevel = -35.0f, .dirtTone = 3200.0f, .dirtBits = 12.0f, .dirtRate = 1.0f, .airMid = 0.2f, .airHigh = 0.0f, .layerLevel = -20.0f, .layerWidth = 0.3f, .layerTone = 2800.0f, .dblMix = 0.0f, .dblDetune = 4.0f, .dblLpHz = 6800.0f, .dblSpreadMs = 24.0f, .dblHpHz = 320.0f, .dlyMix = 0.0f, .dlyFb = 0.07f, .dlyDuck = 16.0f, .dlyTimeMs = 165.0f, .dlyHpHz = 480.0f, .dlyLpHz = 3100.0f, .dlyWidth = 0.0f, .dlyAdlibSync = 2.0f, .revMix = 0.07f, .revPre = 75.0f, .revDecay = 1.05f, .revDampHz = 3400.0f, .revDuck = 15.0f, .revHpHz = 480.0f, .revWidth = 0.55f, .revTip = 1.0f, .outTrimDb = 4.2f, .masterLufs = -10.7f },   // 12 allame
        Recete { .ad = "Şehinşah", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Yumusat, (uint8_t) Ek::TonEq, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::Ds, (uint8_t) Ek::Formant, (uint8_t) Ek::Sat, (uint8_t) Ek::Layer, (uint8_t) Ek::Dirt, (uint8_t) Ek::Air, (uint8_t) Ek::Glow, (uint8_t) Ek::Ds2 }}, .gateHoldMs = 140.0f, .gateRelMs = 190.0f, .tuneStr = 0.95f, .tuneSpd = 2.5f, .presHz = 3400.0f, .bodyHz = 200.0f, .bodyDb = 1.3f, .shelfDb = -1.2f, .havaHz = 11500.0f, .koyuHz = 12000.0f, .ek = {{ { 0.0f, 1150.0f, -2.5f, 1.9f }, { 0.0f, 2600.0f, -2.0f, 2.2f } }}, .levAralik = 5.8f, .levHiz = 0.45f, .ratioA = 5.0f, .grA = 5.2f, .attackMs = 3.2f, .releaseMs = 85.0f, .caKnee = 6.5f, .caKarakter = 2.0f, .caMix = 0.92f, .caLookahead = 2.5f, .grB = 2.8f, .cbRatio = 2.1f, .cbKnee = 7.0f, .cbAttack = 22.0f, .cbRelease = 520.0f, .cbKarakter = 3.0f, .dsTaban = 4.4f, .dsHzOran = 0.86f, .dsRelease = 35.0f, .ds2Range = 7.5f, .sooDepth = 0.33f, .sooSharp = 0.8f, .sooLo = 1050.0f, .sooHi = 8200.0f, .sooSpeed = 0.9f, .glowDepth = 0.13f, .glowTime = 0.65f, .satDrive = 3.8f, .satToneDb = -0.8f, .satMix = 0.9f, .dirtOn = 1.0f, .dirtDrive = 19.0f, .dirtLevel = -21.0f, .dirtTone = 3200.0f, .airMid = 0.0f, .airHigh = 0.26f, .frmShift = 1.1f, .frmMix = 0.7f, .layerOn = 1.0f, .layerA = 12.0f, .layerLevel = -18.0f, .layerWidth = 0.0f, .layerTone = 10500.0f, .dblMix = 0.21f, .dblDetune = 3.0f, .dblVoices = 1.0f, .dblLpHz = 9200.0f, .dblSpreadMs = 31.0f, .dblHpHz = 340.0f, .dlyMix = 0.17f, .dlySync = 4.0f, .dlyFb = 0.12f, .dlyDuck = 12.0f, .dlyTimeMs = 125.0f, .dlyHpHz = 450.0f, .dlyLpHz = 4400.0f, .dlyWidth = 0.8f, .dlyAdlibSync = 2.0f, .revMix = 0.1f, .revPre = 42.0f, .revDecay = 1.7f, .revDampHz = 7200.0f, .revDuck = 14.0f, .revHpHz = 380.0f, .revWidth = 0.95f, .revTip = 3.0f, .outTrimDb = 4.7f, .masterLufs = -10.8f },   // 13 sehinsah
        Recete { .ad = "Sagopa Kajmer", .sira = {{ (uint8_t) Ek::Eq, (uint8_t) Ek::TonEq, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::Ds, (uint8_t) Ek::CompB, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Tune, (uint8_t) Ek::Formant, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2, (uint8_t) Ek::Layer }}, .gateHoldMs = 170.0f, .gateRelMs = 270.0f, .tuneStr = 0.3f, .tuneSpd = 170.0f, .presHz = 4000.0f, .presenceDb = 1.2f, .bodyHz = 150.0f, .bodyDb = 2.3f, .shelfDb = -4.0f, .koyuHz = 8500.0f, .ek = {{ { 0.0f, 3000.0f, -3.0f, 1.7f }, { 0.0f, 420.0f, -2.0f, 1.1f } }}, .levAralik = 7.5f, .levHiz = 0.2f, .ratioA = 2.0f, .grA = 2.2f, .attackMs = 22.0f, .releaseMs = 260.0f, .caKnee = 16.0f, .caKarakter = 3.0f, .caDetector = 1.0f, .caMix = 0.7f, .caLookahead = 0.0f, .grB = 0.9f, .cbRatio = 1.4f, .cbKnee = 14.0f, .cbAttack = 70.0f, .cbRelease = 850.0f, .cbKarakter = 3.0f, .dsTaban = 2.6f, .dsHzOran = 0.86f, .dsRelease = 120.0f, .sooDepth = 0.31f, .sooSharp = 0.7f, .sooLo = 1450.0f, .sooHi = 4400.0f, .sooSpeed = 0.22f, .glowDepth = 0.04f, .glowTime = 2.8f, .satDrive = 2.4f, .satToneDb = -2.5f, .satMix = 0.85f, .satKarakter = 1.0f, .dirtOn = 1.0f, .dirtMode = 2.0f, .dirtDrive = 9.0f, .dirtLevel = -23.0f, .dirtTone = 4000.0f, .dirtBits = 12.0f, .dirtRate = 3.0f, .airMid = 0.0f, .airHigh = 0.0f, .layerOn = 1.0f, .layerA = 12.0f, .layerLevel = -17.0f, .layerWidth = 0.8f, .layerTone = 5600.0f, .dblMix = 0.32f, .dblDetune = 3.0f, .dblLpHz = 9500.0f, .dblSpreadMs = 34.0f, .dblHpHz = 200.0f, .dlyMix = 0.06f, .dlyFb = 0.16f, .dlyDuck = 14.0f, .dlyTimeMs = 128.0f, .dlyHpHz = 550.0f, .dlyLpHz = 3000.0f, .dlyWidth = 0.25f, .revPre = 10.0f, .revDecay = 1.05f, .revDampHz = 3400.0f, .revDuck = 14.0f, .revHpHz = 260.0f, .revWidth = 0.65f, .revTip = 1.0f, .outTrimDb = 2.6f, .masterLufs = -10.8f },   // 14 sagopa
        Recete { .ad = "Ben Fero", .sira = {{ (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::Formant, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Ds, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::CompB, (uint8_t) Ek::TonEq, (uint8_t) Ek::Glow, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2, (uint8_t) Ek::Layer }}, .gateHoldMs = 130.0f, .gateRelMs = 170.0f, .tuneStr = 1.0f, .tuneSpd = 1.0f, .presHz = 3400.0f, .presenceDb = 4.0f, .bodyHz = 430.0f, .bodyDb = -1.5f, .shelfDb = 0.0f, .havaHz = 13000.0f, .koyuHz = 11800.0f, .ek = {{ { 2.0f, 13500.0f, 2.0f, 0.6f }, { 0.0f, 7000.0f, -2.0f, 1.1f } }}, .levAralik = 4.5f, .levHiz = 0.28f, .ratioA = 4.5f, .grA = 5.5f, .attackMs = 7.0f, .releaseMs = 50.0f, .caKnee = 7.0f, .caKarakter = 2.0f, .caDetector = 0.0f, .caMix = 0.8f, .caLookahead = 0.8f, .grB = 1.6f, .cbRatio = 2.6f, .cbKnee = 9.0f, .cbAttack = 38.0f, .cbRelease = 340.0f, .cbKarakter = 3.0f, .dsTaban = 5.0f, .dsHzOran = 0.72f, .dsRelease = 28.0f, .sooDepth = 0.28f, .sooSharp = 0.38f, .sooLo = 600.0f, .sooHi = 5000.0f, .sooSpeed = 0.55f, .glowDepth = 0.2f, .glowTime = 1.1f, .satDrive = 3.6f, .satToneDb = 1.0f, .satMix = 0.9f, .dirtOn = 1.0f, .dirtDrive = 20.0f, .dirtLevel = -20.0f, .dirtTone = 3000.0f, .dirtBits = 12.0f, .dirtRate = 1.0f, .airMid = 0.26f, .airHigh = 0.0f, .layerLevel = -24.0f, .layerWidth = 0.0f, .layerTone = 2800.0f, .dblMix = 0.22f, .dblDetune = 11.0f, .dblVoices = 1.0f, .dblLpHz = 5700.0f, .dblSpreadMs = 17.0f, .dblHpHz = 330.0f, .dlyMix = 0.04f, .dlyFb = 0.12f, .dlyDuck = 17.0f, .dlyTimeMs = 128.0f, .dlyHpHz = 460.0f, .dlyLpHz = 3800.0f, .dlyWidth = 0.55f, .dlyAdlibSync = 1.0f, .revMix = 0.08f, .revPre = 44.0f, .revDecay = 0.85f, .revDampHz = 5000.0f, .revDuck = 17.0f, .revHpHz = 440.0f, .revWidth = 0.8f, .outTrimDb = 5.0f, .masterLufs = -10.0f },   // 15 benfero
    }};
    return t;
}
// @@RECETE_TABLO_SON@@

inline const Recete& recete (int stil) noexcept
{
    const auto& t = receteler();
    return t[(size_t) std::clamp (stil, 0, RECETE_SAYISI - 1)];
}

} // namespace sweax::decision
