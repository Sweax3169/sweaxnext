"""SWEAX NEXT — demo vokal üretici (README / site örnekleri).

Telifli hiçbir şey kullanmaz: sözler bu depo için yazıldı, ses Piper TTS'in "fahrettin" Türkçe sesiyle
(veri kümesi CC0) sentezlenir, beat ise burada sıfırdan üretilir. Sonra temiz TTS sesi bilerek bir ev kaydına
benzetilir: kutu gibi oda, kısa yankı, fan/vınlama gürültüsü, mikrofon patlamaları, sert s sesleri ve satırdan
satıra değişen seviye. SweaxNext'in işi bu kusurları bulup düzeltmek.

Kullanım:
    python demo_uret.py --piper <piper.exe> --model <tr_TR-fahrettin-medium.onnx> --cikis <klasör>
Çıktı (48 kHz):  ham_vokal.wav (mono, 24 bit) · beat.wav (stereo) · satirlar.json (satır zamanları)
"""
import argparse, json, os, subprocess, tempfile
import numpy as np
import soundfile as sf
from scipy import signal

SR = 48000
BPM = 140.0
OLCU = 4 * 60.0 / BPM            # bir ölçü (s); her satır iki ölçüye oturur
RNG = np.random.default_rng(3169)

SATIRLAR = [
    # (söz, satır kazancı dB, satır başında patlama)
    ("Bu gece yine stüdyodayım, ışıklar hiç sönmüyor.", 0.0, True),
    ("Defter açık, kalem hazır, kafamdaki ses susmuyor.", -6.0, False),
    ("Sokaklar sessiz, şehir uykuda, ben hâlâ yazıyorum.", 3.0, False),
    ("Bir nefes, bir satır daha, ritmi yakalıyorum.", -8.0, True),
    ("Para pul peşinde koşan çok, ben sözümü seçtim.", 2.0, True),
    ("Bas biraz, sesi aç, şimdi sıra bende.", -4.0, True),
    ("Her hece yerinde dursun, sesim sahnede.", 4.0, False),
    ("Sabaha kadar sürer bu, sen de kal benimle.", -5.0, False),
    ("Kulaklık takılı, ses bir tık yüksek, dur bakalım.", 1.0, False),
    ("Pes etmek yok, bir kayıt daha, bu sefer tam olacak.", -7.0, True),
    ("Söz uçar yazı kalır derler, ben ikisini de sakladım.", 3.0, False),
    ("Basit bir oda, eski bir mikrofon, bütün dünyam burada.", -3.0, True),
    ("Sesim sert, sözlerim keskin, nefesim hiç bitmiyor.", 5.0, False),
    ("Bas dönüyor, kafa sallıyor, saatler su gibi akıyor.", -6.0, True),
    ("Biri sorarsa söyle, bu gece burada iz bıraktık.", 2.0, True),
    ("Son satır, son nefes, şimdi kaydı durdur.", -4.0, False),
]


