# -*- coding: utf-8 -*-
"""docx -> PDF (Word COM, WPS 兜底) -> PNG (pymupdf)"""
import os
import sys

DOCX = r"D:\ai data\z code\.zcode\workspace\default\AI辅助工程学习方案.docx"
OUT_DIR = r"D:\ai data\z code\.zcode\workspace\default\_build_plan"
PDF = os.path.join(OUT_DIR, "AI辅助工程学习方案.pdf")
PNG_DIR = os.path.join(OUT_DIR, "pages")


def docx_to_pdf():
    import win32com.client
    import pythoncom
    pythoncom.CoInitialize()
    word = None
    for progid in ("Word.Application", "Kwps.Application", "wps.Application"):
        try:
            word = win32com.client.DispatchEx(progid)
            print("using", progid)
            break
        except Exception as e:
            print(progid, "unavailable:", e)
    if word is None:
        raise RuntimeError("no Word/WPS COM available")
    try:
        word.Visible = False
        word.DisplayAlerts = 0
        try:
            word.Options.UpdateFieldsAtPrint = False
        except Exception:
            pass
        d = word.Documents.Open(DOCX, ReadOnly=True, AddToRecentFiles=False)
        d.ExportAsFixedFormat(OutputFileName=PDF, ExportFormat=17)
        d.Close(False)
    finally:
        word.Quit()
    print("PDF saved:", PDF, os.path.exists(PDF))


def pdf_to_png():
    import pymupdf
    os.makedirs(PNG_DIR, exist_ok=True)
    doc = pymupdf.open(PDF)
    print("pages:", len(doc))
    for i, page in enumerate(doc, start=1):
        pix = page.get_pixmap(dpi=150)
        out = os.path.join(PNG_DIR, "page-%02d.png" % i)
        pix.save(out)
        print(out)
    doc.close()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "pdf":
        pdf_to_png()
    else:
        docx_to_pdf()
