#pragma once
// SWEAX NEXT — UI/Metin.h — TÜM kullanıcı metinleri (Türkçe). Kod içinde serbest metin YASAK.
// İkinci dil (EN) için aynı anahtarlarla ikinci tablo eklenir; Faz 12.
#include <juce_core/juce_core.h>

namespace sweax::metin
{
    inline const juce::String URUN          = juce::CharPointer_UTF8 ("SWEAX");
    inline const juce::String ALT_BASLIK    = juce::CharPointer_UTF8 ("AI VOKAL MOTORU");
    inline const juce::String NE_UZERINDE   = juce::CharPointer_UTF8 ("Ne üzerinde çalışıyoruz?");
    inline const juce::String ROL_ANA       = juce::CharPointer_UTF8 ("ANA VOKAL");
    inline const juce::String ROL_BACK      = juce::CharPointer_UTF8 ("BACK VOKAL");
    inline const juce::String ROL_ADLIB     = juce::CharPointer_UTF8 ("ADLIB");
    inline const juce::String ROL_MASTER    = juce::CharPointer_UTF8 ("MASTER");
    inline const juce::String YAKINDA       = juce::CharPointer_UTF8 ("Yakında");
    inline const juce::String ROL_ANA_A     = juce::CharPointer_UTF8 ("Şarkının önündeki tek vokal — tam zincir: temizlik + ton + yoğunluk + efekt.");
    inline const juce::String ROL_BACK_A    = juce::CharPointer_UTF8 ("Ana vokalin altındaki destek — daha koyu, daha geniş, daha az delay.");
    inline const juce::String ROL_ADLIB_A   = juce::CharPointer_UTF8 ("Araya giren nidalar — daha ıslak, 1/4 atma delay ile.");
    inline const juce::String ROL_MASTER_A  = juce::CharPointer_UTF8 ("Tam karışım (beat + vokal) — mastering: tutkal + seviye + güvenli tavan.");
    inline const juce::String ANALIZ_ET     = juce::CharPointer_UTF8 ("VOKALİ ANALİZ ET");
    inline const juce::String DINLIYOR      = juce::CharPointer_UTF8 ("SWEAX DİNLİYOR...");
    inline const juce::String VOKALI_OYNAT  = juce::CharPointer_UTF8 ("Vokali oynat...");
    inline const juce::String MIX_HAZIR     = juce::CharPointer_UTF8 ("MIX HAZIR ✓");
    inline const juce::String SC_BAGLI      = juce::CharPointer_UTF8 ("Beat Sidechain: Bağlı");
    inline const juce::String SC_YOK        = juce::CharPointer_UTF8 ("Beat Sidechain: Bağlı değil");
    inline const juce::String GIRIS         = juce::CharPointer_UTF8 ("Giriş");
    inline const juce::String CIKIS         = juce::CharPointer_UTF8 ("Çıkış");
    inline const juce::String SINYAL_YOK    = juce::CharPointer_UTF8 ("SİNYAL YOK");
    inline const juce::String SEVIYE_DUSUK  = juce::CharPointer_UTF8 ("Çok Düşük");
    inline const juce::String SEVIYE_IYI    = juce::CharPointer_UTF8 ("İyi");
    inline const juce::String SEVIYE_YUKSEK = juce::CharPointer_UTF8 ("Yüksek");
    inline const juce::String KIRPILIYOR    = juce::CharPointer_UTF8 ("Kırpılıyor");
    inline const juce::String KAYIT_IPUCU   = juce::CharPointer_UTF8 ("Daha yüksek kayıt her zaman daha kaliteli değildir. Kırmızıya girmeden boşluk bırak.");
    inline const juce::String GERI_AL       = juce::CharPointer_UTF8 ("GERİ AL");
    inline const juce::String YINELE        = juce::CharPointer_UTF8 ("YİNELE");
    inline const juce::String NASIL         = juce::CharPointer_UTF8 ("NASIL KULLANIRIM?");
    inline const juce::String YENIDEN_ANALIZ= juce::CharPointer_UTF8 ("YENİDEN ANALİZ ET");
    inline const juce::String AB_ORIJINAL   = juce::CharPointer_UTF8 ("\x4f\x52\xc4\xb0\x4a\xc4\xb0\x4e\x41\x4c");
    inline const juce::String AB_ISLENMIS   = juce::CharPointer_UTF8 ("\xc4\xb0\xc5\x9e\x4c\x45\x4e\x4d\xc4\xb0\xc5\x9e");
    inline const juce::String AB_ESIT_SES   = juce::CharPointer_UTF8 ("A/B EŞİT SES");
    inline const juce::String ANALIZ_YOK_HENUZ = juce::CharPointer_UTF8 ("Henüz analiz yok. VOKALİ ANALİZ ET'e bas ve vokali baştan sona oynat.");
    inline const juce::String YETER_BITIR   = juce::CharPointer_UTF8 ("YETER, ANALİZİ BİTİR");
    inline const juce::String DINLIYOR_IPUCU= juce::CharPointer_UTF8 ("Vokali baştan sona oynat. Cümleler, S-sesleri, seviye ve gürültü ölçülüyor.");
    inline const juce::String YAKALANAN     = juce::CharPointer_UTF8 ("Yakalanan");
    inline const juce::String SESLI         = juce::CharPointer_UTF8 ("Sesli");
    inline const juce::String CUMLE         = juce::CharPointer_UTF8 ("Cümle");
    inline const juce::String S_SESI        = juce::CharPointer_UTF8 ("S-sesi");
    inline const juce::String BLOK_KACTI    = juce::CharPointer_UTF8 ("bazı bloklar kaçtı");
    inline const juce::String DEPO_DOLU     = juce::CharPointer_UTF8 ("90 s tavanı doldu");
    inline const juce::String OLCUM_BITTI   = juce::CharPointer_UTF8 ("Ölçüm bitti");
    inline const juce::String HENUZ_SES_YOK = juce::CharPointer_UTF8 ("Henüz ses gelmedi — vokali oynat.");
    inline const juce::String PLAN_GUNCELLENIYOR = juce::CharPointer_UTF8 ("Plan yeni stile göre güncelleniyor…");
    inline const juce::String REPLAN_YOK  = juce::CharPointer_UTF8 ("Analiz kaydı yok — seçim bir sonraki ANALİZ'de uygulanır.");
    inline const juce::String PRESET_HATA = juce::CharPointer_UTF8 ("Preset KAYDEDİLEMEDİ — klasör yazılabilir mi?");

