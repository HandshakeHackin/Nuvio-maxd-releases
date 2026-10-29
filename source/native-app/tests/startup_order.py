"""Check the production module/graphics/decoder ordering without creating a second display."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'gl/main.cpp').read_text().split('int main()',1)[1]
assert main.count('HwDecoder::load_module();')==1
assert main.index('HwDecoder::load_module();')<main.index('display.open(')<main.index('renderer.init()')
assert 'sceVideodec2QueryComputeMemoryInfo' not in main
hw=(root/'src/hwdec_ps5.cpp').read_text().rsplit('void HwDecoder::load_module() {',1)[1].split('HwDecoder* HwDecoder::open',1)[0]
assert 'sceSysmoduleLoadModule' in hw and 'QueryComputeMemoryInfo' not in hw
print('Production startup order: one VideoDec2 preload before EGL/AGC, no early compute warm-up or second display')
