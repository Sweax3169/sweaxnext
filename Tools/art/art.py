"""SweaxNext README / site sanatı: hareketli SVG'ler (koyu + açık, EN + TR).

Kullanım: python Tools/art/art.py docs/fonts docs/audio/demo.json docs/art
Yazı tipleri (Bricolage Grotesque, Hanken Grotesk; SIL OFL) kullanılan karakterlere indirgenip SVG'ye gömülür.
Dalga biçimleri, sayılar ve modül durumları demo.json'dan gelir: demo vokal SweaxRender ile işlendiğinde
eklentinin kendi raporu ve kendi planı. Elle uydurulmuş sayı yok.
"""
import base64, io, json, math, os, re, sys
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

T = {
    "dark": dict(bg="#0b0e14", bg2="#131823", panel="#161b24", panel2="#1d2330", line="#2a3243", ink="#f0ebe1",
                 ink2="#a9b0bd", ink3="#6d7585", gold="#e2b558", teal="#4ab9c4", red="#e0574a", green="#4fc38a",
                 glow=".20", shade=".55"),
    "light": dict(bg="#f6f2ea", bg2="#ece5d9", panel="#fffdf9", panel2="#f1ebe1", line="#dcd3c4", ink="#161a22",
                  ink2="#4e5562", ink3="#8a909b", gold="#a8761b", teal="#1c7f89", red="#c4473a", green="#2b8f60",
                  glow=".14", shade=".12"),
}

L = {
    "en": dict(
        kicker="Vocal mix plug-in · VST3 · Windows",
        tag1="Hears the take first,", tag2="then fixes only what needs fixing.",
        facts="20 modules  ·  16 styles  ·  open source, GPLv3",
        on="on", off="not needed", heard="heard", raw="raw take", done="processed",
        pop="pop", ess="harsh s", noise="room noise",
        c_listen=("Listens first", "Rap the verse once. It measures every phrase."),
        c_diag=("Names the problem", "Pops, harsh esses, room noise, level jumps."),
        c_verify=("Checks its own work", "Renders the plan offline, measures, adjusts."),
        c_styles=("16 styles", "Each one has its own chain order."),
        c_ab=("Level-matched A/B", "Original and processed at the same loudness."),
        c_rt=("Real time", "Live mode for tracking, High for the mix."),
        sec="s", phrases="phrases", esses="s-sounds", conf="confidence",
        plosive="Plosives", sib="Sibilance", floor="Noise floor", lvl="Level jumps", bright="Too bright", boom="Boom",
        plan="plan", render="render", measure="measure", round="round", kept="kept", score_h="distance to target, lower is better",
        orig="ORIGINAL", proc="PROCESSED", same="same loudness",
        live="Live", high="High", ultra="Ultra", latency="latency at 48 kHz", cpu="of one CPU core", cpu2="High, 48 kHz",
        chain_t="Signal chain", chain_s="Clean style on the demo take: what ran, and what it decided wasn't needed.",
        fixed="fixed position", legend_on="on", legend_off="not needed",
        arch_t="How it decides", arch_s="Five layers, three threads. The audio thread never waits for analysis.",
        a_audio="Audio thread", a_worker="Analysis thread", a_msg="Message thread",
        a_host="Host", a_trim="Input trim", a_chain="Vocal chain", a_lim="Limiter", a_out="Out", a_dry="Dry path for A/B and DELTA",
        a_fifo="Capture FIFO", a_store="Capture store", a_meas="Measure", a_meas2="STFT · VAD · pitch · loudness",
        a_det="Diagnose", a_det2="22 problem checks", a_plan="Decide", a_plan2="planner + style recipe",
        a_fb="Verify", a_fb2="sandbox render, re-measure", a_pub="Publish", a_pub2="plan → parameters",
        a_apvts="Parameters", a_apvts2="automatable, undoable", a_cache="Param cache", a_ui="Editor",
    ),
    "tr": dict(
        kicker="Vokal mix eklentisi · VST3 · Windows",
        tag1="Önce kaydı dinler,", tag2="sadece gerekeni düzeltir.",
        facts="20 modül  ·  16 stil  ·  açık kaynak, GPLv3",
        on="açık", off="gerekmedi", heard="dinlendi", raw="ham kayıt", done="işlenmiş",
        pop="patlama", ess="sert s", noise="oda gürültüsü",
        c_listen=("Önce dinler", "Verse'ü bir kez oku. Her cümleyi ölçer."),
        c_diag=("Sorunu adıyla bulur", "Patlama, sert s, oda gürültüsü, seviye zıplaması."),
        c_verify=("Kendi işini ölçer", "Planı arka planda işler, ölçer, düzeltir."),
        c_styles=("16 stil", "Her birinin kendi zincir sırası var."),
        c_ab=("Eş seviyede A/B", "Orijinal ve işlenmiş aynı yükseklikte."),
        c_rt=("Gerçek zamanlı", "Kayıtta Canlı, mixte Yüksek."),
        sec="sn", phrases="cümle", esses="s sesi", conf="güven",
        plosive="Patlama", sib="Sert s", floor="Gürültü tabanı", lvl="Seviye farkı", bright="Fazla parlak", boom="Uğultu",
        plan="plan", render="işle", measure="ölç", round="tur", kept="seçildi", score_h="hedefe uzaklık, düşük olan iyi",
        orig="ORİJİNAL", proc="İŞLENMİŞ", same="aynı yükseklik",
        live="Canlı", high="Yüksek", ultra="Ultra", latency="gecikme, 48 kHz", cpu="tek işlemci çekirdeği", cpu2="Yüksek, 48 kHz",
        chain_t="Sinyal zinciri", chain_s="Demo kayıtta Temiz stil: neyin çalıştığı, neyin gerekmediği.",
        fixed="sabit yer", legend_on="açık", legend_off="gerekmedi",
        arch_t="Nasıl karar verir", arch_s="Beş katman, üç iş parçacığı. Ses hattı analizi asla beklemez.",
        a_audio="Ses iş parçacığı", a_worker="Analiz iş parçacığı", a_msg="Mesaj iş parçacığı",
        a_host="DAW", a_trim="Giriş trimi", a_chain="Vokal zinciri", a_lim="Limiter", a_out="Çıkış", a_dry="A/B ve DELTA için kuru yol",
        a_fifo="Yakalama FIFO", a_store="Yakalama deposu", a_meas="Ölç", a_meas2="STFT · VAD · perde · yükseklik",
        a_det="Teşhis", a_det2="22 sorun kontrolü", a_plan="Karar", a_plan2="planlayıcı + stil reçetesi",
        a_fb="Doğrula", a_fb2="sanal işle, yeniden ölç", a_pub="Yayınla", a_pub2="plan → parametreler",
        a_apvts="Parametreler", a_apvts2="otomasyon, geri al", a_cache="Param önbelleği", a_ui="Arayüz",
    ),
}

