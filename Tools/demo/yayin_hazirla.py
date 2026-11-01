"""Render sonuçlarını README ve site için hazırlar.

Girdi  : demo_uret.py + render_hepsi.py çıktısı (ham_vokal.wav, beat.wav, satirlar.json, stil/<ad>.wav + .json)
Çıktı  : docs/audio/  ham.mp3, <stil>.mp3, beat.mp3 (aynı kesit) + demo.json (ölçümler, teşhisler, zincir sırası,
         dalga zarfları). Her sayı ya SweaxRender'ın kendi raporundan ya da burada ham WAV'lardan ölçülür.
Kullanım: python yayin_hazirla.py <demo klasörü> <docs/audio> [--ffmpeg ffmpeg]
"""
import json, os, subprocess, sys
import numpy as np
import soundfile as sf
from scipy import signal

STILLER = ["dogal", "temiz", "trap", "agresif", "killoki", "reckol", "ati242", "uzi", "motive",
           "kava", "ezhel", "ceza", "allame", "sehinsah", "sagopa", "benfero"]
AD = {"dogal": "Doğal", "temiz": "Temiz", "trap": "Trap", "agresif": "Agresif", "killoki": "Killoki", "reckol": "Reçkol",
      "ati242": "Ati242", "uzi": "Uzi", "motive": "Motive", "kava": "Kava", "ezhel": "Ezhel", "ceza": "Ceza",
      "allame": "Allame", "sehinsah": "Şehinşah", "sagopa": "Sagopa Kajmer", "benfero": "Ben Fero"}
PROBLEM = ["Clipping", "LowLevel", "DcOffset", "NoiseFloor", "Hum", "Hiss", "LfNoise", "Boom", "Proximity", "Mud", "Box",
           "Nasal", "Harsh", "Dull", "Bright", "Thin", "Resonance", "MacroDynamics", "MicroDynamics", "OverCompressed",
           "Sibilance", "Plosive"]
KESIT = (1, 8)          # 1.–8. satırlar (ilk dörtlük + nakaratın yarısı)


def mono(x):
    return x.mean(axis=1) if x.ndim > 1 else x


def db(v):
    return float(20 * np.log10(max(v, 1e-9)))


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)))) if len(x) else 0.0


def hizala(ref, x, sr):
    """İşlenmiş çıkışın ham girişe göre kaymasını ilk 6 sn'de çapraz ilintiyle bul (SweaxRender telafili olmalı)."""
    n = min(len(ref), len(x), int(6 * sr))
    c = signal.correlate(x[:n], ref[:n], mode="full", method="fft")
    return int(np.argmax(np.abs(c)) - (n - 1))


