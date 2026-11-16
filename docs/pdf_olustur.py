"""README.md'yi PDF rehberine çevirir: docs/HibritEnerjiAsistani_Rehber.pdf

Kullanım:  pip install reportlab   ve   python docs/pdf_olustur.py
Tek kaynak README'dir; PDF'te ayrıca elle yazılmış metin yoktur (yalnızca kapak).
Türkçe karakterler için Windows'taki Arial/Consolas yazı tipleri kullanılır;
bulunamazsa DejaVu (Linux) denenir.
"""

import os
import re
import sys
from datetime import date

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (PageBreak, Paragraph, Preformatted,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

KOK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(KOK, "README.md")
CIKTI = os.path.join(KOK, "docs", "HibritEnerjiAsistani_Rehber.pdf")

# --- Yazı tipleri ---
FONT_ADAYLARI = [
    ("C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/arialbd.ttf",
     "C:/Windows/Fonts/ariali.ttf", "C:/Windows/Fonts/consola.ttf"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
]
for normal, kalin, italik, mono in FONT_ADAYLARI:
    if all(os.path.exists(p) for p in (normal, kalin, italik, mono)):
        pdfmetrics.registerFont(TTFont("Govde", normal))
        pdfmetrics.registerFont(TTFont("Govde-Kalin", kalin))
        pdfmetrics.registerFont(TTFont("Govde-Italik", italik))
        pdfmetrics.registerFont(TTFont("Mono", mono))
        pdfmetrics.registerFontFamily("Govde", normal="Govde", bold="Govde-Kalin",
                                      italic="Govde-Italik", boldItalic="Govde-Kalin")
        break
else:
    sys.exit("Hata: Türkçe karakter destekleyen yazı tipi bulunamadı (Arial ya da DejaVu).")

# --- Renkler ve stiller ---
VURGU = colors.HexColor("#1F5F8B")
ACIK = colors.HexColor("#EAF2F8")
KOD_ZEMIN = colors.HexColor("#F4F4F4")
CIZGI = colors.HexColor("#B8C7D3")

GOVDE = ParagraphStyle("govde", fontName="Govde", fontSize=10, leading=14, spaceAfter=5)
MADDE = ParagraphStyle("madde", parent=GOVDE, leftIndent=14, bulletIndent=4, spaceAfter=3)
H1 = ParagraphStyle("h1", fontName="Govde-Kalin", fontSize=20, leading=24,
                    textColor=VURGU, spaceBefore=6, spaceAfter=10, keepWithNext=1)
H2 = ParagraphStyle("h2", fontName="Govde-Kalin", fontSize=15, leading=19,
                    textColor=VURGU, spaceBefore=14, spaceAfter=6, keepWithNext=1)
H3 = ParagraphStyle("h3", fontName="Govde-Kalin", fontSize=12, leading=15,
                    textColor=colors.HexColor("#333333"), spaceBefore=10, spaceAfter=4, keepWithNext=1)
ALINTI = ParagraphStyle("alinti", parent=GOVDE, leftIndent=10, borderPadding=6,
                        backColor=ACIK, textColor=colors.HexColor("#333333"))
HUCRE = ParagraphStyle("hucre", fontName="Govde", fontSize=8.5, leading=11)
HUCRE_BASLIK = ParagraphStyle("hucre_baslik", parent=HUCRE, fontName="Govde-Kalin",
                              textColor=colors.white)
KAPAK_BASLIK = ParagraphStyle("kapak", fontName="Govde-Kalin", fontSize=30, leading=36,
                              textColor=VURGU, alignment=TA_CENTER)
KAPAK_ALT = ParagraphStyle("kapak_alt", fontName="Govde", fontSize=13, leading=18,
                           alignment=TA_CENTER, textColor=colors.HexColor("#444444"))

SAYFA_GENISLIK = A4[0] - 36 * mm


def satir_ici(metin):
    """Markdown satır içi biçimlerini (kalın, italik, kod, bağlantı) reportlab XML'ine çevirir."""
    kodlar = []

    def kod_sakla(m):
        kodlar.append(m.group(1))
        return "\x00%d\x00" % (len(kodlar) - 1)

    metin = re.sub(r"`([^`]+)`", kod_sakla, metin)
    metin = metin.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    metin = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"<u>\1</u>", metin)
    metin = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", metin)
    metin = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", metin)

    def kod_geri(m):
        k = kodlar[int(m.group(1))]
        k = k.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        return '<font name="Mono" color="#8B1F3F">%s</font>' % k

    return re.sub(r"\x00(\d+)\x00", kod_geri, metin)


def tablo(satirlar):
    """Markdown tablo satırlarından bir reportlab Table oluşturur."""
    hucreler = [[h.strip() for h in s.strip().strip("|").split("|")] for s in satirlar]
    baslik, govde = hucreler[0], hucreler[2:]  # 2. satır ayraçtır (|---|)
    sutun = len(baslik)
    # Sütun genişliği: içerik uzunluğuna göre, aşırı uçları sınırlayarak
    uzunluk = [max(min(len(r[i]) if i < len(r) else 0, 60) for r in [baslik] + govde) + 4
               for i in range(sutun)]
    toplam = float(sum(uzunluk))
    genislik = [SAYFA_GENISLIK * u / toplam for u in uzunluk]
    # Başlık zemini koyu: koddaki renk yerine beyaz yazı
    veri = [[Paragraph(satir_ici(h).replace('color="#8B1F3F"', 'color="#FFFFFF"'), HUCRE_BASLIK)
             for h in baslik]]
    for r in govde:
        r = (r + [""] * sutun)[:sutun]
        veri.append([Paragraph(satir_ici(h), HUCRE) for h in r])
    t = Table(veri, colWidths=genislik, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), VURGU),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, ACIK]),
        ("GRID", (0, 0), (-1, -1), 0.4, CIZGI),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return t