MODUL_AD = {
    "en": {"gate": "Gate", "tune": "Tune", "eq": "EQ", "teq": "Tone EQ", "lev": "Leveler", "compa": "Comp A",
           "ds": "De-esser", "compb": "Comp B", "glow": "Glow", "sat": "Character", "dirt": "Dirt", "air": "Air",
           "ds2": "De-ess 2", "layer": "Layer", "soo": "Smooth", "frm": "Formant", "dbl": "Doubler", "dly": "Delay",
           "rev": "Reverb", "wid": "Width", "out": "Limiter"},
    "tr": {"gate": "Kapı", "tune": "Tune", "eq": "EQ", "teq": "Ton EQ", "lev": "Seviyeleyici", "compa": "Komp A",
           "ds": "De-esser", "compb": "Komp B", "glow": "Glow", "sat": "Karakter", "dirt": "Kir", "air": "Hava",
           "ds2": "De-ess 2", "layer": "Katman", "soo": "Yumuşat", "frm": "Formant", "dbl": "Doubler", "dly": "Delay",
           "rev": "Reverb", "wid": "Genişlik", "out": "Limiter"},
}

CHARS = ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:;·+−-→←↻%()/×'’…"
         "ÇĞİÖŞÜçğıöşüâîû")


def font_faces(fonts, text):
    """İki aile: D (Bricolage, başlık) ve B (Hanken, gövde). latin + latin-ext ayrı yüzler, unicode-range ile."""
    chars = "".join(sorted(set(text + CHARS)))
    css = ""
    aileler = [
        ("D", 760, "bricolage-{}.woff2", {"wght": 760, "opsz": 96, "wdth": 100}),
        ("D", 600, "bricolage-{}.woff2", {"wght": 600, "opsz": 24, "wdth": 100}),
        ("B", 400, "hanken-{}.woff2", {"wght": 420}),
        ("B", 600, "hanken-{}.woff2", {"wght": 620}),
    ]
    araliklar = {"latin": "U+0000-00FF,U+0131,U+2000-206F,U+2190-2193,U+2212,U+21BB",
                 "latin-ext": "U+0100-0130,U+0132-024F"}
    for aile, agirlik, desen, eksen in aileler:
        for alt, aralik in araliklar.items():
            yol = os.path.join(fonts, desen.format(alt))
            f = TTFont(yol)
            f = instancer.instantiateVariableFont(f, eksen)
            opts = subset.Options()
            opts.flavor = "woff2"
            opts.layout_features = ["kern", "liga", "tnum"]
            sub = subset.Subsetter(options=opts)
            sub.populate(text=chars)
            sub.subset(f)
            buf = io.BytesIO()
            f.flavor = "woff2"
            f.save(buf)
            b64 = base64.b64encode(buf.getvalue()).decode()
            css += (f"@font-face{{font-family:{aile};font-weight:{agirlik};unicode-range:{aralik};"
                    f"src:url(data:font/woff2;base64,{b64}) format('woff2')}}")
    return css


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def svg(w, h, body, css, t, label):
    # CSS'teki fill sunum niteliğini ezer → renk yalnız nitelikte; rengi verilmemiş yazıya varsayılan eklenir
    body = re.sub(r'<text class="d([^"]*)"(?![^>]*\sfill=)', lambda m: f'<text class="d{m.group(1)}" fill="{t["ink"]}"', body)
    body = re.sub(r'<text class="b([^"]*)"(?![^>]*\sfill=)', lambda m: f'<text class="b{m.group(1)}" fill="{t["ink2"]}"', body)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" '
            f'aria-label="{esc(label)}"><style>{css}'
            f'.d{{font-family:D,sans-serif}}.b{{font-family:B,sans-serif}}'
            f'.n{{font-variant-numeric:tabular-nums}}'
            f'@media (prefers-reduced-motion:reduce){{*{{animation:none!important}}}}</style>{body}</svg>')