def tts(piper, model, metin, uzunluk=0.88):
    with tempfile.TemporaryDirectory() as d:
        yol = os.path.join(d, "s.wav")
        subprocess.run([piper, "-m", model, "-f", yol, "--length_scale", str(uzunluk),
                        "--noise_scale", "0.33", "--noise_w", "0.5", "--sentence_silence", "0.0", "-q"],
                       input=metin.encode("utf-8"), stdout=subprocess.DEVNULL, check=True)
        x, sr = sf.read(yol, dtype="float64")
    if x.ndim > 1:
        x = x.mean(axis=1)
    g = np.gcd(SR, sr)
    return signal.resample_poly(x, SR // g, sr // g)


def peaking(f0, gain_db, q):
    a = 10 ** (gain_db / 40)
    w = 2 * np.pi * f0 / SR
    al = np.sin(w) / (2 * q)
    b = [1 + al * a, -2 * np.cos(w), 1 - al * a]
    aa = [1 + al / a, -2 * np.cos(w), 1 - al / a]
    return np.array(b) / aa[0], np.array(aa) / aa[0]


def pembe(n):
    beyaz = RNG.standard_normal(n)
    b = [0.049922035, -0.095993537, 0.050612699, -0.004408786]
    a = [1, -2.494956002, 2.017265875, -0.522189400]
    return signal.lfilter(b, a, beyaz)


def patlama(uzunluk_s=0.09, hz=55.0):
    t = np.arange(int(uzunluk_s * SR)) / SR
    zarf = np.exp(-t / 0.022) * (1 - np.exp(-t / 0.002))
    return np.sin(2 * np.pi * hz * t * (1 + 0.6 * np.exp(-t / 0.03))) * zarf


def oda_ir():
    n = int(0.42 * SR)
    t = np.arange(n) / SR
    ir = RNG.standard_normal(n) * np.exp(-6.9 * t / 0.38)
    ir = signal.lfilter(*signal.butter(2, 4500 / (SR / 2)), ir)
    for gecikme, g in ((0.0041, 0.55), (0.0067, 0.42), (0.0093, 0.33), (0.0131, 0.25)):
        ir[int(gecikme * SR)] += g * 6
    ir[0] = 0
    return ir / np.max(np.abs(ir))


def ham_vokal(piper, model):
    zamanlar, toplam, onceki_son = [], [], 0
    for i, (metin, kazanc, pat) in enumerate(SATIRLAR):
        # bir ölçü oda sesi (mikrofon açık, henüz söz yok), sonra her satır iki ölçünün başına
        imlec = max(int(OLCU * (1 + 2 * i) * SR), onceki_son + int(0.25 * SR))
        x = tts(piper, model, metin, 0.9)
        x = x / (np.max(np.abs(x)) + 1e-9)
        # kelime başları: sessizlikten sonra enerji sıçraması (patlamaları buralara koy)
        zarf = np.convolve(np.abs(x), np.ones(480) / 480, mode="same")
        acik = zarf > 0.04
        baslar = np.flatnonzero(acik[1:] & ~acik[:-1]) + 1
        if pat:
            secilen = [baslar[0]] + [b for b in baslar[1:] if RNG.random() < 0.35]
            for b in secilen:
                p = patlama(hz=RNG.uniform(45, 75)) * RNG.uniform(0.55, 0.9)
                son = min(len(x), b + len(p))
                x[b:son] += p[: son - b]
        x *= 10 ** (kazanc / 20)
        toplam.append((imlec, x))
        zamanlar.append({"metin": metin, "bas": round(imlec / SR, 3), "son": round((imlec + len(x)) / SR, 3)})
        onceki_son = imlec + len(x)
    n = int(OLCU * (2 + 2 * len(SATIRLAR)) * SR)
    v = np.zeros(n)
    for bas, x in toplam:
        v[bas:bas + len(x)] += x
    # kutu oda + sert tiz: 330 Hz kabarık, 7 kHz civarı s sesleri batıyor
    for f0, g, q in ((330, 4.5, 1.0), (7200, 9.0, 1.3), (3100, 2.5, 1.5)):
        b, a = peaking(f0, g, q)
        v = signal.lfilter(b, a, v)
    v = v + 0.11 * signal.fftconvolve(v, oda_ir())[:n]
    v /= np.max(np.abs(v)) + 1e-9
    v *= 10 ** (-3.5 / 20)
    # gürültü tabanı: fan (pembe) + şebeke vınlaması
    t = np.arange(n) / SR
    fan = pembe(n)
    fan = fan / np.sqrt(np.mean(fan ** 2)) * 10 ** (-55 / 20)
    vin = sum(0.5 ** k * np.sin(2 * np.pi * 50 * (k + 1) * t) for k in range(4))
    vin = vin / np.sqrt(np.mean(vin ** 2)) * 10 ** (-63 / 20)
    return v + fan + vin, zamanlar


def beat(n, bpm=BPM):
    """Sıfırdan sentezlenen sade bir trap beat (808, clap, hi-hat, karanlık pad)."""
    t_vurus = 60.0 / bpm
    out = np.zeros((n, 2))
    sn = lambda s: int(s * SR)

    def ekle(x, bas, pan=0.0):
        son = min(n, bas + len(x))
        if bas >= n:
            return
        out[bas:son, 0] += x[: son - bas] * np.sqrt(0.5 * (1 - pan))
        out[bas:son, 1] += x[: son - bas] * np.sqrt(0.5 * (1 + pan))

    def ses808(hz, sure):
        t = np.arange(sn(sure)) / SR
        f = hz * (1 + 1.4 * np.exp(-t / 0.018))
        x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (sure * 0.55))
        return np.tanh(2.2 * x) * 0.8

    def clap():
        t = np.arange(sn(0.22)) / SR
        g = RNG.standard_normal(len(t))
        g = signal.lfilter(*signal.butter(2, [900 / (SR / 2), 6000 / (SR / 2)], "band"), g)
        zarf = np.exp(-t / 0.05)
        for k in (0.0, 0.011, 0.022):
            zarf += 0.6 * np.exp(-np.maximum(t - k, 0) / 0.006) * (t >= k)
        return g * zarf * 0.35

    def hat(sure=0.05, acik=False):
        t = np.arange(sn(sure)) / SR
        g = RNG.standard_normal(len(t))
        g = signal.lfilter(*signal.butter(2, 7500 / (SR / 2), "high"), g)
        return g * np.exp(-t / (0.06 if acik else 0.012)) * 0.12

    bas_notalar = [43.65, 43.65, 51.91, 38.89]  # F1, F1, G#1, D#1 (fa minör çevresi)
    olcu = 4 * t_vurus
    olcu_sayisi = int(np.ceil(n / SR / olcu))
    for o in range(olcu_sayisi):
        o0 = o * olcu
        hz = bas_notalar[o % 4]
        for vurus, uzun in ((0.0, 1.4), (1.5, 0.45), (2.75, 1.1)):
            ekle(ses808(hz, uzun * t_vurus), sn(o0 + vurus * t_vurus))
        for vurus in (1.0, 3.0):
            ekle(clap(), sn(o0 + vurus * t_vurus), 0.05)
        adim = t_vurus / 2
        for k in range(8):
            ekle(hat(), sn(o0 + k * adim), 0.25)
        if o % 2 == 1:  # ölçü sonunda hi-hat yuvarlaması (1/32)
            for k in range(6):
                ekle(hat() * (0.6 + 0.08 * k), sn(o0 + 3.25 * t_vurus + k * t_vurus / 8), 0.25)
    # karanlık pad: fa minör akorları, yavaş giriş
    akorlar = [(174.61, 207.65, 261.63), (174.61, 207.65, 261.63), (207.65, 261.63, 311.13), (155.56, 196.0, 233.08)]
    for o in range(olcu_sayisi):
        t = np.arange(sn(olcu)) / SR
        x = sum(signal.sawtooth(2 * np.pi * f * t * (1 + d)) for f in akorlar[o % 4] for d in (-0.003, 0.003))
        x = signal.lfilter(*signal.butter(2, 1400 / (SR / 2)), x)
        x *= np.minimum(1, t / 0.4) * np.minimum(1, (olcu - t) / 0.3) * 0.022
        ekle(x, sn(o * olcu), -0.3)
        ekle(x, sn(o * olcu), 0.3)
    out /= np.max(np.abs(out)) + 1e-9
    return out * 10 ** (-6 / 20)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--piper", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--cikis", required=True)
    a = ap.parse_args()
    os.makedirs(a.cikis, exist_ok=True)
    v, zamanlar = ham_vokal(a.piper, a.model)
    sf.write(os.path.join(a.cikis, "ham_vokal.wav"), v.astype(np.float32), SR, subtype="PCM_24")
    sf.write(os.path.join(a.cikis, "beat.wav"), beat(len(v)).astype(np.float32), SR, subtype="PCM_24")
    with open(os.path.join(a.cikis, "satirlar.json"), "w", encoding="utf-8") as f:
        json.dump(zamanlar, f, ensure_ascii=False, indent=1)
    print("tamam:", len(v) / SR, "sn")


if __name__ == "__main__":
    main()