def kod_blogu(satirlar):
    """Kod bloğu: en uzun satır sayfa genişliğine sığacak yazı boyutu seçilir."""
    enuzun = max((len(s) for s in satirlar), default=1)
    boyut = min(9.0, (SAYFA_GENISLIK - 12) / (0.6 * max(enuzun, 1)))
    stil = ParagraphStyle("kod", fontName="Mono", fontSize=boyut, leading=boyut * 1.3)
    t = Table([[Preformatted("\n".join(satirlar), stil)]], colWidths=[SAYFA_GENISLIK])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), KOD_ZEMIN),
        ("BOX", (0, 0), (-1, -1), 0.4, CIZGI),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


def markdown_akisi(metin):
    """README metnini reportlab akış öğelerine çevirir (projede kullanılan Markdown alt kümesi)."""
    akis = []
    satirlar = metin.splitlines()
    i = 0
    paragraf = []

    def paragrafi_bitir():
        if paragraf:
            akis.append(Paragraph(satir_ici(" ".join(paragraf)), GOVDE))
            paragraf.clear()

    while i < len(satirlar):
        s = satirlar[i]
        if s.startswith("```"):
            paragrafi_bitir()
            blok = []
            i += 1
            while i < len(satirlar) and not satirlar[i].startswith("```"):
                blok.append(satirlar[i])
                i += 1
            akis.append(kod_blogu(blok))
            akis.append(Spacer(1, 6))
        elif s.startswith("|"):
            paragrafi_bitir()
            blok = []
            while i < len(satirlar) and satirlar[i].startswith("|"):
                blok.append(satirlar[i])
                i += 1
            akis.append(tablo(blok))
            akis.append(Spacer(1, 8))
            continue
        elif s.startswith("#"):
            paragrafi_bitir()
            seviye = len(s) - len(s.lstrip("#"))
            stil = {1: H1, 2: H2}.get(seviye, H3)
            akis.append(Paragraph(satir_ici(s.lstrip("#").strip()), stil))
        elif s.startswith(">"):
            paragrafi_bitir()
            blok = []
            while i < len(satirlar) and satirlar[i].startswith(">"):
                blok.append(satirlar[i].lstrip(">").strip())
                i += 1
            akis.append(Paragraph(satir_ici(" ".join(blok)), ALINTI))
            akis.append(Spacer(1, 6))
            continue
        elif re.match(r"^(- |\d+\. )", s):
            paragrafi_bitir()
            m = re.match(r"^(- |(\d+)\. )(.*)", s)
            isaret = "•" if m.group(1) == "- " else m.group(2) + "."
            parca = [m.group(3)]
            i += 1
            # Girintili devam satırları aynı maddeye aittir
            while i < len(satirlar) and satirlar[i].startswith("  ") and satirlar[i].strip():
                parca.append(satirlar[i].strip())
                i += 1
            akis.append(Paragraph(satir_ici(" ".join(parca)), MADDE, bulletText=isaret))
            continue
        elif not s.strip():
            paragrafi_bitir()
        else:
            paragraf.append(s.strip())
        i += 1
    paragrafi_bitir()
    return akis


def sayfa_altligi(canvas, doc):
    canvas.saveState()
    canvas.setFont("Govde", 8)
    canvas.setFillColor(colors.HexColor("#777777"))
    canvas.drawString(18 * mm, 10 * mm, "Akıllı Hibrit Enerji Asistanı — Rehber")
    canvas.drawRightString(A4[0] - 18 * mm, 10 * mm, "Sayfa %d" % doc.page)
    canvas.restoreState()


def main():
    with open(README, encoding="utf-8") as f:
        metin = f.read()
    # Kapakta başlık zaten var; README'deki ilk H1 ve PDF'in kendisine verilen bağlantı atlanır
    metin = re.sub(r"^# .*\n", "", metin, count=1)
    metin = re.sub(r"^Bu belgenin PDF hâli:.*\n\(yeniden üretmek için:.*\n", "", metin,
                   flags=re.M)

    kapak = [
        Spacer(1, 70 * mm),
        Paragraph("Akıllı Hibrit Enerji Asistanı", KAPAK_BASLIK),
        Spacer(1, 8 * mm),
        Paragraph("Hibrit bir aracın motorunu ve bataryasını en az yakıtla yöneten "
                  "stratejileri simüle eden ve karşılaştıran yazılım", KAPAK_ALT),
        Spacer(1, 6 * mm),
        Paragraph("Ne işe yarar, ekrandaki değerler ne anlama gelir, nereden gelir?",
                  KAPAK_ALT),
        Spacer(1, 50 * mm),
        Paragraph(date.today().strftime("%d.%m.%Y"), KAPAK_ALT),
        PageBreak(),
    ]
    akis = kapak + markdown_akisi(metin)

    doc = SimpleDocTemplate(CIKTI, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                            topMargin=16 * mm, bottomMargin=18 * mm,
                            title="Akıllı Hibrit Enerji Asistanı — Rehber",
                            author="Akıllı Hibrit Enerji Asistanı")
    doc.build(akis, onFirstPage=lambda c, d: None, onLaterPages=sayfa_altligi)
    print("PDF yazıldı:", CIKTI)


if __name__ == "__main__":
    main()
