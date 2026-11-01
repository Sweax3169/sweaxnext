"""README ve site medyası: SweaxShots çıktısından WebP/GIF/MP4, demo sesinden önce/sonra spektrogramı,
sosyal önizleme görseli ve site simgesi.

Kullanım: python Tools/art/medya.py <SweaxShots çıkışı> <demo klasörü> <fonts klasörü> <docs> [--ffmpeg ffmpeg]
"""
import io, json, os, subprocess, sys
import numpy as np
import soundfile as sf
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from scipy import signal

BG = (11, 14, 20)
INK = (240, 235, 225)
INK2 = (169, 176, 189)
INK3 = (109, 117, 133)
GOLD = (226, 181, 88)
TEAL = (74, 185, 196)


def ffmpeg_calistir(ff, *args):
    subprocess.run([ff, "-loglevel", "error", "-y", *args], check=True)


def ttf(fonts, ad, eksen, boy):
    """woff2 değişken yazı tipinden PIL'in okuyabileceği statik TTF örneği."""
    f = TTFont(os.path.join(fonts, ad))
    f = instancer.instantiateVariableFont(f, eksen)
    f.flavor = None
    buf = io.BytesIO()
    f.save(buf)
    buf.seek(0)
    return ImageFont.truetype(buf, boy)


def webp(src, dst, en=1600):
    im = Image.open(src).convert("RGB")
    if im.width > en:
        im = im.resize((en, round(im.height * en / im.width)), Image.LANCZOS)
    im.save(dst, "WEBP", quality=88, method=6)


def video(ff, mkv, mp4, gif, poster, gif_en=900, gif_fps=12):
    ffmpeg_calistir(ff, "-i", mkv, "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "slow",
                    "-movflags", "+faststart", "-an", mp4)
    palet = mkv + ".palet.png"
    filtre = f"fps={gif_fps},scale={gif_en}:-1:flags=lanczos"
    ffmpeg_calistir(ff, "-i", mkv, "-vf", f"{filtre},palettegen=max_colors=200:stats_mode=diff", palet)
    ffmpeg_calistir(ff, "-i", mkv, "-i", palet, "-lavfi", f"{filtre}[x];[x][1:v]paletteuse=dither=sierra2_4a:diff_mode=rectangle",
                    "-loop", "0", gif)
    os.remove(palet)
    ffmpeg_calistir(ff, "-sseof", "-1.5", "-i", mkv, "-frames:v", "1", poster + ".png")
    webp(poster + ".png", poster, 1180)
    os.remove(poster + ".png")


def spektrogram(x, sr, gen, yuk, fmin=60.0, fmax=16000.0):
    f, t, Z = signal.stft(x, sr, nperseg=2048, noverlap=2048 - 256, window="hann")
    P = 20 * np.log10(np.abs(Z) + 1e-9)
    # log frekans ekseni
    hedef_f = fmin * (fmax / fmin) ** (np.arange(yuk)[::-1] / (yuk - 1))
    satir = np.interp(hedef_f, f, np.arange(len(f)))
    S = np.array([np.interp(satir, np.arange(len(f)), P[:, k]) for k in range(P.shape[1])]).T
    kol = np.linspace(0, S.shape[1] - 1, gen)
    S = np.array([np.interp(kol, np.arange(S.shape[1]), r) for r in S])
    return S


def renk_haritasi(S, ust_db, alt=-95.0):
    v = np.clip((S - alt) / (ust_db - alt), 0, 1)
    duraklar = np.array([[11, 14, 20], [24, 40, 60], [36, 110, 122], [74, 185, 196], [226, 181, 88], [250, 240, 220]], float)
    konum = np.linspace(0, 1, len(duraklar))
    out = np.zeros(S.shape + (3,))
    for c in range(3):
        out[..., c] = np.interp(v, konum, duraklar[:, c])
    return out.astype(np.uint8)


