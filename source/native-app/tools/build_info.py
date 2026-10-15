"""Record the source and dependency pins used for a native image."""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
from validate_display_metadata import validate_display_metadata

root = Path(__file__).resolve().parents[1]
def git(*args):
    return subprocess.check_output(['git', '-C', str(root.parent), *args], text=True).strip()

try:
    if Path(git('rev-parse', '--show-toplevel')).resolve() != root.parent.resolve():
        raise ValueError('source archive is outside its original repository')
    source_commit = git('rev-parse', 'HEAD')
    source_dirty = bool(git('status', '--porcelain', '--untracked-files=normal'))
except (subprocess.CalledProcessError, ValueError):
    archived = json.loads((root.parent / 'source-archive.json').read_text())
    source_commit = archived['sourceCommit']
    source_dirty = any(not (root.parent / path).is_file() or
                       hashlib.sha256((root.parent / path).read_bytes()).hexdigest() != digest
                       for path, digest in archived['fileSha256'].items())
    for file in root.rglob('*'):
        relative = file.relative_to(root)
        if relative.parts[0] in ('build', 'dist', 'test-output') or '__pycache__' in relative.parts:
            continue
        if file.is_file() and str(file.relative_to(root.parent)) not in archived['fileSha256']:
            source_dirty = True

display_profile = validate_display_metadata(json.loads(
    (Path(sys.argv[1]).parent / 'sce_sys/param.json').read_text()))
