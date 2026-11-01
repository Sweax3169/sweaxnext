#pragma once
// SWEAX NEXT — Decision/PlanPublisher.h
// Plan yayını (mimari §2.1, §5, §6.2): YALNIZ message thread. MixPlan → APVTS + SESSION.
//  • APVTS: her parametre begin/endChangeGesture içinde setValueNotifyingHost (host otomasyonu/undo'su dokunuşu görür).
//  • Eklenti içi undo: TEK adım — ParamSnapshotAction (değişen parametrelerin eski ham değerleri) + SESSION alanları
//    aynı UndoManager işlemi içinde (planId, ekran, AB kazancı, tabanlar, plan düğümü) → GERİ AL hepsini döndürür.
//  • Kilitli modül (Reason=Locked) atlanır. EQ etkinken planda olmayan bantlar kapatılır (bayat bant kalmaz).
//  • AI tabanları: yazılan her parametre için SESSION.baseline := plan değeri (makro "AI değerine dön" hedefi) ve
//    ParamCache taban atomikleri tazelenir.
//  • Plan düğümü: modül enable + Reason kodları (UI "Neden?" bunu Metin.h ile render eder; Türkçe metin state'e girmez).
#include "Plan.h"
#include "../Core/ParamCache.h"
#include "../State/Session.h"

namespace sweax::decision
{

class PlanPublisher
{
public:
    // uygulanan parametre sayısını döndürür (test/teşhis)
    static int apply (const MixPlan& plan, ParamCache& cache, state::Session& sess, juce::UndoManager& undo);

    // plan düğümünden UI için modül gerekçesi okuma (kod düzeyi; render Metin.h'de)
    static juce::ValueTree moduleNode (state::Session& sess, const char* moduleId);

    // "YİNE DE AÇ" (§8): GEREKMEDİ denen modülü NÖTR değerlerle açar (0 dB / 1:1 / range 0 — işitilebilir değişim yok),
    // modülü otomatik KİLİTLER (yeniden analiz dokunmaz) ve plan düğümüne Reason=UserRequest yazar. Tek undo adımı.
    static void forceEnableNeutral (const char* moduleId, ParamCache& cache, state::Session& sess, juce::UndoManager& undo);

    // "AI DEĞERİNE DÖN": tek makroyu AI tabanına (0.5) döndürür; moduleId == nullptr ile TÜM parametreleri
    // SESSION tabanlarına döndürür (plan uygulandığı andaki değerler). Tek undo adımı.
    static void returnMacroToAi (params::Makro m, ParamCache& cache, state::Session& sess, juce::UndoManager& undo);
    static void returnAllToAi (ParamCache& cache, state::Session& sess, juce::UndoManager& undo);
};

} // namespace sweax::decision