def once_sonra(demo, fonts, dst):
    satirlar = json.load(open(os.path.join(demo, "satirlar.json"), encoding="utf-8"))
    ham, sr = sf.read(os.path.join(demo, "ham_vokal.wav"))
    isl, _ = sf.read(os.path.join(demo, "stil", "temiz.wav"))
    isl = isl.mean(axis=1) if isl.ndim > 1 else isl
    t0, t1 = satirlar[2]["bas"] - 0.4, satirlar[5]["son"] + 0.4          # 3.–6. satırlar
    a, b = int(t0 * sr), int(t1 * sr)
    W, H, pad, sh = 1680, 830, 56, 290
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    baslik = ttf(fonts, "bricolage-latin.woff2", {"wght": 700, "opsz": 48, "wdth": 100}, 34)
    gov = ttf(fonts, "hanken-latin.woff2", {"wght": 520}, 22)
    kucuk = ttf(fonts, "hanken-latin.woff2", {"wght": 450}, 19)
    ust = max(np.percentile(spektrogram(ham[a:b], sr, 400, 120), 99.7), -20)
    for i, (x, ad, renk) in enumerate(((ham[a:b], "Raw take  ·  ham kayıt", GOLD), (isl[a:b], "Temiz (Clean) style  ·  Temiz stil", TEAL))):
        y0 = pad + 40 + i * (sh + 70)
        S = spektrogram(x, sr, W - 2 * pad - 70, sh)
        im.paste(Image.fromarray(renk_haritasi(S, ust)), (pad + 70, y0))
        d.text((pad + 70, y0 - 38), ad, font=baslik, fill=renk)
        for f in (100, 1000, 10000):
            yy = y0 + sh - 1 - (np.log(f / 60.0) / np.log(16000 / 60.0)) * (sh - 1)
            d.text((pad + 62, yy - 11), (f"{f // 1000}k" if f >= 1000 else str(f)), font=kucuk, fill=INK3, anchor="ra")
            d.line([(pad + 66, yy), (pad + 70, yy)], fill=INK3)
    d.text((W - pad, H - 26), "60 Hz – 16 kHz, log  ·  lines 3–6 of the demo take  ·  demo kaydın 3.–6. satırları",
           font=kucuk, fill=INK3, anchor="rs")
    im.save(dst, "WEBP", quality=90, method=6)


def og(shots, fonts, dst):
    W, H = 1200, 630
    im = Image.new("RGB", (W, H), BG)
    ekran = Image.open(os.path.join(shots, "sekme-zincir.png")).convert("RGB")
    ekran = ekran.resize((760, round(ekran.height * 760 / ekran.width)), Image.LANCZOS)
    im.paste(ekran, (W - 760 + 60, 120))
    d = ImageDraw.Draw(im)
    b = ttf(fonts, "bricolage-latin.woff2", {"wght": 780, "opsz": 96, "wdth": 100}, 92)
    g = ttf(fonts, "hanken-latin.woff2", {"wght": 520}, 30)
    d.text((64, 150), "Sweax", font=b, fill=INK)
    d.text((64 + d.textlength("Sweax", font=b), 150), "Next", font=b, fill=TEAL)
    d.text((66, 280), "Hears the take first,", font=g, fill=INK)
    d.text((66, 320), "then fixes only", font=g, fill=INK)
    d.text((66, 360), "what needs fixing.", font=g, fill=INK)
    d.text((66, 440), "VST3 · Windows · GPLv3", font=g, fill=INK3)
    im.save(dst, optimize=True)


def favicon(dst, n=128):
    yy, xx = np.mgrid[0:n, 0:n] + 0.5
    cx = cy = n / 2
    r = np.hypot(xx - cx, yy - cy)
    aci = (np.degrees(np.arctan2(yy - cy, xx - cx)) + 360 + 60) % 360 / 360
    k = 0.5 - 0.5 * np.cos(2 * np.pi * aci)
    rgb = np.stack([np.array(GOLD)[c] * (1 - k) + np.array(TEAL)[c] * k for c in range(3)], -1)
    alfa = np.clip(n * 0.40 - r, 0, 1) * np.clip(r - n * 0.18, 0, 1)
    img = np.dstack([rgb, alfa * 255]).astype(np.uint8)
    Image.fromarray(img, "RGBA").resize((64, 64), Image.LANCZOS).save(dst)


def main():
    shots, demo, fonts, docs = sys.argv[1:5]
    ff = sys.argv[sys.argv.index("--ffmpeg") + 1] if "--ffmpeg" in sys.argv else "ffmpeg"
    media = os.path.join(docs, "media")
    os.makedirs(media, exist_ok=True)
    for f in sorted(os.listdir(shots)):
        if f.endswith(".png"):
            webp(os.path.join(shots, f), os.path.join(media, f[:-4] + ".webp"))
            print(" ", f[:-4] + ".webp")
    video(ff, os.path.join(shots, "tur.mkv"), os.path.join(media, "tour.mp4"), os.path.join(media, "tour.gif"),
          os.path.join(media, "tour-poster.webp"))
    video(ff, os.path.join(shots, "stiller.mkv"), os.path.join(media, "styles.mp4"), os.path.join(media, "styles.gif"),
          os.path.join(media, "styles-poster.webp"))
    once_sonra(demo, fonts, os.path.join(media, "before-after.webp"))
    og(shots, fonts, os.path.join(media, "og.png"))
    favicon(os.path.join(docs, "favicon.png"))
    for f in sorted(os.listdir(media)):
        print(f"{os.path.getsize(os.path.join(media, f)) / 1024:9.0f} KB  {f}")


if __name__ == "__main__":
    main()
