#!/usr/bin/env python3
"""Reproduce the renamed OFL subtitle fallback; native builds use the pinned asset.

Requires fontTools 4.68.0. Inputs are the three unchanged bundled fonts. No
network or console fonts are used. Normalize outlines to 1000 UPEM, remove
resolution-specific hinting, merge layout tables, retain the complete input
Unicode coverage, and give the modified font its own family and PostScript name.
"""
from pathlib import Path
import argparse, hashlib, tempfile
import fontTools
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.scaleUpem import scale_upem
from fontTools.merge import Merger

ROOT=Path(__file__).resolve().parents[1]
INPUTS={
 'NotoSans-Regular.ttf':'478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823',
 'NotoSansThai-Regular.ttf':'404ddfb5ed0aaa6b6ec8a85700d682978992062d67da93903967b56cbd9a4acc',
 'Inter-Regular.ttf':'40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82',
}
FAMILY='Nuvio Subtitle Fallback'
EPOCH=2082844800  # OpenType seconds since 1904: fixed 1970-01-01.

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args()
 if fontTools.__version__!='4.68.0':raise SystemExit('Requires fontTools 4.68.0')
 expected=set();copyrights=[]
 with tempfile.TemporaryDirectory(prefix='nuvio-subtitle-font-') as temp:
  paths=[]
  for name,digest in INPUTS.items():
   path=ROOT/'app/fonts'/name
   if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise SystemExit('Input font pin differs: '+name)
   font=TTFont(path,recalcTimestamp=False)
   expected.update(font.getBestCmap())
   copyright=font['name'].getDebugName(0)
   if copyright and copyright not in copyrights:copyrights.append(copyright)
   for tag in ['fpgm','prep','cvt ','hdmx','VDMX','LTSH','DSIG']:
    if tag in font:del font[tag]
   for glyph in font['glyf'].glyphs.values():glyph.removeHinting()
   scale_upem(font,1000)
   font['head'].created=font['head'].modified=EPOCH
   target=Path(temp)/name;font.save(target);paths.append(str(target))
  font=Merger().merge(paths);font.recalcTimestamp=False
  font['head'].created=font['head'].modified=EPOCH
  names=newTable('name');names.names=[]
  records={0:'\n'.join(copyrights),1:FAMILY,2:'Regular',3:'NuvioSubtitleFallback-1.000',4:FAMILY+' Regular',
   5:'Version 1.000',6:'NuvioSubtitleFallback-Regular',13:'Licensed under the SIL Open Font License, Version 1.1. See the bundled NuvioSubtitleFallback-NOTICE.txt and source font licenses.',
   14:'https://openfontlicense.org/',16:FAMILY,17:'Regular'}
  for key,value in records.items():names.setName(value,key,3,1,0x409)
  font['name']=names
  cmap=font.getBestCmap()
  if not expected<=cmap.keys():raise SystemExit('Merged font lost input Unicode coverage')
  if any(cmap.get(c) in (None,'.notdef') for c in map(ord,'ภาษาไทย ทดสอบ น้ำ')):raise SystemExit('Thai glyph coverage missing')
  args.output.parent.mkdir(parents=True,exist_ok=True);font.save(args.output)
  check=TTFont(args.output)
  if not expected<=check.getBestCmap().keys():raise SystemExit('Serialized font lost coverage')
  print(f'{len(expected)} input code points retained; {args.output.stat().st_size} bytes; SHA-256 '+hashlib.sha256(args.output.read_bytes()).hexdigest())
if __name__=='__main__':main()