    // ---- modül adları ve kart durumları ----
    inline juce::String modulAdi (const juce::String& id)
    {
        if (id == "eq")    return juce::CharPointer_UTF8 ("DÜZELTİCİ EQ");
        if (id == "lev")   return juce::CharPointer_UTF8 ("SEVİYELEYİCİ");
        if (id == "compa") return juce::CharPointer_UTF8 ("KOMPRESÖR");
        if (id == "compb") return juce::CharPointer_UTF8 ("KOMPRESÖR B");
        if (id == "ds")    return juce::CharPointer_UTF8 ("DE-ESSER");
        if (id == "out")   return juce::CharPointer_UTF8 ("GÜVENLİK LİMİTER");
        if (id == "gate")  return juce::CharPointer_UTF8 ("GÜRÜLTÜ KAPISI");
        if (id == "sat")   return juce::CharPointer_UTF8 ("KARAKTER");
        if (id == "dbl")   return juce::CharPointer_UTF8 ("DOUBLER");
        if (id == "dly")   return juce::CharPointer_UTF8 ("DELAY");
        if (id == "rev")   return juce::CharPointer_UTF8 ("REVERB");
        if (id == "wid")   return juce::CharPointer_UTF8 ("GENİŞLİK");
        if (id == "tune")  return juce::CharPointer_UTF8 ("TUNE");
        if (id == "air")   return juce::CharPointer_UTF8 ("HAVA (AIR)");
        if (id == "glow")  return juce::CharPointer_UTF8 ("GLOW (PARLAT)");
        if (id == "dirt")  return juce::CharPointer_UTF8 ("KİR (DİSTORSİYON)");
        if (id == "layer") return juce::CharPointer_UTF8 ("KATMAN");
        if (id == "soo")   return juce::CharPointer_UTF8 ("YUMUŞAT");
        if (id == "frm")   return juce::CharPointer_UTF8 ("FORMANT");
        if (id == "teq")   return juce::CharPointer_UTF8 ("TON EQ");
        return id.toUpperCase();
    }
    inline const juce::String PRESET        = juce::CharPointer_UTF8 ("PRESET");
    inline const juce::String PRESET_KAYDET = juce::CharPointer_UTF8 ("\x50\x72\x65\x73\x65\x74\x20\x6b\x61\x79\x64\x65\x74\xe2\x80\xa6");
    inline const juce::String PRESET_AC     = juce::CharPointer_UTF8 ("\x50\x72\x65\x73\x65\x74\x20\x61\xc3\xa7\xe2\x80\xa6");
    inline const juce::String STIL_IPUCU    = juce::CharPointer_UTF8 ("Stil: analiz taban\xc4\xb1n\xc4\xb1 se\xc3\xa7\x65r (comp/doygunluk/efekt karakteri). Haz\xc4\xb1r bir mixte se\xc3\xa7im hemen uygulan\xc4\xb1r. Sanat\xc3\xa7\xc4\xb1 adl\xc4\xb1 stiller o sanat\xc3\xa7\xc4\xb1lar\xc4\xb1n kay\xc4\xb1tlar\xc4\xb1nda duyulan vokal sound'unun tarifidir; sanat\xc3\xa7\xc4\xb1larla ba\xc4\x9flant\xc4\xb1s\xc4\xb1 ya da onay\xc4\xb1 yoktur.");
    inline const juce::String ACIK          = juce::CharPointer_UTF8 ("AÇIK");
    inline const juce::String GEREKMEDI     = juce::CharPointer_UTF8 ("GEREKMEDİ");
    inline const juce::String KILITLI       = juce::CharPointer_UTF8 ("KİLİTLİ");
    inline const juce::String KILITLE       = juce::CharPointer_UTF8 ("KİLİTLE");
    inline const juce::String KILIDI_AC     = juce::CharPointer_UTF8 ("KİLİDİ AÇ");
    inline const juce::String YINE_DE_AC    = juce::CharPointer_UTF8 ("YİNE DE AÇ");
    inline const juce::String DETAYLAR      = juce::CharPointer_UTF8 ("DETAYLAR");
    inline const juce::String KARTLAR       = juce::CharPointer_UTF8 ("KARTLAR");
    inline const juce::String AI_DON        = juce::CharPointer_UTF8 ("AI DEĞERİNE DÖN");
    inline const juce::String AI_DON_TUMU   = juce::CharPointer_UTF8 ("TÜM AI DEĞERLERİNE DÖN");
    inline const juce::String MAKROLAR      = juce::CharPointer_UTF8 ("MAKROLAR");
    inline const juce::String NEDEN         = juce::CharPointer_UTF8 ("Neden?");

