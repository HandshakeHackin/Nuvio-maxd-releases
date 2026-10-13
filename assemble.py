from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import zipfile

VERSION = '0.5.23'
SOURCE = 'b36cda66d4a98e9d04f1d74e2d4a0aca319f535b'
manifest = json.loads(Path('manifest.json').read_text())
destination = Path('download')
destination.mkdir(exist_ok=True)
for name, expected in manifest.items():
    path = Path('payload') / name
    assert path.stat().st_size == expected['bytes'], name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected['sha256'], name
    shutil.copy2(path, destination / name)
report = json.loads((destination / 'native-validation.json').read_text())
assert report['sourceCommit'] == SOURCE and report['version'] == VERSION + '-native-experimental'
assert report['extractedFileCount'] == 135 and report['allExtractedFileHashesMatch']
assert report['evoPlayer']['version'] == '0.12.0' and report['hostEvoIntegrationTestsPassed']
assert report['hostStartupCompileCheckPassed'] and report['startupHotfix']['displayFailureHaltConditional']
assert report['hostDriverHdrTransactionTestsPassed'] == 8 and report['hostBitstreamTestsPassed']
assert report['experimentalAv']['pcmDefault'] and report['experimentalAv']['driverOwnedHdrSwitch']
assert report['graphicsRefresh']['opengl'] == '1.0.2'
assert report['graphicsRefresh']['streamReloadSelection'] and report['graphicsRefresh']['startupStageTiming']
assert report['hostStreamPresentationTestsPassed'] == 14
assert report['hostExperienceTestsPassed']
assert report['hostPlaybackQualityTestsPassed'] == 11 and report['hostPlaybackLifecycleTestsPassed'] == 3
assert report['userExperienceUpdates']['installSourceSelector'] and report['userExperienceUpdates']['sourceAwareUpdateReminders']
assert report['playbackQualityUpdates']['embeddedFirstSubtitleDefaults'] and report['playbackQualityUpdates']['manualSubtitleChoiceProtected']
assert report['playbackQualityUpdates']['libassStyledText'] and report['playbackQualityUpdates']['surroundPcm']
assert all(report['userExperienceUpdates'][key] for key in ('versionChangelog','subtitleStylingPreview','customSeekIntervals','customEpisodeCountdown','reducedMotion','higherContrast','privateRuntimeAndCrashDiagnostics'))
assert report['zipFilesMatchImage'] and not report['hardwareVerified']
assert report['applicationDisplayProfile'] == {'attribute': 0x62000000, 'attribute2': 0, 'attribute3': 0x40040}
for evidence in ('hostTestEvidence', 'hostToolsEvidence'):
    assert report[evidence]['conclusion'] == 'success' and report[evidence]['headSha'] == SOURCE
image_name = f'NuvioMaxd-{VERSION}-native.ffpfsc'
with zipfile.ZipFile(destination / f'NuvioMaxd-{VERSION}-FFPFSC.zip') as archive:
    assert archive.testzip() is None
    assert set(archive.namelist()) == {image_name, 'INSTALL.txt', 'SHA256SUMS'}
    data = archive.read(image_name)
    assert len(data) == report['imageBytes']
    assert hashlib.sha256(data).hexdigest() == report['imageSha256']
    (destination / image_name).write_bytes(data)
    (destination / 'INSTALL.txt').write_bytes(archive.read('INSTALL.txt'))
subprocess.run(['python3', 'verify_payload.py', str(destination)], check=True)
for line in (destination / 'SHA256SUMS').read_text().splitlines():
    digest, name = line.split('  ', 1)
    assert hashlib.sha256((destination / name).read_bytes()).hexdigest() == digest, name
print('Verified public 0.5.23 payload; console confirmation remains pending.')
