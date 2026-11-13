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
    assert 'nuvio-usb-reader.elf' in actual
    reader=(extracted/'nuvio-usb-reader.elf').read_bytes()
    assert reader[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',reader,18)[0]==62
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
    assert param['contentVersion'] == '07.000.024'
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
    assert info['version'] == '1.0.0.24-native-experimental'
    assert info['userExperienceUpdates']['optionalDonationQr']
    assert info['userExperienceUpdates']['installSourceSelector'] and info['userExperienceUpdates']['sourceAwareUpdateReminders']
    assert info['experimentalAv']['driverOwnedHdrSwitch'] and info['experimentalAv']['pcmDefault']
    assert info['startupHotfix']['displayFailureHaltConditional'] and info['startupHotfix']['productionEntryPointCompilerGuard']
    assert info['dependencies']['opengl']['version'] == '1.0.2'
    assert info['graphicsRefresh']['streamReloadSelection'] and info['graphicsRefresh']['startupStageTiming']
    assert info['controllerLightbar']['sharedBootEnvelope'] and info['controllerLightbar']['systemResetOnClose']
    assert info['name'] == "Nuvio Max'd"
    assert info['releaseChannel']=='experimental'
    dependencies = info['dependencies']
    assert dependencies['sdk']['pacbrewVersion'] == '0.41.1'
    assert dependencies['hostToolchain']['version'] == '23.1.5'
    assert dependencies['maintenanceLibraries']['sources']['OpenSSL']['version'] == '4.1.0'
    assert dependencies['maintenanceLibraries']['sources']['ffmpeg']['version'] == '9.0.2'
    for path, digest in info['dependencyRefresh20261113']['assetSha256'].items():
        if path.startswith('app/'):
            assert actual[path[4:]] == digest, path
    assert info['interfaceLanguages']['available']==['en','de','es','fr','ar','zh-Hans','ja','ko'] and info['interfaceLanguages']['englishFallback']
    assert info['detailActions']['libraryMembership'] and info['detailActions']['controllerGlyphHints']
    assert info['mdblistMembershipFix']['rawRemoteReconciliation'] and info['mdblistMembershipFix']['boundedOffsetPagination']
    assert info['detailActions']['trackerLibraryWrites']==['trakt','mdblist'] and info['detailActions']['bulkEpisodeLimit']==5000
    assert info['peopleBrowsing']['castAndCrewPhotos'] and info['peopleBrowsing']['personBiography'] and info['peopleBrowsing']['movieAndTvCredits']
    assert info['updater']['repository'] == 'HandshakeHackin/Nuvio-maxd-releases'
    assert not info['sourceDirty']
    assert info['cloudProgressSync'] is True
    assert info['accountProfiles']['maximum'] == 6
    assert info['accountProfiles']['selection'] and info['accountProfiles']['create']
    assert info['accountProfiles']['isolatedLocalData']
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
    assert info['playbackEngines']['optional'] == ['EVO Player 0.13.7', 'VLC 3.0.24']
    evo = info['dependencies']['evoPlayer']
    assert evo['version'] == '0.13.7' and evo['commit'] == '08bcab5de76cb7507ecc0bc3e1d4bca9bea13788'
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
    assert info['torrentHub']['optIn'] and info['torrentHub']['consoleConfirmation']
    assert info['torrentHub']['trackerWorkers'] == 2
    assert 'licenses/torrent-edition/UPSTREAM.json' in actual
    assert info['displayRefresh']['default'] == 60 and info['displayRefresh']['optional'] == 120
    assert info['displayRefresh']['resolutionReducedForRefresh'] is False
    assert info['hdrToSdr']['exposureGain'] == 1
    test_log = args.host_test_log.read_text()
    assert 'MDBList membership: 43 regression cases passed' in test_log
    assert 'MDBList watched history: 33 regression cases passed' in test_log
    assert 'Watched Session: production snapshot/revisit' in test_log
    assert 'legacy cache retirement, unchanged focus hints' in test_log
    assert 'watched_refresh_keeps_carousel_position: PASS' in test_log
    assert all(mode+': GL error 0x0' in test_log for mode in ['watched-movie','watched-movie-unmarked','watched-home','watched-person','de-watched-movie','ar-watched-movie','ja-watched-movie','ko-watched-movie'])
    assert info['watchedStateFix']['explicitEpisodesOnly'] and not info['watchedStateFix']['activitySummariesExpanded'] and info['watchedStateFix']['legacyScopeCacheRetired'] and info['watchedStateFix']['watchedRefreshPreservesFocus'] and info['watchedStateFix']['historyBasedMovieIndicators']
    assert 'Remote remapping: learning, consumed capture' in test_log
    assert 'Remote Session: action selection, assign/edit/remove/reset' in test_log
    assert info['mediaRemote']['buttonRemapping'] and info['mediaRemote']['appLocalOnly']
    assert 'USB diagnostics: missing/access/empty mounts, metadata fallback, unknown types, enumeration errors, privacy, cancellation and global count/depth limits passed' in test_log
    assert 'USB reader: bounded protocol, loopback loader, fragmented/buffered transfers, invalid replies, cancellation, validation, private atomic copies and profile isolation passed' in test_log
    assert 'Localization: eight saved languages' in test_log
    assert 'Localization dialogs: eight native language names' in test_log
    assert 'People metadata: movie/TV credits' in test_log
    assert 'People UI: cast/person focus' in test_log
    assert re.search(r'Feature cores: (\d+) checks passed',test_log) and int(re.search(r'Feature cores: (\d+) checks passed',test_log)[1])>=99
    assert info['configurationBackup']['authenticatedEncryption'] and info['stalkerPortal']['liveTv'] and info['tvdbOrdering']['canonicalHistoryIdentity']
    assert 'Title actions: production menus, library toggle' in test_log
    assert all(mode+': GL error 0x0' in test_log for mode in ['title-actions','title-actions-movie','watched-confirm','de-title-actions','ar-title-actions','zh-Hans-title-actions','ja-title-actions','ko-title-actions'])
    assert 'People Session: cancelled/stale/profile completions' in test_log
    assert all(mode+': GL error 0x0' in test_log for mode in ['de-person','de-cast','de-language','de-login','de-profile-pin','de-usb-language','de-person-last','de-cast-last'])
    assert 'person: GL error 0x0' in test_log and 'cast: GL error 0x0' in test_log and 'language: GL error 0x0' in test_log
    assert info['interfaceLanguages']['coreMenuEntries']>=425
    assert info['interfaceLanguages']['arabicTextShaping'] and info['interfaceLanguages']['advancedHelpMayUseEnglish']
    assert all(language+'-'+scene+': GL error 0x0' in test_log for language in ['es','fr','ar','zh-Hans','ja','ko'] for scene in ['language-picker','settings-categories','login','profile-pin','person','home','player','usb-language','release-notes'])
    assert 'ALL TORRENT STREAMING HOST TESTS PASSED!' in test_log
    assert 'HDR fallback: strict/SDR modes, failed restoration, retained picture, session reset and unchanged preferences passed' in test_log
    assert 'Controller lightbar: throttling, failure suppression, reconnect, reset and logo/reduced-motion envelopes passed' in test_log
    assert 'Production startup: actual entry point passes conditional-indentation compiler check' in test_log
    assert re.search(r'Ran 88 tests', test_log) and '\nOK\n' in test_log
    assert 'NetworkTests.test_parallel_dns_failure_does_not_walk_entire_file' in test_log
    assert re.search(r'Ran 8 tests', test_log)
    assert 'EVO integration: SDR/10-bit B-frame decode, EOF, reopen after seek, owned planes, 24/120 cadence, early/late frames and P5 rejection passed' in test_log
    assert 'Upstream refresh: Unicode wrapping, 24/60 and 24/120 frame order, bounded catch-up, holds and nonblocking logging passed' in test_log
    assert 'Release notes: first launch, update and persistent acknowledgement passed' in test_log
    assert info['userExperienceUpdates']['versionChangelog']
    assert 'User experience: preference recovery, subtitle styles, motion/contrast, custom countdown and private crash/runtime diagnostics passed' in test_log
    assert all(info['userExperienceUpdates'][key] for key in ('subtitleStylingPreview','customSeekIntervals','customEpisodeCountdown','reducedMotion','higherContrast','privateRuntimeAndCrashDiagnostics'))
    assert 'FAILED' not in test_log and 'FAIL (' not in test_log
    assert 'Traceback (most recent call last)' not in test_log
    assert 'AssertionError' not in test_log
    assert 'UI navigation: 12 tests, 0 failures' in test_log
    assert 'Stream presentation and navigation: 16 tests, 0 failures' in test_log
    assert 'Hardware decoder setup and HEVC base-layer filtering: 11 tests, 0 failures' in test_log
    assert 'Video packing and GPU presentation: 24 tests, 0 failures' in test_log
    assert 'Display refresh lifecycle: 6 tests, 0 failures' in test_log
    assert '19 Connected Services tests passed' in test_log
    assert '26 Instant playback tests passed' in test_log
    assert 'Episode skip and next playback: 61 tests, 0 failures' in test_log
    assert 'FFPFSC updater: 46 tests, 0 failures' in test_log
    assert 'Scanout driver: in-place HDR/SDR, refusal preserves registration, busy queue passed' in test_log
    assert 'Bitstream audio: framing, capabilities, lossless fallback, pacing, seek epoch and port recovery passed' in test_log
    assert 'HDR output transitions: 10 tests, 0 failures' in test_log
    assert test_log.count('Playback quality:') == 13
    assert 'Install source: unset migration, validated local persistence and source-specific update routing passed' in test_log
    for preview in ('install-source', 'store-update', 'github-update', 'donation', 'audio-output'):
        assert preview + ': GL error 0x0' in test_log
    assert 'Playback lifecycle: open, pause, sample-trimmed forward/back seek passed' in test_log
    assert 'Playback lifecycle: bitstream pause/seek and failed-port recovery to PCM passed' in test_log
    assert 'Playback lifecycle: hardware failure reseek to software without an A/V wait cycle passed' in test_log
    assert 'PS5 display output: 27 display output selection and probe-lifecycle assertions' in test_log
    report = {
        'name': info['name'], 'version': info['version'], 'sourceCommit': info['sourceCommit'],
        'contentVersion': param['contentVersion'],
        'image': args.image.name, 'imageBytes': args.image.stat().st_size,
        'imageSha256': sha(args.image), 'container': 'PS5 PFS + PFSC containing exFAT',
        'containerErrors': 0, 'containerWarnings': 0,
        'extractedFileCount': len(actual), 'allExtractedFileHashesMatch': True,
        'executable': 'x86-64 ET_SCE_EXEC inside native PS5 SELF',
        'hostHttpsTestsPassed': 88, 'hostUiNavigationTestsPassed': 12,
        'hostUsbReaderTestsPassed': True, 'hostUsbMountDiagnosticsTestsPassed': True, 'usbPlaylistHardwareVerified': False,
        'hostTorrentIntegrationTestsPassed': True, 'hostHdrFallbackTestsPassed': True,
        'hostRemoteRemappingTestsPassed': True, 'hostRemoteSettingsTestsPassed': True,
        'mediaRemote': info['mediaRemote'],
        'detailActions': info['detailActions'],
        'hostTitleActionsTestsPassed': True,
        'torrentHub': info['torrentHub'],
        'hostConnectedServicesTestsPassed': 19, 'connectedServices': info['connectedServices'],
        'hostInstantPlaybackTestsPassed': 26,
        'hostDecoderSetupTestsPassed': 11,
        'hostStreamPresentationTestsPassed': 15,
        'hostVideoPackingAndGpuTestsPassed': 24, 'hostNetworkTestsPassed': 8,
        'hostHdrOutputTestsPassed': 10, 'hostEpisodePlaybackTestsPassed': 61,
        'hostDisplaySelectionAssertionsPassed': 27,
        'hostDisplayRefreshTestsPassed': 6,
        'watchProgressSync': 'Profile-isolated Nuvio Sync; portable merged snapshots and persistent offline mutations',
        'videoPresentation': info['videoConversion'],
        'videoConversionSurface': 'RGB10_A2; HDR10 final pixels packed exactly into BGRA8 transport for the accepted RGB/BGR10A2 PQ VideoOut format',
        'userExperienceUpdates': info['userExperienceUpdates'],
        'graphicsRefresh': info['graphicsRefresh'], 'controllerLightbar': info['controllerLightbar'], 'hostControllerLightbarTestsPassed': True,
        'hostUpstreamAdaptationTestsPassed': True, 'hostExperienceTestsPassed': True,
        'playbackEngines': info['playbackEngines'],
        'evoPlayer': evo, 'hostEvoIntegrationTestsPassed': True,
        'nativeHdr': info['nativeHdr'], 'experimentalAv': info['experimentalAv'], 'startupHotfix': info['startupHotfix'], 'hostStartupCompileCheckPassed': True, 'hostDriverHdrTransactionsPassed': True, 'hostBitstreamTestsPassed': True,
        'playbackQualityUpdates': info['playbackQualityUpdates'],
        'hostPlaybackQualityTestsPassed': 13, 'hostPlaybackLifecycleTestsPassed': 3,
        'applicationDisplayProfile': display_profile,
        'hdrToSdr': info['hdrToSdr'], 'displayRefresh': info['displayRefresh'],
        'updater': info['updater'], 'hostUpdaterTestsPassed': 46,
        'episodePlayback': info['episodePlayback'],
        'displaySelection': info['displaySelection'],
        'interfaceLanguages': info['interfaceLanguages'], 'peopleBrowsing': info['peopleBrowsing'],
        'hostLocalizationTestsPassed': True, 'hostPeopleMetadataUiSessionTestsPassed': True,
        'hardwareVerified': False, 'verifiedFirmwares': [],
        'filesystemPermissions': 'ExFAT mount permissions require console verification',
        'fileSha256': actual,
    }
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(f'Verified {len(actual)} files; native SELF and clean source metadata match; current host suites, 27 display assertions and EVO/upstream integration tests passed')
