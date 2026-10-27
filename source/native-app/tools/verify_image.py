"""Verify the container, fully extract it and compare every packaged byte."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from validate_display_metadata import validate_display_metadata

p = argparse.ArgumentParser()
p.add_argument('image', type=Path)
p.add_argument('app', type=Path)
p.add_argument('mkpfs')
p.add_argument('report', type=Path)
p.add_argument('--host-test-log', type=Path, required=True)
args = p.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

check = subprocess.run([args.mkpfs, 'verify', str(args.image)], check=True,
                       capture_output=True, text=True)
assert re.search(r'Errors:\s+0', check.stdout), check.stdout
assert re.search(r'Warnings:\s+0', check.stdout), check.stdout
assert 'PFSC' in check.stdout and 'Version:' in check.stdout

with tempfile.TemporaryDirectory(prefix='nuvio-image-') as temp:
    extracted = Path(temp) / 'app'
    subprocess.run([args.mkpfs, 'unpack', '--deep', '--no-progress', str(args.image),
                    str(extracted)], check=True, capture_output=True, text=True)
    expected = {str(f.relative_to(args.app)): sha(f) for f in args.app.rglob('*') if f.is_file()}
    actual = {str(f.relative_to(extracted)): sha(f) for f in extracted.rglob('*') if f.is_file()}
    assert expected == actual, 'extracted files differ from staged app'
    assert 'nuvio-updater.elf' in actual
    helper = (extracted / 'nuvio-updater.elf').read_bytes()
    assert helper[:6] == b'\x7fELF\x02\x01'
    assert struct.unpack_from('<H', helper, 18)[0] == 62
    assert 'eboot.bin' in actual and 'sce_sys/param.json' in actual
    assert not any(Path(path).name in ('account.json', 'settings.json', 'connected-services.json',
                                      'connected-services.json.tmp') for path in actual)
    param = json.loads((extracted / 'sce_sys/param.json').read_text())
    display_profile = validate_display_metadata(param)
    assert param['titleId'] == 'PPSA99288'
    assert param['contentVersion'] == '05.000.027'
    assert param['localizedParameters']['en-US']['titleName'] == "Nuvio Max'd"
    assert param['contentId'] == 'UP9000-PPSA99288_00-NUVIOPS5NAT00001'
    assert param['applicationCategoryType'] == 0
    assert 'webAppUri' not in param
    data = (extracted / 'eboot.bin').read_bytes()
    assert struct.unpack_from('<I', data)[0] == 0x1D3D154F
    elf = data[:65536].find(b'\x7fELF')
    assert elf >= 0 and data[elf + 4:elf + 6] == b'\x02\x01'
    assert struct.unpack_from('<HH', data, elf + 16) == (0xFE10, 62)
    assert struct.unpack_from('<Q', data, elf + 24)[0] != 0
    info = json.loads((extracted / 'build-info.json').read_text())
    assert info['version'] == '0.5.27-native-experimental'
    assert info['userExperienceUpdates']['optionalDonationQr']
    assert info['userExperienceUpdates']['installSourceSelector'] and info['userExperienceUpdates']['sourceAwareUpdateReminders']
    assert info['experimentalAv']['driverOwnedHdrSwitch'] and info['experimentalAv']['pcmDefault']
    assert info['startupHotfix']['displayFailureHaltConditional'] and info['startupHotfix']['productionEntryPointCompilerGuard']
    assert info['dependencies']['opengl']['version'] == '1.0.2'
    assert info['graphicsRefresh']['streamReloadSelection'] and info['graphicsRefresh']['startupStageTiming']
    assert info['controllerLightbar']['sharedBootEnvelope'] and info['controllerLightbar']['systemResetOnClose']
    assert info['name'] == "Nuvio Max'd"
    assert info['updater']['repository'] == 'HandshakeHackin/Nuvio-maxd-releases'
    assert not info['sourceDirty']
    assert info['cloudProgressSync'] is True and info['accountProfile'] == 1
    assert info['episodePlayback']['controllerGlyphs'] is True
    assert info['connectedServices']['providers'] == ['torbox', 'premiumize']
    assert info['connectedServices']['cachedFilesOnly'] is True
    assert info['connectedServices']['backgroundLinkPreparation'] is True
    assert info['connectedServices']['preparedLinkTtlSeconds'] == 300
    assert info['connectedServices']['preparedLinkCacheCapacity'] == 100
    assert info['connectedServices']['preparationMaxConcurrent'] == 1
    assert info['connectedServices']['preparedLinksPersisted'] is False
    assert info['connectedServices']['hardwareVerified'] is False
    assert actual['sce_module/libc.prx'] == info['dependencies']['boilerplate']['runtimeSha256']
    assert info['hardwareVerified'] is False
    assert info['playbackEngines']['default'] == 'native'
    assert info['playbackEngines']['optional'] == ['EVO Player 0.13.0', 'VLC 3.0.24']
    evo = info['dependencies']['evoPlayer']
    assert evo['version'] == '0.13.0' and evo['commit'] == '2db44b5294657b12bf91666154a05f029ddb5aa2'
    for name in ('LICENSE', 'UPSTREAM.json', 'INTEGRATION.md'):
        assert 'licenses/evo-player/' + name in actual
    assert info['applicationDisplayProfile'] == display_profile
    assert info['nativeHdr']['default'] is True
    assert info['nativeHdr']['packingMatchesAcceptedFormat']
    assert info['playbackQualityUpdates']['surroundPcm'] and info['playbackQualityUpdates']['libassStyledText']
    assert info['playbackQualityUpdates']['embeddedFirstSubtitleDefaults'] and info['playbackQualityUpdates']['manualSubtitleChoiceProtected']
    for name, digest in info['dependencies']['subtitleStack']['newFonts'].items():
        assert actual['fonts/'+name] == digest
    for license in ('libass','HarfBuzz','FreeType','fontconfig','expat','libpng'):
        assert 'licenses/'+license+'.txt' in actual
    assert info['nativeHdr']['sourceMasteringMetadataForwarded'] is False
    assert info['nativeHdr']['hardwareVerified'] is False
    assert info['displayRefresh']['default'] == 60 and info['displayRefresh']['optional'] == 120
    assert info['displayRefresh']['resolutionReducedForRefresh'] is False
    assert info['hdrToSdr']['exposureGain'] == 1
    test_log = args.host_test_log.read_text()
    assert 'Controller lightbar: throttling, failure suppression, reconnect, reset and logo/reduced-motion envelopes passed' in test_log
    assert 'Production startup: actual entry point passes conditional-indentation compiler check' in test_log
    assert re.search(r'Ran 52 tests', test_log) and '\nOK\n' in test_log
    assert 'NetworkTests.test_parallel_dns_failure_does_not_walk_entire_file' in test_log
    assert re.search(r'Ran 6 tests', test_log)
    assert 'EVO integration: SDR/10-bit B-frame decode, EOF, reopen after seek, owned planes, 24/120 cadence, early/late frames and P5 rejection passed' in test_log
    assert 'Upstream refresh: Unicode wrapping, 24/60 and 24/120 frame order, bounded catch-up, holds and nonblocking logging passed' in test_log
    assert 'Release notes: first launch, update and persistent acknowledgement passed' in test_log
    assert info['userExperienceUpdates']['versionChangelog']
    assert 'User experience: preference recovery, subtitle styles, motion/contrast, custom countdown and private crash/runtime diagnostics passed' in test_log
    assert all(info['userExperienceUpdates'][key] for key in ('subtitleStylingPreview','customSeekIntervals','customEpisodeCountdown','reducedMotion','higherContrast','privateRuntimeAndCrashDiagnostics'))
    assert 'FAILED' not in test_log and 'FAIL (' not in test_log
    assert 'UI navigation: 6 tests, 0 failures' in test_log
    assert 'Stream presentation and navigation: 14 tests, 0 failures' in test_log
    assert 'Hardware decoder setup: 10 tests, 0 failures' in test_log
    assert 'Video packing and GPU presentation: 23 tests, 0 failures' in test_log
    assert 'Display refresh lifecycle: 6 tests, 0 failures' in test_log
    assert '16 Connected Services tests passed' in test_log
    assert '26 Instant playback tests passed' in test_log
    assert 'Episode skip and next playback: 35 tests, 0 failures' in test_log
    assert 'FFPFSC updater: 28 tests, 0 failures' in test_log
    assert 'Driver HDR transactions: 8 registration, busy and rollback cases passed' in test_log
    assert 'Bitstream audio: framing, capabilities, lossless fallback, pacing, seek epoch and port recovery passed' in test_log
    assert 'HDR output transitions: 9 tests, 0 failures' in test_log
    assert test_log.count('Playback quality:') == 11
    assert 'Install source: unset migration, validated local persistence and source-specific update routing passed' in test_log
    for preview in ('install-source', 'store-update', 'github-update', 'donation', 'audio-output'):
        assert preview + ': GL error 0x0' in test_log
    assert 'Playback lifecycle: open, pause, sample-trimmed forward/back seek passed' in test_log
    assert 'Playback lifecycle: bitstream pause/seek and failed-port recovery to PCM passed' in test_log
    assert 'Playback lifecycle: hardware failure reseek to software without an A/V wait cycle passed' in test_log
    assert 'PS5 display output: 27 display output selection and probe-lifecycle assertions' in test_log
    report = {
        'name': info['name'], 'version': info['version'], 'sourceCommit': info['sourceCommit'],
        'image': args.image.name, 'imageBytes': args.image.stat().st_size,
        'imageSha256': sha(args.image), 'container': 'PS5 PFS + PFSC containing exFAT',
        'containerErrors': 0, 'containerWarnings': 0,
        'extractedFileCount': len(actual), 'allExtractedFileHashesMatch': True,
        'executable': 'x86-64 ET_SCE_EXEC inside native PS5 SELF',
        'hostHttpsTestsPassed': 52, 'hostUiNavigationTestsPassed': 6,
        'hostConnectedServicesTestsPassed': 16, 'connectedServices': info['connectedServices'],
        'hostInstantPlaybackTestsPassed': 26,
        'hostDecoderSetupTestsPassed': 10,
        'hostStreamPresentationTestsPassed': 14,
        'hostVideoPackingAndGpuTestsPassed': 23, 'hostNetworkTestsPassed': 6,
        'hostHdrOutputTestsPassed': 9, 'hostEpisodePlaybackTestsPassed': 35,
        'hostDisplaySelectionAssertionsPassed': 27,
        'hostDisplayRefreshTestsPassed': 6,
        'watchProgressSync': 'Nuvio Sync profile 1; portable merged snapshots and persistent offline mutations',
        'videoPresentation': info['videoConversion'],
        'videoConversionSurface': 'RGB10_A2; HDR10 final pixels packed exactly into BGRA8 transport for the accepted RGB/BGR10A2 PQ VideoOut format',
        'userExperienceUpdates': info['userExperienceUpdates'],
        'graphicsRefresh': info['graphicsRefresh'], 'controllerLightbar': info['controllerLightbar'], 'hostControllerLightbarTestsPassed': True,
        'hostUpstreamAdaptationTestsPassed': True, 'hostExperienceTestsPassed': True,
        'playbackEngines': info['playbackEngines'],
        'evoPlayer': evo, 'hostEvoIntegrationTestsPassed': True,
        'nativeHdr': info['nativeHdr'], 'experimentalAv': info['experimentalAv'], 'startupHotfix': info['startupHotfix'], 'hostStartupCompileCheckPassed': True, 'hostDriverHdrTransactionTestsPassed': 8, 'hostBitstreamTestsPassed': True,
        'playbackQualityUpdates': info['playbackQualityUpdates'],
        'hostPlaybackQualityTestsPassed': 11, 'hostPlaybackLifecycleTestsPassed': 3,
        'applicationDisplayProfile': display_profile,
        'hdrToSdr': info['hdrToSdr'], 'displayRefresh': info['displayRefresh'],
        'updater': info['updater'], 'hostUpdaterTestsPassed': 28,
        'episodePlayback': info['episodePlayback'],
        'displaySelection': info['displaySelection'],
        'hardwareVerified': False, 'verifiedFirmwares': [],
        'filesystemPermissions': 'ExFAT mount permissions require console verification',
        'fileSha256': actual,
    }
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(f'Verified {len(actual)} files; native SELF and clean source metadata match; 229 existing host tests, 27 display assertions and EVO/upstream integration tests passed')