    // ---- Reason kodu › Türkçe (kod state'te, metin BURADA — §5.1/§6) ----
    inline juce::String nedenMetni (int rule, float v0, float v1, float v2, float v3)
    {
        juce::ignoreUnused (v3);
        auto T = [] (const char* c) { return juce::String (juce::CharPointer_UTF8 (c)); };
        auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) std::lround (f)) + " Hz"; };
        auto db = [] (float d) { return juce::String (d, 1) + " dB"; };
        switch (rule)
        {
            case 0:  return T ("Bu işlemi gerektirecek kanıt bulunamadı (olasılık %") + juce::String ((int) std::lround (v0 * 100)) + T (", güven %") + juce::String ((int) std::lround (v2 * 100)) + ").";
            case 1:  return T ("Kanıt yeterli değildi; temkinli davranıldı.");
            case 2:  return T ("Kayıt seviyesi dahili referansa alındı (") + db (v2) + ").";
            case 3:  return T ("Alçak frekans gürültüsü ölçüldü › HPF ") + hz (v0) + T (" (en pes notanın güvenli altında).");
            case 4:  return T ("Patlamalı sesler (p/b) ölçüldü › HPF ") + hz (v0) + ".";
            case 5:  return T ("Mikrofona yakınlık tınısı ölçüldü › HPF ") + hz (v0) + ".";
            case 6:  return T ("Bulanıklık (mud) ") + hz (v0) + T (" çevresinde ölçüldü › ") + db (v1) + " kesim.";
            case 7:  return T ("Kutu tınısı ") + hz (v0) + T (" çevresinde ölçüldü › ") + db (v1) + " kesim.";
            case 8:  return T ("Gümleme ") + hz (v0) + T (" çevresinde ölçüldü › ") + db (v1) + " kesim.";
            case 9:  return T ("Genizden tını ölçüldü › ") + hz (v0) + " " + db (v1) + " kesim.";
            case 10: return T ("Kalıcı rezonans ") + hz (v0) + T (" (dar bant) › ") + db (v1) + ".";
            case 11: return T ("Yüksek pasajlarda sertlik › ") + hz (v0) + T (" dinamik bant ") + db (v1) + ".";
            case 12: return T ("Harf patlaması koruması: p/b patlamasında ") + hz (v0) + T (" altı anlık ") + db (v1) + T (" (sesli harfe dokunmaz).");
            case 13: return T ("Tepeler donuk ölçüldü › hafif parlaklık (") + db (v1) + ").";
            case 14: return T ("Aşırı parlaklık ölçüldü › ") + db (v1) + T (" yumuşatma.");
            case 15: return T ("Gövde eksik ölçüldü › ") + db (v1) + T (" alçak raf desteği.");
            case 16: return T ("S-sesleri ") + hz (v0) + T (" çevresinde, dakikada ") + juce::String ((int) std::lround (v2)) + T (" olay › ") + db (v1) + " kontrol.";
            case 17: return T ("Cümleler arası seviye farkı ") + db (v0) + T (" ölçüldü › otomatik seviyeleme.");
            case 18: return T ("Cümle içi dalgalanma ölçüldü › yumuşak kompresyon (hedef ") + db (v0) + " GR).";
            case 19: return T ("Kayıt zaten sıkışmış ölçüldü › ek kompresyon YAPILMADI.");
            case 20: return T ("Güvenlik limiter'ı: tavan ") + db (v0) + " dBTP.";
            case 21: return T ("Senin isteğinle nötr değerlerle açıldı (ses değişmez) ve kilitlendi.");
            case 22: return T ("Kilitli: analiz bu modüle dokunmadı.");
            case 23: return T ("Host otomasyonu var: analiz bu parametreye dokunmadı.");
            case 24: return T ("Taban gürültüsü ölçüldü (") + db (v1) + T (") › kapı eşiği ") + db (v0) + T (", bastırma ") + db (v2) + ".";
            case 25: return T ("Temiz kayıt; cümle aralarını sessizleştiren hafif kapı (") + db (v2) + ").";
            case 26: return T ("Rap vokal tabanı: en pes notaya göre HPF ") + hz (v0) + ".";
            case 27: return T ("Rap vokal yoğunluğu: ") + juce::String (v0, 1) + T (":1 kompresyon (hedef ") + db (v1) + " GR).";
            case 28: return T ("İkinci kademe: yavaş tutkal kompresyonu (") + juce::String (v0, 1) + T (":1, ~") + db (v1) + " GR).";
            case 29: return T ("Cümle dengesi için taban seviyeleme (±") + db (v0) + ").";
            case 30: return T ("Stil hedefi: ") + hz (v0) + T (" çevresinde ") + db (v1) + T (" varlık (presence).");
            case 31: return T ("Stil hedefi: ") + hz (v0) + T (" üstünde ") + db (v1) + T (" hava (parlaklık).");
            case 32: return T ("Stil karakteri: ") + db (v0) + T (" doygunluk (tüp tarzı harmonikler).");
            case 33: return T ("Stil efekti: miktar %") + juce::String ((int) std::lround (v0 * 100)) + ".";
            case 34: return T ("Perde düzeltme: güç %") + juce::String ((int) std::lround (v0 * 100)) + T (", hız ") + juce::String ((int) std::lround (v1)) + " ms.";
            case 35: return T ("Seçili stil bu modülü kullanmıyor.");
            case 36: return T ("Çıkış vokal hedef seviyesine ayarlandı (") + (v0 >= 0.0f ? juce::String ("+") : juce::String()) + db (v0) + T (" — limiter güvenli, stiller eş-seviye).");
            case 37: return T ("Stil hedefi: gövde desteği ") + hz (v0) + " " + db (v1) + T (" (cılız tona karşı).");
            case 39: return T ("Stil karakteri: gövde inceltme ") + hz (v0) + " " + db (v1) + ".";
            case 40: return T ("Stil karakteri: ") + hz (v0) + T (" üstünde ") + db (v1) + T (" koyu üst.");
            case 41: return T ("Stil karakteri: ") + hz (v0) + T (" altında ") + db (v1) + T (" alçak raf (inceltme, temel korunur).");
            case 42: return T ("Stil tizi kabartıyor → önleyici hafif de-esser (") + db (v1) + ", " + hz (v0) + ").";
            case 43:
            {
                static const char* modAd[3] = { "scream", "fuzz", "lo-fi" };
                return T ("Stil kiri: ") + juce::String (modAd[juce::jlimit (0, 2, (int) std::lround (v0))])
                     + T (", sürüş ") + db (v1) + T (", seviye ") + db (v2) + ".";
            }
            case 44:
            {
                auto st = [] (float v) { const int s = (int) std::lround (v); return s == 0 ? juce::String ("-") : (s > 0 ? "+" + juce::String (s) : juce::String (s)); };
                return T ("Stil katmanı: ") + st (v0) + " / " + st (v1) + T (" yarım ton, seviye ") + db (v2) + ".";
            }
            case 45: return T ("Preset tonu: ") + hz (v0) + " " + db (v1) + T (" (reçete bandı; kayıttaki eksikliğe göre ayarlandı).");
            case 46: return T ("Preset yumuşatma: derinlik %") + juce::String ((int) std::lround (v0 * 100)) + ", " + hz (v1) + " – " + hz (v2)
                          + T (" arası gezen sert rezonanslar dinamik bastırılır.");
            case 47: return T ("Preset formantı: ") + (v0 >= 0.0f ? juce::String ("+") : juce::String()) + juce::String (v0, 1)
                          + T (" yarım ton (perde aynı, ses rengi ") + (v0 >= 0.0f ? T ("incelir") : T ("kalınlaşır")) + T ("), karışım %")
                          + juce::String ((int) std::lround (v1 * 100)) + ".";
            case 38: return T ("Master \xc3\xa7\xc4\xb1\x6b\xc4\xb1\xc5\x9f\xc4\xb1\x20\x68\x65\x64\x65\x66\x20\x79\x61\x79\xc4\xb1\x6e\x20\x73\x65\x76\x69\x79\x65\x73\x69\x6e\x65\x3a\x20") + juce::String (v0, 1) + T (" LUFS (") + db (v1) + ").";
            default: return {};
        }
    }

    // ---- parametre kontrol açıklamaları (DETAYLAR araç ipuçları) ----
    inline juce::String paramAciklama (const juce::String& idStr)
    {
        if (idStr == "swx.soo.depth")     return juce::CharPointer_UTF8 ("Gezen sert rezonansların (tırmalayan tepeler) ne kadar bastırılacağı. Hiç yükseltmez; sessiz yerlere dokunmaz.");
        if (idStr == "swx.soo.sharp")     return juce::CharPointer_UTF8 ("Seçicilik: düşük = geniş sertlik bölgeleri, yüksek = dar ıslık/rezonans tepeleri.");
        if (idStr == "swx.soo.lo")        return juce::CharPointer_UTF8 ("Yumuşatmanın odak bandının alt sınırı.");
        if (idStr == "swx.soo.hi")        return juce::CharPointer_UTF8 ("Yumuşatmanın odak bandının üst sınırı. S-sesi için 10-12 kHz yeterli.");
        if (idStr == "swx.soo.speed")     return juce::CharPointer_UTF8 ("Bastırmanın bırakma hızı. Hızlı = şeffaf, yavaş = daha pürüzsüz.");
        if (idStr == "swx.frm.shift")     return juce::CharPointer_UTF8 ("Ses rengini perdeyi değiştirmeden kaydırır. Artı = incelir/burunlaşır, eksi = kalınlaşır/koyulaşır. ±1 ince renk, ±3 bariz efekt.");
        if (idStr == "swx.frm.mix")       return juce::CharPointer_UTF8 ("Formant kaydırılmış sesin kuru sesle karışımı.");
        if (idStr.startsWith ("swx.teq.")) { if (idStr.endsWith (".gain")) return juce::CharPointer_UTF8 ("Preset ton eğrisinin bu bandı. Düzeltici EQ'dan ayrıdır; zincirde preset'in istediği yerde çalışır."); }
        if (idStr == "swx.rev.type")      return juce::CharPointer_UTF8 ("Oda tipi: Plate pürüzsüz vokal plakası, Oda küçük ve yakın, Salon büyük hall, Chamber yoğun ve parlak.");
        if (idStr == "swx.dly.width")     return juce::CharPointer_UTF8 ("1 = yankılar sağ-sol gezer (ping-pong), 0 = yankılar ortada (mono tekrar).");
        if (idStr == "swx.sat.char")      return juce::CharPointer_UTF8 ("Tüp: sıcak, dolgun. Bant: yumuşak, tizleri hafif toparlar. Kırık: sert, kirli çıtırtı.");
        if (idStr.contains (".hpf.hz"))   return juce::CharPointer_UTF8 ("Bu frekansın altını temizler. Vokalin en pes notasının altında kalmalı.");
        if (idStr.contains (".gain"))     return juce::CharPointer_UTF8 ("Bu bandın kazancı. Eksi = kesim, artı = destek.");
        if (idStr.contains (".q"))        return juce::CharPointer_UTF8 ("Bant genişliği. Büyük Q = daha dar, cerrahi müdahale.");
        if (idStr.contains (".dynrng"))   return juce::CharPointer_UTF8 ("Dinamik bandın en fazla ne kadar kısacağı. Yalnız bant öne çıktığında devreye girer.");
        if (idStr.contains ("lev.target"))return juce::CharPointer_UTF8 ("Cümlelerin çekileceği hedef ses seviyesi.");
        if (idStr.contains ("lev.maxup")) return juce::CharPointer_UTF8 ("Sessiz cümlelerin en fazla ne kadar yükseltileceği. Gürültüyü de yükseltmemek için sınırlıdır.");
        if (idStr.contains (".thresh"))   return juce::CharPointer_UTF8 ("Eşik: bunun üstündeki seviye işlenir.");
        if (idStr.contains (".ratio"))    return juce::CharPointer_UTF8 ("Oran: eşiği aşan kısmın ne kadar bastırılacağı. 2:1 yumuşak, 4:1 sıkı.");
        if (idStr.contains (".attack"))   return juce::CharPointer_UTF8 ("Atak: işlemin devreye girme hızı. Çok hızlı = vuruş yumuşar.");
        if (idStr.contains (".release"))  return juce::CharPointer_UTF8 ("Bırakma: işlemin geri çekilme hızı.");
        if (idStr.contains (".makeup"))   return juce::CharPointer_UTF8 ("Kompresyon sonrası telafi kazancı.");
        if (idStr.contains (".mix"))      return juce::CharPointer_UTF8 ("Kuru/ıslak karışımı. 1 = tamamen işlenmiş (paralel için düşür).");
        if (idStr.contains ("ds.hz"))     return juce::CharPointer_UTF8 ("S-sesi bandının başlangıcı. Analiz şarkıcıya göre kalibre eder.");
        if (idStr.contains ("ds.range"))  return juce::CharPointer_UTF8 ("S-seslerinin en fazla ne kadar bastırılacağı.");
        if (idStr.contains ("out.ceiling")) return juce::CharPointer_UTF8 ("Gerçek tepe tavanı. Hiçbir örnek bunu aşmaz.");
        if (idStr.contains ("macro."))    return juce::CharPointer_UTF8 ("AI planının bu boyuttaki derinliği. 0.5 = AI değeri, sağ = daha fazla, sol = daha az.");
        if (idStr == "swx.ds2.range")     return juce::CharPointer_UTF8 ("Son aşama de-esser'ın en fazla ne kadar kısacağı. Zincir s-sesini vokal gövdesine göre öne çıkarmadıysa hiç çalışmaz.");
        if (idStr == "swx.ds2.on")        return juce::CharPointer_UTF8 ("Zincirin sonunda, gövdeye göreli çalışan s-sesi sigortası. Parlatıcı modüller s'leri azdırırsa devreye girer.");
        if (idStr == "swx.ds.post")       return juce::CharPointer_UTF8 ("İkinci (son) de-ess aşaması: s-sesi vokal gövdesini bu kadar aşınca kısılır. Analiz kayda göre koyar: zincir s'yi orijinalden en fazla 2 dB öne çıkarabilir.");
        if (idStr == "swx.eq.plos.hz")    return juce::CharPointer_UTF8 ("Patlama korumasının köşesi: p/b patlaması algılanınca bunun altı anlık kısılır. Analiz en pes notaya göre koyar.");
        if (idStr == "swx.eq.plos.range") return juce::CharPointer_UTF8 ("Patlama anında en fazla ne kadar kısılacağı. Yalnız F0-altı patlamada devreye girer, sesli harflere dokunmaz.");
        if (idStr == "swx.air.mid")       return juce::CharPointer_UTF8 ("3 kHz üstü parlaklık. Program-bağımlı: bant zayıfken kaldırır, zaten parlaksa dokunmaz.");
        if (idStr == "swx.air.high")      return juce::CharPointer_UTF8 ("9 kHz üstü hava/nefes. S-sesi kanıtı varsa analiz bunu otomatik kısar.");
        if (idStr == "swx.glow.depth")    return juce::CharPointer_UTF8 ("OTT tarzı 3 bant yukarı+aşağı sıkıştırma karışımı. %15-30 tatlı bölge; fazlası nefesleri büyütür.");
        if (idStr == "swx.glow.time")     return juce::CharPointer_UTF8 ("Zarf hızı çarpanı. Küçük = daha yapışık/agresif, büyük = daha yumuşak.");
        if (idStr == "swx.dirt.mode")     return juce::CharPointer_UTF8 ("Scream: boğazdan yağlı grit. Fuzz: duvara çakılmış vızıltı. Lofi: bitcrush + telefon hışırtısı. Lead'e PARALEL eklenir.");
        if (idStr == "swx.dirt.level")    return juce::CharPointer_UTF8 ("Kir veri yolunun lead bandına göre seviyesi (otomatik eşlenir). Trap metal scream bus ≈ −10, lo-fi katman ≈ −12.");
        if (idStr == "swx.layer.a")       return juce::CharPointer_UTF8 ("Birinci katman sesinin aralığı (yarım ton). −12 = oktav-altı, 0 = kapalı.");
        if (idStr == "swx.layer.b")       return juce::CharPointer_UTF8 ("İkinci katman sesi. +7 beşli, +12 oktav-üstü, 0 = kapalı.");
        if (idStr.contains ("input.trim")) return juce::CharPointer_UTF8 ("Analizin ölçtüğü giriş kazancı. Tüm eşikler buna göre kurulur.");
        return {};
    }

    inline const juce::String GUVEN         = juce::CharPointer_UTF8 ("Güven");
    inline const juce::String GURULTU       = juce::CharPointer_UTF8 ("Gürültü");
    inline const juce::String KANIT_YETERLI = juce::CharPointer_UTF8 ("kanıt yeterli");
    inline const juce::String KANIT_YETERSIZ= juce::CharPointer_UTF8 ("daha fazla vokal gerekli");
    inline const juce::String S_SESI_DK     = juce::CharPointer_UTF8 ("S-sesi/dk");

    // Plan özet satırları (Decision/Plan.h SummaryLine sırasıyla; kod state'te, metin burada — §6)
    inline juce::String ozetSatiri (int code)
    {
        switch (code)
        {
            case 1:  return juce::CharPointer_UTF8 ("Giriş seviyesi referansa alındı");
            case 2:  return juce::CharPointer_UTF8 ("Alçak frekans temizliği (HPF) uygulandı");
            case 3:  return juce::CharPointer_UTF8 ("Bulanıklık (mud) azaltıldı");
            case 4:  return juce::CharPointer_UTF8 ("Kutu tınısı azaltıldı");
            case 5:  return juce::CharPointer_UTF8 ("Rezonans kesildi");
            case 6:  return juce::CharPointer_UTF8 ("Sertlik yumuşatıldı");
            case 7:  return juce::CharPointer_UTF8 ("S-sesleri kontrol altına alındı");
            case 8:  return juce::CharPointer_UTF8 ("Cümle seviyeleri dengelendi");
            case 9:  return juce::CharPointer_UTF8 ("Dinamikler sıkılaştırıldı");
            case 10: return juce::CharPointer_UTF8 ("Ciddi bir sorun bulunamadı — hafif dokunuşlar yeterli");
            case 11: return juce::CharPointer_UTF8 ("Kanıt sınırlı — temkinli ayarlar yapıldı");
            case 12: return juce::CharPointer_UTF8 ("Dikkat: kayıtta gürültü yüksek");
            case 13: return juce::CharPointer_UTF8 ("Dikkat: kayıtta kırpılma var — yeniden kayıt önerilir");
            default: return {};
        }
    }

    inline juce::String saniye (double s) { return juce::String (s, 1) + " s"; }
    inline juce::String yuzde (float x) { return "%" + juce::String ((int) std::lround (juce::jlimit (0.0f, 1.0f, x) * 100.0f)); }
    // rapor özeti: seviye, gürültü, S-sesi sıklığı, güven
    // sayılar roundToInt ile: juce::String (v, 0) sıfır ondalık değil TAM hassasiyet basar
    inline juce::String raporOzeti (float voicedLufs, float noiseFloorDb, float sibilantPerMin, float guven, bool yeterli)
    {
        return juce::String (voicedLufs, 1) + " LUFS  ·  " + GURULTU + " " + juce::String (juce::roundToInt (noiseFloorDb)) + " dB  ·  "
             + S_SESI_DK + " " + juce::String (juce::roundToInt (sibilantPerMin)) + "  ·  " + GUVEN + " " + yuzde (guven)
             + "  (" + (yeterli ? KANIT_YETERLI : KANIT_YETERSIZ) + ")";
    }
    inline juce::String ilerlemeMetni (double yakalananSn, double sesliSn, int cumle, int sSesi, bool blokKacti, bool depoDolu)
    {
        juce::String t = YAKALANAN + " " + saniye (yakalananSn) + "  ·  " + SESLI + " " + saniye (sesliSn)
                       + "  ·  " + CUMLE + " " + juce::String (cumle) + "  ·  " + S_SESI + " " + juce::String (sSesi);
        if (blokKacti) t << "  ·  (" << BLOK_KACTI << ")";
        if (depoDolu) t << "  ·  (" << DEPO_DOLU << ")";
        return t;
    }

    // Kayıt seviyesi sınıflandırması (tepe dBFS) — teknik terim yok, dört kelime
    inline const juce::String& seviyeMetni (float peakDb)
    {
        if (peakDb < -60.0f) return SINYAL_YOK;
        if (peakDb < -24.0f) return SEVIYE_DUSUK;
        if (peakDb < -6.0f)  return SEVIYE_IYI;
        if (peakDb < -0.5f)  return SEVIYE_YUKSEK;
        return KIRPILIYOR;
    }

    // ---- gam adi (TuneKey choice + TuneScale choice; 0 = kromatik) ----
    inline juce::String gamAdi (int keyChoice, int scaleChoice)
    {
        static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (scaleChoice <= 0) return juce::CharPointer_UTF8 ("Kromatik");
        return juce::String (n[((keyChoice % 12) + 12) % 12])
             + (scaleChoice == 2 ? juce::String (juce::CharPointer_UTF8 (" minör"))
                                 : juce::String (juce::CharPointer_UTF8 (" majör")));
    }

    // ---- UI v2: sekmeler ----
    inline const juce::String TAB_ZINCIR   = juce::CharPointer_UTF8 ("ZİNCİR");
    inline const juce::String TAB_EQ       = juce::CharPointer_UTF8 ("EQ");
    inline const juce::String TAB_DINAMIK  = juce::CharPointer_UTF8 ("DİNAMİK");
    inline const juce::String TAB_TUNE     = juce::CharPointer_UTF8 ("TUNE");
    inline const juce::String TAB_FX       = juce::CharPointer_UTF8 ("EFEKTLER");
    inline const juce::String TAB_CIKIS    = juce::CharPointer_UTF8 ("ÇIKIŞ");
    inline const juce::String GENEL        = juce::CharPointer_UTF8 ("GENEL");
    inline const juce::String PATLAMA_KORUMASI = juce::CharPointer_UTF8 ("PATLAMA KORUMASI");
    inline const juce::String SON_ASAMA = juce::CharPointer_UTF8 ("SON AŞAMA (S SİGORTASI)");

    // ---- UI v2: parametre KISA adı (knob etiketi, Türkçe) — id sonekinden türetilir ----
    inline juce::String paramKisaAd (const juce::String& idStr, const juce::String& tabloAd)
    {
        auto T = [] (const char* c) { return juce::String (juce::CharPointer_UTF8 (c)); };
        // EQ bant alanları (grup başlığı "BANT N" zaten var — önek yok)
        if (idStr.startsWith ("swx.teq.b"))
        {
            if (idStr.endsWith (".type"))   return T ("TİP");
            if (idStr.endsWith (".hz"))     return T ("FREKANS");
            if (idStr.endsWith (".gain"))   return T ("KAZANÇ");
            if (idStr.endsWith (".q"))      return "Q";
            if (idStr.endsWith (".on"))     return T ("AÇIK");
        }
        if (idStr == "swx.soo.depth")      return T ("DERİNLİK");
        if (idStr == "swx.soo.sharp")      return T ("SEÇİCİLİK");
        if (idStr == "swx.soo.lo")         return T ("ODAK ALT");
        if (idStr == "swx.soo.hi")         return T ("ODAK ÜST");
        if (idStr == "swx.frm.shift")      return T ("KAYDIR");
        if (idStr == "swx.rev.type")       return T ("TİP");
        if (idStr == "swx.sat.char")       return T ("KARAKTER");
        if (idStr.startsWith ("swx.eq.b"))
        {
            if (idStr.endsWith (".type"))   return T ("TİP");
            if (idStr.endsWith (".hz"))     return T ("FREKANS");
            if (idStr.endsWith (".gain"))   return T ("KAZANÇ");
            if (idStr.endsWith (".q"))      return "Q";
            if (idStr.endsWith (".dyn"))    return T ("DİNAMİK");
            if (idStr.endsWith (".dynthr")) return T ("DYN EŞİK");
            if (idStr.endsWith (".dynrng")) return T ("DYN ARALIK");
            if (idStr.endsWith (".dynatk")) return T ("DYN ATAK");
            if (idStr.endsWith (".dynrel")) return T ("DYN BIRAKMA");
            if (idStr.endsWith (".on"))     return T ("AÇIK");
        }
        if (idStr == "swx.eq.hpf.on")      return T ("HPF AÇIK");
        if (idStr == "swx.eq.plos.on")     return T ("AÇIK");
        if (idStr == "swx.ds.post")        return T ("SON EŞİK");
        if (idStr == "swx.ds2.range")      return T ("SON ARALIK");
        if (idStr == "swx.ds2.on")         return T ("SİGORTA");
        if (idStr == "swx.eq.plos.hz")     return T ("KÖŞE");
        if (idStr == "swx.eq.plos.range")  return T ("DERİNLİK");   // genel .on'dan ÖNCE (denetim: dal ölü koddu)
        if (idStr == "swx.dirt.level")     return T ("SEVİYE");
        if (idStr == "swx.dirt.bits")      return T ("BİT");
        if (idStr == "swx.dirt.rate")      return T ("ÖRNEK TUT");
        if (idStr == "swx.layer.a")        return T ("SES A");
        if (idStr == "swx.layer.b")        return T ("SES B");
        if (idStr == "swx.layer.level")    return T ("SEVİYE");
        if (idStr == "swx.layer.grit")     return T ("GRİT");
        if (idStr == "swx.glow.depth")     return T ("DERİNLİK");
        if (idStr == "swx.glow.time")      return T ("ZAMAN");
        if (idStr.endsWith (".on"))        return T ("AÇIK");
        if (idStr.endsWith (".thresh"))    return T ("EŞİK");
        if (idStr.endsWith (".ratio"))     return T ("ORAN");
        if (idStr.endsWith (".knee"))      return T ("DİZ");
        if (idStr.endsWith (".attack"))    return T ("ATAK");
        if (idStr.endsWith (".release"))   return T ("BIRAKMA");
        if (idStr.endsWith (".makeup"))    return T ("TELAFİ");
        if (idStr.endsWith (".mix"))       return T ("KARIŞIM");
        if (idStr.endsWith (".detector"))  return T ("DEDEKTÖR");
        if (idStr.endsWith (".character")) return T ("KARAKTER");
        if (idStr.endsWith (".schpf"))     return T ("SC HPF");
        if (idStr.endsWith (".lookahead")) return T ("ÖNGÖRÜ");
        if (idStr == "swx.eq.hpf.hz")      return T ("HPF FREKANS");
        if (idStr == "swx.eq.hpf.slope")   return T ("HPF EĞİM");
        if (idStr.endsWith (".target"))    return T ("HEDEF");
        if (idStr.endsWith (".maxup"))     return T ("MAKS +");
        if (idStr.endsWith (".maxdown"))   return T ("MAKS −");
        if (idStr.endsWith (".speed"))     return T ("HIZ");
        if (idStr.endsWith (".range"))     return T ("ARALIK");
        if (idStr.endsWith (".hold"))      return T ("TUTMA");
        if (idStr.endsWith (".hyst"))      return T ("HİSTEREZİS");
        if (idStr.endsWith (".mode"))      return T ("MOD");
        if (idStr.endsWith (".link"))      return T ("BAĞ");
        if (idStr.endsWith (".drive"))     return T ("SÜRÜŞ");
        if (idStr.endsWith (".tone"))      return T ("TON");
        if (idStr.endsWith (".strength"))  return T ("GÜÇ");
        if (idStr.endsWith (".key"))       return T ("TON (KEY)");
        if (idStr.endsWith (".scale"))     return T ("GAM");
        if (idStr.endsWith (".spread"))    return T ("YAYILIM");
        if (idStr.endsWith (".detune"))    return T ("DETUNE");
        if (idStr.endsWith (".voices"))    return T ("SES SAYISI");
        if (idStr.endsWith (".hpf"))       return T ("HP KESİM");
        if (idStr.endsWith (".lp"))        return T ("LP KESİM");
        if (idStr.endsWith (".sync"))      return T ("BPM SENK");
        if (idStr.endsWith (".time"))      return T ("SÜRE");
        if (idStr.endsWith (".fb"))        return T ("GERİ BESLEME");
        if (idStr.endsWith (".duck"))      return T ("DUCKING");
        if (idStr.endsWith (".pre"))       return T ("ÖN GECİKME");
        if (idStr.endsWith (".decay"))     return T ("SÖNÜŞ");
        if (idStr.endsWith (".damp"))      return T ("SÖNÜMLEME");
        if (idStr.endsWith (".hp"))        return T ("HP KESİM");
        if (idStr.endsWith (".width"))     return T ("GENİŞLİK");
        if (idStr.endsWith (".amount"))    return T ("MİKTAR");
        if (idStr == "swx.input.trim")     return T ("GİRİŞ TRİM");
        if (idStr == "swx.output.trim")    return T ("ÇIKIŞ TRİM");
        if (idStr == "swx.out.ceiling")    return T ("TAVAN");
        if (idStr == "swx.out.release")    return T ("BIRAKMA");
        if (idStr == "swx.air.mid")        return T ("MID AIR");
        if (idStr == "swx.air.high")       return T ("HIGH AIR");
        if (idStr.endsWith (".hz"))        return T ("FREKANS");
        return tabloAd.toUpperCase();
    }
}