def olc(x, sr, satirlar):
    hp = signal.butter(2, 100 / (sr / 2), "high", output="sos")
    xh = signal.sosfilt(hp, x)
    satir_db = [db(rms(xh[int(s["bas"] * sr):int(s["son"] * sr)])) for s in satirlar]
    bosluk = []
    for a, b in zip(satirlar[:-1], satirlar[1:]):
        i0, i1 = int((a["son"] + 0.15) * sr), int((b["bas"] - 0.05) * sr)
        if i1 - i0 > int(0.1 * sr):
            bosluk.append(db(rms(x[i0:i1])))
    intro = db(rms(x[int(0.2 * sr):int((satirlar[0]["bas"] - 0.1) * sr)]))
    # alçak frekans darbesi: 20-120 Hz bandının 20 ms'lik en yüksek RMS'i, gövdenin (300-3k) medyanına göre
    lf = signal.sosfilt(signal.butter(4, [20 / (sr / 2), 120 / (sr / 2)], "band", output="sos"), x)
    govde = signal.sosfilt(signal.butter(2, [300 / (sr / 2), 3000 / (sr / 2)], "band", output="sos"), x)
    s_bant = signal.sosfilt(signal.butter(4, [5000 / (sr / 2), 10000 / (sr / 2)], "band", output="sos"), x)
    w = int(0.02 * sr)
    kare = lambda y: np.sqrt(np.convolve(np.square(y), np.ones(w) / w, mode="valid")[::w // 2])
    lf_k, gv_k, s_k = kare(lf), kare(govde), kare(s_bant)
    sesli = gv_k > np.percentile(gv_k, 95) * 10 ** (-20 / 20)
    govde_med = np.median(gv_k[sesli])
    return {
        "satirDb": [round(v, 2) for v in satir_db],
        "satirFarkDb": round(max(satir_db) - min(satir_db), 1),
        "satirSapmaDb": round(float(np.std(satir_db)), 2),
        "boslukDb": round(float(np.median(bosluk)), 1),
        "introDb": round(intro, 1),
        "lfDarbeDb": round(db(np.max(lf_k)) - db(govde_med), 1),
        "sTepeDb": round(db(np.percentile(s_k[sesli], 99)) - db(govde_med), 1),
    }


def zarf(x, sr, t0, t1, n=720):
    seg = x[int(t0 * sr):int(t1 * sr)]
    k = len(seg) // n
    seg = np.abs(seg[:k * n]).reshape(n, k)
    return [round(float(v), 4) for v in seg.max(axis=1)]


def mp3(ffmpeg, x, sr, yol, kbps):
    gecici = yol + ".wav"
    sf.write(gecici, x.astype(np.float32), sr, subtype="FLOAT")
    subprocess.run([ffmpeg, "-loglevel", "error", "-y", "-i", gecici, "-codec:a", "libmp3lame", "-b:a", f"{kbps}k",
                    "-map_metadata", "-1", yol], check=True)
    os.remove(gecici)


def main():
    kok, hedef = sys.argv[1], sys.argv[2]
    ffmpeg = sys.argv[sys.argv.index("--ffmpeg") + 1] if "--ffmpeg" in sys.argv else "ffmpeg"
    os.makedirs(hedef, exist_ok=True)
    satirlar = json.load(open(os.path.join(kok, "satirlar.json"), encoding="utf-8"))
    ham, sr = sf.read(os.path.join(kok, "ham_vokal.wav"))
    beat, _ = sf.read(os.path.join(kok, "beat.wav"))
    t0 = round(satirlar[KESIT[0] - 1]["bas"] - 0.35, 3)
    t1 = round(satirlar[KESIT[1] - 1]["son"] + 0.6, 3)
    kes = lambda y: y[int(t0 * sr):int(t1 * sr)]
    sol = lambda s: 0.5 * (1 - np.cos(np.pi * np.clip(s, 0, 1)))
    def yumusak(y):
        y = y.copy(); f = int(0.03 * sr)
        y[:f] = (y[:f].T * sol(np.arange(f) / f)).T
        y[-f:] = (y[-f:].T * sol(np.arange(f)[::-1] / f)).T
        return y

    veri = {"kesit": [t0, t1], "satirlar": satirlar, "sr": sr, "stiller": []}
    veri["ham"] = {"olcum": olc(ham, sr, satirlar), "zarf": zarf(ham, sr, t0, t1)}
    mp3(ffmpeg, yumusak(kes(ham)), sr, os.path.join(hedef, "ham.mp3"), 96)
    mp3(ffmpeg, yumusak(kes(beat)), sr, os.path.join(hedef, "beat.mp3"), 128)

    for ad in STILLER:
        if not os.path.exists(os.path.join(kok, "stil", ad + ".json")):
            print(f"UYARI {ad}: render yok, atlandı")
            continue
        js = json.load(open(os.path.join(kok, "stil", ad + ".json"), encoding="utf-8"))
        y, _ = sf.read(os.path.join(kok, "stil", ad + ".wav"))
        # SweaxRender gecikme telafili yazar. Doubler/reverb/tune çapraz ilintide birkaç ms'lik sahte tepe
        # üretebildiği için küçük değerler kaydırılmaz; 20 ms üstü gerçek bir hiza hatasıdır.
        kayma = hizala(ham, mono(y), sr)
        if abs(kayma) > int(0.02 * sr):
            raise SystemExit(f"HATA {ad}: çıkış {kayma} örnek kaymış, render gecikme telafili değil")
        mp3(ffmpeg, yumusak(kes(y)), sr, os.path.join(hedef, ad + ".mp3"), 128)
        plan = js["plan"]
        moduller = {m["id"]: {"acik": m["acik"], "p": {p["id"]: round(p["deger"], 3) for p in m["parametreler"]}}
                    for m in plan["moduller"]}
        teshis = lambda r: [{"tip": PROBLEM[t["tip"]], "p": round(t["p"], 2), "s": round(t["s"], 2), "c": round(t["c"], 2),
                             "hz": round(t["hz"]), "deger": round(t["deger"], 1), "adet": t["adet"]} for t in r["teshisler"]]
        alan = ("integratedLufs", "voicedLufs", "truePeakDb", "noiseFloorDb", "snrDb", "crestMedianDb", "sibilantEvents",
                "voicedSec", "sentences", "guven")
        veri["stiller"].append({
            "id": ad, "ad": AD[ad], "sanatci": STILLER.index(ad) >= 4,
            "giris": {k: round(js["giris"][k], 2) for k in alan}, "girisTeshis": teshis(js["giris"]),
            "cikis": {k: round(js["cikis"][k], 2) for k in alan}, "cikisTeshis": teshis(js["cikis"]),
            "feedback": {"tur": plan["tur"], "secilen": plan["secilen"], "skor0": round(plan["skor0"], 3),
                         "skor": round(plan["skorSecilen"], 3)},
            "abGainDb": round(plan["abGainDb"], 2), "inputTrimDb": round(plan["inputTrimDb"], 2),
            "sira": plan["sira"], "moduller": moduller,
            "olcum": olc(mono(y), sr, satirlar), "zarf": zarf(mono(y), sr, t0, t1),
        })
        print(f"{ad:9s} LUFS {js['giris']['integratedLufs']:6.1f} -> {js['cikis']['integratedLufs']:6.1f}  "
              f"SNR {js['giris']['snrDb']:5.1f} -> {js['cikis']['snrDb']:5.1f}")
    with open(os.path.join(hedef, "demo.json"), "w", encoding="utf-8") as f:
        json.dump(veri, f, ensure_ascii=False, separators=(",", ":"))
    print("kesit", t0, t1, "sn")


if __name__ == "__main__":
    main()
