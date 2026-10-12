"""Prepare the OFL CJK font for both libass and the quadratic SDF UI renderer.

Input: Noto Sans CJK SC variable TrueType font at the pinned URL in the notice.
Requires fonttools==4.61.1. No font conversion is needed to build the app: the
reproducibly generated regular-weight face is committed alongside its license.
"""
from pathlib import Path
import sys
import hashlib
import json
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
pins=json.loads((Path(__file__).resolve().parents[1]/'dependencies.json').read_text())['subtitleStack']['cjkPreparation']
assert hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()==pins['sourceSha256'], 'CJK source hash differs'
font = TTFont(sys.argv[1], recalcTimestamp=False)
instantiateVariableFont(font, {'wght': 400}, inplace=True, updateFontNames=True)
font.save(Path(sys.argv[2]))
