#pragma once
// SWEAX NEXT — Tests/Cati.h : bağımlılıksız mini test çatısı
//  SWX_TEST(ad) { ... }             — test tanımı (otomatik kayıt)
//  SWX_BEKLE(kosul)                 — doğru olmalı
//  SWX_BEKLE_YAKIN(a, b, tol)       — |a-b| <= tol
//  SWX_BEKLE_EQ(a, b)               — eşit
//  Çalıştırıcı: sweax::test::hepsiniCalistir(argc, argv) → başarısız sayısı (çıkış kodu)
//  Filtre: SweaxTests <alt-dize>  yalnız adı eşleşenleri koşturur.
#ifndef _CRT_SECURE_NO_WARNINGS
 #define _CRT_SECURE_NO_WARNINGS
#endif
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdlib>

namespace sweax::test
{
    struct Durum { int basarisiz = 0; int kontrol = 0; std::string sonHata; };

    struct Kayit { const char* ad; void (*fn) (Durum&); };
    inline std::vector<Kayit>& kayitlar() { static std::vector<Kayit> k; return k; }
    struct Kaydedici { Kaydedici (const char* ad, void (*fn) (Durum&)) { kayitlar().push_back ({ ad, fn }); } };

    inline int hepsiniCalistir (int argc, char** argv)
    {
        std::string filtre = argc > 1 ? argv[1] : "";
        int gecti = 0, kaldi = 0, atlandi = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (auto& k : kayitlar())
        {
            if (! filtre.empty() && std::string (k.ad).find (filtre) == std::string::npos) { ++atlandi; continue; }
            Durum d;
            if (std::getenv ("SWX_VERBOSE")) { std::printf ("--- %s\n", k.ad); std::fflush (stdout); }
            const auto ta = std::chrono::steady_clock::now();
            try { k.fn (d); }
            catch (const std::exception& e) { ++d.basarisiz; d.sonHata = std::string ("istisna: ") + e.what(); }
            catch (...) { ++d.basarisiz; d.sonHata = "bilinmeyen istisna"; }
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - ta).count();
            if (d.basarisiz == 0) { ++gecti; std::printf ("PASS  %-56s (%d kontrol, %.0f ms)\n", k.ad, d.kontrol, ms); }
            else { ++kaldi; std::printf ("FAIL  %-56s %d/%d kontrol düştü — %s\n", k.ad, d.basarisiz, d.kontrol, d.sonHata.c_str()); }
        }
        const double top = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("\nSWEAX TESTS: %d PASS / %d FAIL%s  (%.2f s)\n", gecti, kaldi,
                     atlandi ? (" / " + std::to_string (atlandi) + " atlandı").c_str() : "", top);
        return kaldi;
    }
}

#define SWX_TEST(ad) \
    static void swx_test_##ad (sweax::test::Durum& _d); \
    static sweax::test::Kaydedici swx_kayit_##ad (#ad, &swx_test_##ad); \
    static void swx_test_##ad (sweax::test::Durum& _d)

#define SWX_BEKLE(kosul) do { ++_d.kontrol; if (! (kosul)) { ++_d.basarisiz; \
    _d.sonHata = std::string (#kosul) + " [" + __FILE__ + ":" + std::to_string (__LINE__) + "]"; } } while (0)

#define SWX_BEKLE_YAKIN(a, b, tol) do { ++_d.kontrol; const double _va = (double) (a), _vb = (double) (b); \
    if (! (std::fabs (_va - _vb) <= (double) (tol))) { ++_d.basarisiz; char _buf[256]; \
    std::snprintf (_buf, sizeof (_buf), "%s=%.6g beklenen %s=%.6g (tol %.3g) [%s:%d]", #a, _va, #b, _vb, (double) (tol), __FILE__, __LINE__); \
    _d.sonHata = _buf; } } while (0)

#define SWX_BEKLE_EQ(a, b) do { ++_d.kontrol; if (! ((a) == (b))) { ++_d.basarisiz; \
    _d.sonHata = std::string (#a " == " #b) + " [" + __FILE__ + ":" + std::to_string (__LINE__) + "]"; } } while (0)