def zemin(w, h, t, uid, r=18):
    return (f'<defs><linearGradient id="z{uid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{t["bg2"]}"/>'
            f'<stop offset="1" stop-color="{t["bg"]}"/></linearGradient>'
            f'<radialGradient id="g{uid}a" cx=".12" cy=".05" r=".6"><stop offset="0" stop-color="{t["gold"]}" stop-opacity="{t["glow"]}"/>'
            f'<stop offset="1" stop-color="{t["gold"]}" stop-opacity="0"/></radialGradient>'
            f'<radialGradient id="g{uid}b" cx=".92" cy="1" r=".65"><stop offset="0" stop-color="{t["teal"]}" stop-opacity="{t["glow"]}"/>'
            f'<stop offset="1" stop-color="{t["teal"]}" stop-opacity="0"/></radialGradient></defs>'
            f'<rect width="{w}" height="{h}" rx="{r}" fill="url(#z{uid})"/>'
            f'<rect width="{w}" height="{h}" rx="{r}" fill="url(#g{uid}a)"/><rect width="{w}" height="{h}" rx="{r}" fill="url(#g{uid}b)"/>'
            f'<rect x=".5" y=".5" width="{w - 1}" height="{h - 1}" rx="{r}" fill="none" stroke="{t["line"]}"/>')


def dalga(zarf, x0, y0, w, h, renk, kalin=False):
    """Simetrik dalga zarfı: tepe değerlerinden dolu yol (dB'ye yakın sıkıştırma ile)."""
    n = len(zarf)
    ust, alt = [], []
    for i, v in enumerate(zarf):
        a = min(1.0, (max(v, 1e-4) ** 0.62) * 1.08)
        x = x0 + w * i / (n - 1)
        ust.append(f"{x:.1f},{y0 - a * h:.1f}")
        alt.append(f"{x:.1f},{y0 + a * h:.1f}")
    d = "M" + " L".join(ust) + " L" + " L".join(reversed(alt)) + "Z"
    return f'<path d="{d}" fill="{renk}"/>'


def ornekle(zarf, n):
    k = len(zarf) / n
    return [max(zarf[int(i * k):max(int(i * k) + 1, int((i + 1) * k))]) for i in range(n)]