info = {
    'name': "Nuvio Max'd",
    'version': '0.5.27-native-experimental',
    'titleId': sys.argv[2],
    'architecture': 'single native OpenGL app with integrated playback',
    'sourceCommit': source_commit,
    'sourceDirty': source_dirty,
    'dependencies': json.loads((root / 'dependencies.json').read_text()),
    'updater': {'repository': 'HandshakeHackin/Nuvio-maxd-releases', 'format': 'FFPFSC', 'helper': 'nuvio-updater.elf', 'shadowmountApi': '1.7 / public HTTP API v1', 'automaticInstallHardwareVerified': False},
    'userExperienceUpdates': {'versionChangelog': True, 'installSourceSelector': True, 'optionalDonationQr': True, 'sourceAwareUpdateReminders': True, 'startupUpdateCheck': True, 'subtitleStylingPreview': True, 'customSeekIntervals': True, 'customEpisodeCountdown': True, 'reducedMotion': True, 'higherContrast': True, 'privateRuntimeAndCrashDiagnostics': True, 'backgroundLogging': True, 'calmerBackgrounds': 'optional, saved locally; video quality unchanged', 'nativePresentation': 'timestamp order with bounded source-frame catch-up', 'longWordWrapping': 'UTF-8 code-point boundaries', 'updaterBackup': 'retains previous verified image'},
    'playbackQualityUpdates': {'sampleClock': True, 'seekGate': True, 'decoderFailureReseek': True, 'surroundPcm': True, 'floatingPointAudio': 'stereo fallback only', 'preferredPcm': 'S16 48 kHz', 'explicitStereoDownmix': True, 'pcmLevelDiagnostics': True, 'pcmDeviceLatencyEstimateGrains': 2, 'audioStereoFallback': True, 'embeddedFirstSubtitleDefaults': True, 'manualSubtitleChoiceProtected': True, 'externalSubtitleReopenRecovery': True, 'libassStyledText': True, 'bitmapSubtitles': True, 'bundledCjkThaiFonts': True, 'packetSlabBytes': 268435456, 'packetHardBudgetBytes': 335544320, 'pcmRingSeconds': 2, 'hardwareVerified': False},
    'experimentalAv': {'driverOwnedHdrSwitch': True, 'pcmDefault': True, 'audioModes': ['pcm','auto','bitstream'], 'bitstreamCodecs': ['AC3','EAC3','DTS core'], 'trueHdDtsHd': 'decoded PCM; S16 output preferred for compatibility', 'autoHonorsStereo': True, 'hardwareVerified': False, 'driverPatchSha256': hashlib.sha256((root/'gl/scanout_driver_extension.inc').read_bytes()).hexdigest(), 'driverObjectSha256': hashlib.sha256(Path(sys.argv[3]).read_bytes()).hexdigest()},
    'consoleLightSync': {'default': True, 'setting': 'Appearance & Lighting', 'channels': ['blue','white','amber'], 'approximateLogoColours': True, 'backgroundWorker': True, 'maxUpdatesPerSecond': 5, 'restoreOnDisableAndExit': True, 'respectSystemOwnership': True, 'hardwareVerified': False},
    'settingsCategories': True,
    'presentationTiming': {'source': 'VideoOut completed flip process timestamp', 'paths': ['GPU command', 'CPU submit'], 'minimumSamples': 3, 'maximumVideoLeadMs': 40, 'maximumSampleDelayMs': 100, 'staleAfterMs': 1000, 'physicalTvReceiverLatencyMeasured': False, 'hardwareVerified': False},
    'hardwareVerified': False,
    'controllerLightbar': {'logoPalette': 'teal/white/pink-purple', 'cycleSeconds': 18, 'sharedBootEnvelope': True, 'steadyAfterBoot': False, 'steadyWithReducedMotion': True, 'reducedMotion': True, 'maxUpdatesPerSecond': 30, 'reconnectRecovery': True, 'systemResetOnClose': True, 'hardwareVerified': False},
    'graphicsRefresh': {'opengl': '1.0.2', 'periodicDriverStatistics': 'opt-in; teardown summary retained', 'streamReloadSelection': True, 'startupStageTiming': True, 'hardwareVerified': False},
    'startupHotfix': {'displayFailureHaltConditional': True, 'productionEntryPointCompilerGuard': True, 'hardwareVerified': False},
    'firmwareTarget': '9.00 and above where required jailbreak and homebrew tools work; below 9.00 untested',
    'verifiedFirmwares': [],
    'applicationDisplayProfile': display_profile,
    'accountProfile': 1,
    'cloudProgressSync': True,
    'connectedServices': {
        'providers': ['torbox', 'premiumize'],
        'torboxAuthentication': ['device QR', 'API key', 'Nuvio Sync import'],
        'premiumizeAuthentication': ['API key', 'Nuvio Sync import'],
        'sync': 'explicit read-only credential import from Nuvio profile 1',
        'clientResolve': True,
        'cachedFilesOnly': True,
        'fileQuality': 'original provider file; no provider transcoded stream link',
        'preferredProvider': True,
        'cloudLibrary': False,
        'backgroundLinkPreparation': True,
        'preparationDefaultCandidates': 2,
        'preparationMaxCandidates': 5,
        'preparationMaxConcurrent': 1,
        'preparationStartsPerMinute': 6,
        'preparationStartsPerHour': 30,
        'preparedLinkTtlSeconds': 300,
        'preparedLinkCacheCapacity': 100,
        'preparedLinksPersisted': False,
        'hardwareVerified': False,
    },
    'episodePlayback': {'timings': 'IntroDB', 'skipIntro': True, 'skipRecap': True, 'skipCredits': True, 'controllerGlyphs': True, 'nextEpisodePrefetch': True, 'creditsAutoplayCountdownSeconds': 10, 'qualityPolicy': 'same binge group then same addon; no missing advertised resolution/format traits', 'hardwareVerified': False},
    'displaySelection': 'PS5 configured resolution by default; 1080p, 1440p and 2160p overrides',
    'displayRefresh': {'default': 60, 'optional': 120, 'restartRequired': True, 'acceptedRateReadback': True, 'resolutionReducedForRefresh': False, 'hardwareVerified': False},
    'hdrToSdr': {'toneMap': 'luminance extended Reinhard', 'outputResponse': 'BT.1886 ideal black (2.4)', 'gamut': 'BT.2020 to BT.709 with luminance-preserving neutral-axis fitting', 'sourcePeak': 'MaxCLL, mastering display, otherwise 1000 nits', 'exposureGain': 1, 'hardwareVerified': False},
    'playbackEngines': {'default': 'native', 'optional': 'EVO Player 0.12.0', 'scope': 'video decoder and presentation core', 'hardwareVerified': False},
    'videoConversion': 'Native-resolution YUV to RGB10_A2; compatible PQ/BT.2020 sources use experimental 10-bit HDR10 output',
    'nativeHdr': {
        'default': True,
        'format': 'Rgb10A2Bt2100Pq; Bgr10A2Bt2100Pq only after invalid-format rejection',
        'packingMatchesAcceptedFormat': True,
        'confirmation': 'VideoOut output status after a successful flip',
        'sdrConversion': 'Explicit HDR playback setting',
        'sourceMasteringMetadataForwarded': False,
        'dynamicMetadataSupported': False,
        'hardwareVerified': False,
    },
}
Path(sys.argv[1]).write_text(json.dumps(info, indent=2) + '\n')
