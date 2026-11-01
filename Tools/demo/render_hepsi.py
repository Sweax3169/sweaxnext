"""Demo vokali 16 stilin hepsinden SweaxRender ile geçirir (ana vokal rolü).

Kullanım: python render_hepsi.py <SweaxRender.exe> <klasör>   (klasörde ham_vokal.wav olmalı)
Çıktı: <klasör>/stil/<ad>.wav + <ad>.json
"""
import os, subprocess, sys, time

STILLER = ["dogal", "temiz", "trap", "agresif", "killoki", "reckol", "ati242", "uzi", "motive",
           "kava", "ezhel", "ceza", "allame", "sehinsah", "sagopa", "benfero"]


def main():
    exe, kok = sys.argv[1], sys.argv[2]
    hedef = os.path.join(kok, "stil")
    os.makedirs(hedef, exist_ok=True)
    giris = os.path.join(kok, "ham_vokal.wav")
    for ad in STILLER:
        t0 = time.time()
        r = subprocess.run([exe, giris, os.path.join(hedef, ad + ".wav"), os.path.join(hedef, ad + ".json"),
                            "--rol", "ana", "--stil", ad], capture_output=True, text=True)
        print(f"{ad:10s} {time.time() - t0:5.1f} s  {r.stdout.strip().splitlines()[-1] if r.stdout else r.stderr}", flush=True)


if __name__ == "__main__":
    main()