# ---------------------------------------------------------------------------------------------------------------
def banner(t, l, demo, css, tema, dil):
    W, H = 1280, 470
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    ham = ornekle(demo["ham"]["zarf"], 360)
    isl = ornekle(st["zarf"], 360)
    x0, wx, yc, hy = 56, W - 112, 378, 50
    b = zemin(W, H, t, "b")
    b += f'<text class="b" x="64" y="84" font-size="19" font-weight="600" fill="{t["ink2"]}">{esc(l["kicker"])}</text>'
    b += f'<text class="d" x="60" y="172" font-size="98" font-weight="760" letter-spacing="-2">Sweax<tspan fill="{t["teal"]}">Next</tspan></text>'
    b += f'<text class="b" x="64" y="224" font-size="27" fill="{t["ink"]}">{esc(l["tag1"])}</text>'
    b += f'<text class="b" x="64" y="260" font-size="27" fill="{t["ink"]}">{esc(l["tag2"])}</text>'
    b += f'<text class="b n" x="64" y="306" font-size="17" fill="{t["ink3"]}">{esc(l["facts"])}</text>'
    # sağ: Temiz planında modüller (açık / gerekmedi), sırayla yanar
    sira = ["gate"] + [m for m in st["sira"] if m != "ds2"] + ["dbl", "dly", "rev", "wid", "out"]
    acik = {k: v["acik"] for k, v in st["moduller"].items()}
    ad = MODUL_AD[dil]
    cols, cw, ch, gx, gy = 4, 128, 30, 8, 8
    bx, by = W - 56 - cols * cw - (cols - 1) * gx, 52
    for i, m in enumerate(sira):
        r, c = divmod(i, cols)
        x, y = bx + c * (cw + gx), by + r * (ch + gy)
        on = acik.get(m, True)
        renk = t["teal"] if on else t["ink3"]
        b += (f'<rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="7" fill="{t["panel"]}" stroke="{renk if on else t["line"]}" stroke-opacity="{.9 if on else .7}"/>'
              f'<circle cx="{x + 14}" cy="{y + ch / 2}" r="3.6" fill="{renk}" fill-opacity="{1 if on else .5}"/>'
              f'<text class="b" x="{x + 26}" y="{y + 20}" font-size="13.5" font-weight="600" fill="{t["ink"] if on else t["ink3"]}">{esc(ad.get(m, m))}</text>')
        if on:   # sinyal zincirde ilerlerken açık modülün kenarı kısa bir an altın yanar (dalga taramasıyla aynı döngü)
            b += (f'<rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="7" fill="none" stroke="{t["gold"]}" stroke-width="1.6" '
                  f'opacity="0" style="animation:yan 9s {0.54 + i * 0.27:.2f}s infinite"/>')
    leg_y = by + ((len(sira) + cols - 1) // cols) * (ch + gy) + 14
    b += (f'<circle cx="{bx + 6}" cy="{leg_y - 4}" r="3.6" fill="{t["teal"]}"/><text class="b" x="{bx + 16}" y="{leg_y}" font-size="13">{esc(l["on"])}</text>'
          f'<circle cx="{bx + 86}" cy="{leg_y - 4}" r="3.6" fill="{t["ink3"]}" fill-opacity=".5"/><text class="b" x="{bx + 96}" y="{leg_y}" font-size="13">{esc(l["off"])}</text>')
    # alt: ham dalga (altın) → işlenmiş (turkuaz) taraması
    b += (f'<defs><clipPath id="sw"><rect x="{x0}" y="{yc - hy - 8}" width="{wx}" height="{2 * hy + 16}" '
          f'style="transform-origin:{x0}px 0;animation:tara 9s cubic-bezier(.45,0,.25,1) infinite"/></clipPath>'
          f'<linearGradient id="oy" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{t["gold"]}"/><stop offset="1" stop-color="{t["teal"]}"/></linearGradient></defs>')
    b += f'<g opacity=".95">{dalga(ham, x0, yc, wx, hy, t["gold"])}</g>'
    b += f'<g clip-path="url(#sw)"><rect x="{x0 - 4}" y="{yc - hy - 8}" width="{wx + 8}" height="{2 * hy + 16}" fill="{t["bg"]}"/>{dalga(isl, x0, yc, wx, hy, t["teal"])}</g>'
    b += f'<rect x="{x0}" y="{yc - hy - 12}" width="2.5" height="{2 * hy + 24}" rx="1.2" fill="url(#oy)" style="animation:kafa 9s cubic-bezier(.45,0,.25,1) infinite"/>'
    b += (f'<text class="b" x="{x0}" y="{yc + hy + 28}" font-size="14" fill="{t["gold"]}">{esc(l["raw"])}</text>'
          f'<text class="b" x="{x0 + wx}" y="{yc + hy + 28}" font-size="14" text-anchor="end" fill="{t["teal"]}">{esc(l["done"])}</text>')
    anim = (f"@keyframes tara{{0%,6%{{transform:scaleX(0)}}70%,90%{{transform:scaleX(1)}}100%{{transform:scaleX(1);opacity:0}}}}"
            f"@keyframes kafa{{0%,6%{{transform:translateX(0);opacity:1}}70%{{transform:translateX({wx}px);opacity:1}}"
            f"76%,100%{{transform:translateX({wx}px);opacity:0}}}}"
            f"@keyframes yan{{0%{{opacity:0}}3%{{opacity:1}}9%,100%{{opacity:0}}}}")
    return svg(W, H, b, css + anim, t, "SweaxNext — " + l["tag1"] + " " + l["tag2"])


# ---------------------------------------------------------------------------------------------------------------
KW, KH = 620, 330


def kart_baslik(t, baslik, alt):
    return (f'<text class="d" x="32" y="56" font-size="31" font-weight="760" letter-spacing="-.4">{esc(baslik)}</text>'
            f'<text class="b" x="32" y="88" font-size="16.5">{esc(alt)}</text>')


def kart_dinle(t, l, demo, css, uid):
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    g = st["giris"]
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_listen"])
    z = ornekle(demo["ham"]["zarf"], 200)
    b += (f'<defs><clipPath id="c{uid}"><rect x="32" y="120" width="556" height="120" '
          f'style="transform-origin:32px 0;animation:dl{uid} 7s linear infinite"/></clipPath></defs>')
    b += f'<g opacity=".22">{dalga(z, 32, 180, 556, 50, t["ink3"])}</g>'
    b += f'<g clip-path="url(#c{uid})">{dalga(z, 32, 180, 556, 50, t["gold"])}</g>'
    yuzde = int(round(g["guven"] * 100))
    sayilar = [(f'{demo["satirlar"][-1]["son"]:.0f} {l["sec"]}', l["heard"]),
               (str(int(g["sentences"])), l["phrases"]), (str(int(g["sibilantEvents"])), l["esses"]),
               (f'%{yuzde}' if l is L["tr"] else f'{yuzde}%', l["conf"])]
    x = 32
    for i, (v, ad) in enumerate(sayilar):
        b += (f'<g style="animation:sf{uid} 7s {0.9 + i * 0.5:.1f}s infinite both">'
              f'<text class="d n" x="{x}" y="282" font-size="26" font-weight="760" fill="{t["ink"]}">{esc(v)}</text>'
              f'<text class="b" x="{x}" y="304" font-size="14">{esc(ad)}</text></g>')
        x += 140
    anim = (f"@keyframes dl{uid}{{0%{{transform:scaleX(0)}}80%,100%{{transform:scaleX(1)}}}}"
            f"@keyframes sf{uid}{{0%{{opacity:0;transform:translateY(6px)}}8%,92%{{opacity:1;transform:none}}100%{{opacity:0}}}}")
    return svg(KW, KH, b, css + anim, t, l["c_listen"][0])


def kart_teshis(t, l, demo, css, uid):
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    d = {x["tip"]: x for x in st["girisTeshis"]}
    sat = []
    if "Plosive" in d: sat.append((l["plosive"], f'× {d["Plosive"]["adet"]}', d["Plosive"]["s"]))
    if "Sibilance" in d: sat.append((l["sib"], f'{d["Sibilance"]["hz"] / 1000:.1f} kHz × {d["Sibilance"]["adet"]}', d["Sibilance"]["s"]))
    if "NoiseFloor" in d: sat.append((l["floor"], f'{st["giris"]["noiseFloorDb"]:.1f} dB', d["NoiseFloor"]["s"]))
    if "MacroDynamics" in d: sat.append((l["lvl"], f'{d["MacroDynamics"]["deger"]:.1f} dB', d["MacroDynamics"]["s"]))
    if "Bright" in d: sat.append((l["bright"], f'{d["Bright"]["hz"] / 1000:.0f} kHz', d["Bright"]["s"]))
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_diag"])
    y = 134
    for i, (ad, deger, s) in enumerate(sat[:5]):
        bw = 210 * max(0.06, min(1.0, s))
        renk = t["red"] if s >= 0.7 else t["gold"]
        b += (f'<text class="b" x="32" y="{y + 5}" font-size="16" font-weight="600" fill="{t["ink"]}">{esc(ad)}</text>'
              f'<text class="b n" x="250" y="{y + 5}" font-size="15">{esc(deger)}</text>'
              f'<rect x="378" y="{y - 8}" width="210" height="10" rx="5" fill="{t["panel2"]}"/>'
              f'<rect x="378" y="{y - 8}" width="{bw:.1f}" height="10" rx="5" fill="{renk}" '
              f'style="transform-origin:378px 0;animation:bar{uid} 6s {i * .18:.2f}s cubic-bezier(.2,.9,.25,1.15) infinite both"/>')
        y += 38
    anim = f"@keyframes bar{uid}{{0%{{transform:scaleX(0)}}14%,90%{{transform:scaleX(1)}}100%{{transform:scaleX(0)}}}}"
    return svg(KW, KH, b, css + anim, t, l["c_diag"][0])


def kart_dogrula(t, l, demo, css, uid):
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    fb = st["feedback"]
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_verify"])
    cx, cy, r = 150, 214, 70
    b += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{t["line"]}" stroke-width="2"/>'
    for i, (ad, aci) in enumerate(((l["plan"], -90), (l["render"], 30), (l["measure"], 150))):
        a = math.radians(aci)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        b += (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="7" fill="{t["panel"]}" stroke="{t["teal"]}" stroke-width="2"/>'
              f'<text class="b" x="{x + (16 if aci == 30 else -16 if aci == 150 else 0):.1f}" y="{y + (5 if aci != -90 else -16):.1f}" '
              f'font-size="14" font-weight="600" fill="{t["ink"]}" text-anchor="{"start" if aci == 30 else "end" if aci == 150 else "middle"}">{esc(ad)}</text>')
    b += (f'<g style="transform-origin:{cx}px {cy}px;animation:don{uid} 3s linear infinite">'
          f'<circle cx="{cx}" cy="{cy - r}" r="5" fill="{t["gold"]}"/></g>')
    # turlar: gerçek skorlar (düşük = hedefe daha yakın)
    turlar = [("0", fb["skor0"])] + ([(str(fb["secilen"]), fb["skor"])] if fb["secilen"] else [])
    x = 330
    b += f'<text class="b" x="{x}" y="150" font-size="14">{esc(l["score_h"])}</text>'
    for i in range(fb["tur"]):
        sec = i == fb["secilen"]
        deger = fb["skor0"] if i == 0 else (fb["skor"] if sec else None)
        b += (f'<g style="animation:sf{uid} 6s {0.4 + i * .5:.1f}s infinite both">'
              f'<rect x="{x}" y="{166 + i * 40}" width="256" height="30" rx="8" fill="{t["panel"]}" stroke="{t["teal"] if sec else t["line"]}"/>'
              f'<text class="b n" x="{x + 14}" y="{186 + i * 40}" font-size="15" fill="{t["ink"]}">{esc(f"{i + 1}. tur" if l is L["tr"] else f"Round {i + 1}")}</text>'
              + (f'<text class="b n" x="{x + 120}" y="{186 + i * 40}" font-size="15">{deger:.3f}</text>' if deger is not None else "")
              + (f'<text class="b" x="{x + 244}" y="{186 + i * 40}" font-size="13" text-anchor="end" fill="{t["teal"]}">{esc(l["kept"])}</text>' if sec else "")
              + '</g>')
    anim = (f"@keyframes don{uid}{{to{{transform:rotate(360deg)}}}}"
            f"@keyframes sf{uid}{{0%{{opacity:0}}10%,92%{{opacity:1}}100%{{opacity:0}}}}")
    return svg(KW, KH, b, css + anim, t, l["c_verify"][0])


def kart_stiller(t, l, demo, css, uid, dil):
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_styles"])
    secim = [s for s in demo["stiller"] if s["id"] in ("temiz", "trap", "killoki", "motive")]
    ad = MODUL_AD[dil]
    n = len(secim)
    sure = 3.2 * n
    ilk = secim[0]["sira"]
    gorunen = [m for m in ilk if m not in ("ds2",)][:9]
    for k, s in enumerate(secim):
        bas, son = k / n * 100, (k + 1) / n * 100
        b += (f'<g style="animation:st{uid}{k} {sure}s infinite">'
              f'<text class="d" x="32" y="134" font-size="20" font-weight="600" fill="{t["gold"]}">{esc(s["ad"])}</text>')
        sira = [m for m in s["sira"] if m in gorunen]
        acik = {m: v["acik"] for m, v in s["moduller"].items()}
        for i, m in enumerate(sira):
            x = 32 + (i % 3) * 186
            y = 156 + (i // 3) * 46
            on = acik.get(m, True)
            b += (f'<rect x="{x}" y="{y}" width="176" height="36" rx="9" fill="{t["panel"]}" stroke="{t["teal"] if on else t["line"]}"/>'
                  f'<text class="b n" x="{x + 14}" y="{y + 23}" font-size="13" fill="{t["ink3"]}">{i + 1}</text>'
                  f'<text class="b" x="{x + 34}" y="{y + 23}" font-size="15" font-weight="600" fill="{t["ink"] if on else t["ink3"]}">{esc(ad.get(m, m))}</text>')
        b += "</g>"
    anim = ""
    for k in range(n):
        bas, son = k / n * 100, (k + 1) / n * 100
        anim += (f"@keyframes st{uid}{k}{{0%{{opacity:0}}{bas:.2f}%{{opacity:0}}{bas + 2:.2f}%{{opacity:1}}"
                 f"{son - 2:.2f}%{{opacity:1}}{son:.2f}%{{opacity:0}}100%{{opacity:0}}}}")
    return svg(KW, KH, b, css + anim, t, l["c_styles"][0])


def kart_ab(t, l, demo, css, uid):
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_ab"])
    for i, (ad, renk) in enumerate(((l["orig"], t["gold"]), (l["proc"], t["teal"]))):
        x = 32 + i * 140
        b += (f'<g style="animation:ab{uid}{i} 4s infinite"><rect x="{x}" y="130" width="130" height="38" rx="9" fill="{renk}" fill-opacity=".16" stroke="{renk}"/>'
              f'<text class="b" x="{x + 65}" y="155" font-size="14" font-weight="600" text-anchor="middle" fill="{renk}">{esc(ad)}</text></g>')
    lufs = st["giris"]["integratedLufs"] + st["abGainDb"]
    for i, renk in enumerate((t["gold"], t["teal"])):
        x = 330 + i * 70
        b += (f'<rect x="{x}" y="128" width="40" height="160" rx="6" fill="{t["panel2"]}"/>'
              f'<rect x="{x}" y="166" width="40" height="122" rx="6" fill="{renk}" fill-opacity=".85" '
              f'style="transform-origin:0 288px;animation:mt{uid} 0.9s {i * .13:.2f}s ease-in-out infinite alternate"/>')
    b += (f'<text class="d n" x="476" y="212" font-size="30" font-weight="760" fill="{t["ink"]}">{st["cikis"]["integratedLufs"]:.1f}</text>'
          f'<text class="b" x="476" y="236" font-size="14">LUFS · {esc(l["same"])}</text>'
          f'<text class="b n" x="32" y="214" font-size="15" fill="{t["ink"]}">A/B {st["abGainDb"]:+.1f} dB</text>'
          f'<text class="b" x="32" y="238" font-size="14">{esc("EŞİT SES · DELTA" if l is L["tr"] else "EQUAL LOUDNESS · DELTA")}</text>')
    anim = (f"@keyframes ab{uid}0{{0%,49%{{opacity:1}}50%,100%{{opacity:.35}}}}@keyframes ab{uid}1{{0%,49%{{opacity:.35}}50%,100%{{opacity:1}}}}"
            f"@keyframes mt{uid}{{from{{transform:scaleY(.82)}}to{{transform:scaleY(1)}}}}")
    return svg(KW, KH, b, css + anim, t, l["c_ab"][0])


def kart_gercek(t, l, demo, css, uid, olcum):
    b = zemin(KW, KH, t, uid) + kart_baslik(t, *l["c_rt"])
    gec = olcum["gecikme48"]
    cpu = ("%" + olcum["cpuYuzde"].rstrip("%")) if l is L["tr"] else olcum["cpuYuzde"]
    y = 140
    en = max(gec.values())
    for i, (k, ad) in enumerate((("canli", l["live"]), ("yuksek", l["high"]), ("ultra", l["ultra"]))):
        w = 190 * gec[k] / en
        b += (f'<text class="b" x="32" y="{y + 14}" font-size="16" font-weight="600" fill="{t["ink"]}">{esc(ad)}</text>'
              f'<rect x="120" y="{y}" width="{w:.1f}" height="18" rx="9" fill="{t["teal"] if k == "canli" else t["gold"]}" fill-opacity=".85" '
              f'style="transform-origin:120px 0;animation:gb{uid} 6s {i * .2:.1f}s cubic-bezier(.2,.9,.25,1.1) infinite both"/>'
              f'<text class="b n" x="{128 + w:.1f}" y="{y + 14}" font-size="15">{gec[k]:.0f} ms</text>')
        y += 38
    b += f'<text class="b" x="32" y="{y + 10}" font-size="13.5" fill="{t["ink3"]}">{esc(l["latency"])}</text>'
    # sağ blok: tek çekirdek yükü (zincir_rt_butcesi testi)
    b += (f'<line x1="408" y1="132" x2="408" y2="{y + 14}" stroke="{t["line"]}"/>'
          f'<text class="d n" x="430" y="182" font-size="40" font-weight="760" fill="{t["ink"]}">{cpu}</text>'
          f'<text class="b" x="430" y="208" font-size="14">{esc(l["cpu"])}</text>'
          f'<text class="b" x="430" y="228" font-size="13.5" fill="{t["ink3"]}">{esc(l["cpu2"])}</text>')
    anim = f"@keyframes gb{uid}{{0%{{transform:scaleX(0)}}14%,92%{{transform:scaleX(1)}}100%{{transform:scaleX(0)}}}}"
    return svg(KW, KH, b, css + anim, t, l["c_rt"][0])


# ---------------------------------------------------------------------------------------------------------------
def zincir(t, l, demo, css, dil):
    W, H = 1280, 360
    st = next(s for s in demo["stiller"] if s["id"] == "temiz")
    acik = {k: v["acik"] for k, v in st["moduller"].items()}
    sira = ["gate"] + st["sira"] + ["dbl", "dly", "rev", "wid", "out"]
    sabit = {"gate", "dbl", "dly", "rev", "wid", "out"}
    ad = MODUL_AD[dil]
    b = zemin(W, H, t, "z")
    b += (f'<text class="d" x="48" y="62" font-size="32" font-weight="760">{esc(l["chain_t"])}</text>'
          f'<text class="b" x="48" y="92" font-size="17">{esc(l["chain_s"])}</text>')
    per = 11
    cw, chh, gx = 98, 46, 10
    x0, ys = 48, (132, 236)
    yol = []
    for i, m in enumerate(sira):
        r, c = divmod(i, per)
        if r % 2 == 1:
            c = per - 1 - c
        x, y = x0 + c * (cw + gx), ys[r]
        yol.append((x + cw / 2, y + chh / 2))
        on = acik.get(m if m != "ds2" else "ds", True) and not (m == "ds2" and not st["moduller"].get("ds", {}).get("p", {}).get("swx.ds2.on", 0))
        renk = t["teal"] if on else t["line"]
        b += (f'<g><rect x="{x}" y="{y}" width="{cw}" height="{chh}" rx="10" fill="{t["panel"]}" '
              f'stroke="{renk}" stroke-width="{1.6 if on else 1}" {"stroke-dasharray=\"4 4\"" if m in sabit else ""}/>'
              f'<text class="b n" x="{x + 10}" y="{y + 18}" font-size="11.5" fill="{t["ink3"]}">{i + 1}</text>'
              f'<text class="b" x="{x + cw / 2}" y="{y + 32}" font-size="13.5" font-weight="600" text-anchor="middle" '
              f'fill="{t["ink"] if on else t["ink3"]}">{esc(ad.get(m, m))}</text></g>')
    d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in yol)
    # hat ve üstünde gezen sinyal kutuların ALTINDA: sinyal yalnız kutu aralarında görünür
    hat = (f'<path d="{d}" fill="none" stroke="{t["line"]}" stroke-width="2"/>'
           f'<circle r="5" fill="{t["gold"]}"><animateMotion dur="8s" repeatCount="indefinite" path="{d}"/></circle>')
    b = b.replace('<g><rect', hat + '<g><rect', 1)
    ly = 330
    b += (f'<rect x="48" y="{ly - 12}" width="22" height="14" rx="4" fill="{t["panel"]}" stroke="{t["teal"]}"/><text class="b" x="78" y="{ly}" font-size="14">{esc(l["legend_on"])}</text>'
          f'<rect x="168" y="{ly - 12}" width="22" height="14" rx="4" fill="{t["panel"]}" stroke="{t["line"]}"/><text class="b" x="198" y="{ly}" font-size="14">{esc(l["legend_off"])}</text>'
          f'<rect x="318" y="{ly - 12}" width="22" height="14" rx="4" fill="{t["panel"]}" stroke="{t["ink3"]}" stroke-dasharray="4 4"/><text class="b" x="348" y="{ly}" font-size="14">{esc(l["fixed"])}</text>')
    anim = "@keyframes zy{0%{opacity:.45}6%,90%{opacity:1}100%{opacity:.45}}"
    return svg(W, H, b, css + anim, t, l["chain_t"])


def mimari(t, l, css):
    W, H = 1280, 660
    b = zemin(W, H, t, "m")
    b += (f'<text class="d" x="48" y="62" font-size="32" font-weight="760">{esc(l["arch_t"])}</text>'
          f'<text class="b" x="48" y="92" font-size="17">{esc(l["arch_s"])}</text>')
    seritler = [(l["a_audio"], 124, t["teal"]), (l["a_worker"], 304, t["gold"]), (l["a_msg"], 500, t["ink3"])]
    for ad, y, renk in seritler:
        b += (f'<rect x="32" y="{y}" width="{W - 64}" height="{150 if y != 500 else 128}" rx="14" fill="{t["panel"]}" fill-opacity=".55" stroke="{t["line"]}"/>'
              f'<text class="b" x="52" y="{y + 28}" font-size="14" font-weight="600" fill="{renk}">{esc(ad)}</text>')

    def kutu(x, y, w, h, bas, alt=None, renk=None):
        renk = renk or t["line"]
        s = (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="{t["bg"]}" stroke="{renk}" stroke-width="1.5"/>'
             f'<text class="d" x="{x + w / 2}" y="{y + (h / 2 + 6 if not alt else h / 2 - 3)}" font-size="16" font-weight="600" text-anchor="middle">{esc(bas)}</text>')
        if alt:
            s += f'<text class="b" x="{x + w / 2}" y="{y + h / 2 + 17}" font-size="12.5" text-anchor="middle">{esc(alt)}</text>'
        return s

    def ok(d, renk, sure=2.4, uid=""):
        return (f'<path d="{d}" fill="none" stroke="{renk}" stroke-width="2" stroke-opacity=".35"/>'
                f'<path d="{d}" fill="none" stroke="{renk}" stroke-width="2.2" stroke-dasharray="6 14" '
                f'style="animation:ak {sure}s linear infinite"/>')

    # ses iş parçacığı
    ay = 164
    b += kutu(52, ay, 120, 64, l["a_host"])
    b += kutu(222, ay, 150, 64, l["a_trim"])
    b += kutu(422, ay, 360, 64, l["a_chain"], "Gate → … → Reverb → Width", t["teal"])
    b += kutu(832, ay, 150, 64, l["a_lim"])
    b += kutu(1032, ay, 196, 64, l["a_out"])
    for x1, x2 in ((172, 222), (372, 422), (782, 832), (982, 1032)):
        b += ok(f"M{x1},{ay + 32} L{x2},{ay + 32}", t["teal"], 1.4)
    b += f'<path d="M297,{ay + 64} L297,{ay + 92} L1130,{ay + 92} L1130,{ay + 64}" fill="none" stroke="{t["ink3"]}" stroke-width="1.5" stroke-dasharray="3 5"/>'
    b += f'<text class="b" x="714" y="{ay + 86}" font-size="12.5" text-anchor="middle" fill="{t["ink3"]}">{esc(l["a_dry"])}</text>'
    # analiz iş parçacığı
    wy = 344
    kutular = [(52, 150, l["a_fifo"], None), (232, 160, l["a_store"], None), (422, 200, l["a_meas"], l["a_meas2"]),
               (652, 170, l["a_det"], l["a_det2"]), (852, 180, l["a_plan"], l["a_plan2"]), (1062, 166, l["a_fb"], l["a_fb2"])]
    for x, w, bas, alt in kutular:
        b += kutu(x, wy, w, 74, bas, alt, t["gold"] if bas in (l["a_meas"], l["a_det"], l["a_plan"], l["a_fb"]) else None)
    for (x, w, *_), (x2, *_r) in zip(kutular[:-1], kutular[1:]):
        b += ok(f"M{x + w},{wy + 37} L{x2},{wy + 37}", t["gold"], 2.2)
    b += ok(f"M110,{ay + 64} L110,{wy}", t["gold"], 2.0)
    # mesaj iş parçacığı
    my = 540
    b += kutu(1012, my, 216, 64, l["a_pub"], l["a_pub2"])
    b += kutu(712, my, 220, 64, l["a_apvts"], l["a_apvts2"])
    b += kutu(452, my, 200, 64, l["a_cache"])
    b += kutu(52, my, 180, 64, l["a_ui"])
    b += ok(f"M1145,{wy + 74} L1145,{my}", t["ink2"], 2.0)
    b += ok(f"M1012,{my + 32} L932,{my + 32}", t["ink2"], 1.6)
    b += ok(f"M712,{my + 32} L652,{my + 32}", t["ink2"], 1.6)
    # parametre önbelleği → ses zinciri: analiz şeridindeki kutuların arasındaki boşluktan (x 407) yukarı
    b += ok(f"M470,{my} L470,476 L407,476 L407,290 L520,290 L520,{ay + 64}", t["teal"], 2.4)
    # arayüz ↔ parametreler: kutuların altından
    b += ok(f"M142,{my + 64} L142,{my + 78} L822,{my + 78} L822,{my + 64}", t["ink3"], 3.0).replace('stroke-opacity=".35"', 'stroke-opacity=".2"')
    anim = "@keyframes ak{to{stroke-dashoffset:-40}}"
    return svg(W, H, b, css + anim, t, l["arch_t"])


def main():
    fonts, demo_yol, cikis = sys.argv[1], sys.argv[2], sys.argv[3]
    olcum_yol = os.path.join(os.path.dirname(demo_yol), "olcum.json")
    demo = json.load(open(demo_yol, encoding="utf-8"))
    olcum = json.load(open(olcum_yol, encoding="utf-8"))
    os.makedirs(cikis, exist_ok=True)
    metin = json.dumps(L, ensure_ascii=False) + json.dumps(MODUL_AD, ensure_ascii=False) + "".join(s["ad"] for s in demo["stiller"])
    css = font_faces(fonts, metin)
    for tema, t in T.items():
        for dil, l in L.items():
            yaz = lambda ad, icerik: open(os.path.join(cikis, f"{ad}-{tema}-{dil}.svg"), "w", encoding="utf-8").write(icerik)
            yaz("banner", banner(t, l, demo, css, tema, dil))
            yaz("card-listen", kart_dinle(t, l, demo, css, "l"))
            yaz("card-diagnose", kart_teshis(t, l, demo, css, "d"))
            yaz("card-verify", kart_dogrula(t, l, demo, css, "v"))
            yaz("card-styles", kart_stiller(t, l, demo, css, "s", dil))
            yaz("card-ab", kart_ab(t, l, demo, css, "a"))
            yaz("card-realtime", kart_gercek(t, l, demo, css, "r", olcum))
            yaz("chain", zincir(t, l, demo, css, dil))
            yaz("architecture", mimari(t, l, css))
    print("tamam:", len(os.listdir(cikis)), "dosya")


if __name__ == "__main__":
    main()
