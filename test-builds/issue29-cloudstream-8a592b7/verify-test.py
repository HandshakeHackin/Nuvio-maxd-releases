"""Verify issues28/29 native app, cache payload, ZIP/image and exact matching source."""
from pathlib import Path,PurePosixPath
import hashlib,json,os,re,stat,struct,subprocess,tempfile,zipfile
repo=Path('/workspace/nuvio-cloudstream')
root=repo/'source/native-app'
evidence=Path('/workspace/nuvio-cloudstream-evidence')
output=Path('/workspace/nuvio-cloudstream-output')
app=output/'PPSA99288'
delivery=Path('/workspace/nuvio-cloudstream-delivery')
expected_commit='8a592b79d8261431e8da06f6d5d148a8d7429364'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip()==expected_commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=repo,text=True)
def sha(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
staged={p.relative_to(app).as_posix():sha(p) for p in app.rglob('*') if p.is_file()}
package=output/'PPSA99288.zip'
with zipfile.ZipFile(package) as archive:
 entries=archive.infolist();assert len({e.filename for e in entries})==len(entries)
 assert archive.testzip() is None
 for entry in entries:
  parts=PurePosixPath(entry.filename).parts
  assert parts and parts[0]=='PPSA99288' and '..' not in parts
  assert not entry.filename.startswith('/') and '\\' not in entry.filename and not entry.flag_bits&1
  assert not stat.S_ISLNK(entry.external_attr>>16)
  assert (entry.external_attr>>16)&0o777==0o777
 files={e.filename.removeprefix('PPSA99288/'):hashlib.sha256(archive.read(e)).hexdigest() for e in entries if not e.is_dir()}
 assert files==staged,'ZIP differs from freshly built staged app'
 required={'eboot.bin','tracker-applications.json','build-info.json','sce_sys/param.json','CREDITS.txt','LICENSE','THIRD_PARTY.md','release-manifest.json'}
 assert required<=set(files)
 assert not set(files)&{'tracking.json','account.json','settings.json','connected-services.json','profile-settings.json'}
 native=archive.read('PPSA99288/eboot.bin')
 assert len(native)>1<<20 and struct.unpack_from('<I',native)[0]==0x1d3d154f
 elf=native[:65536].find(b'\x7fELF');assert elf>=0 and native[elf+4:elf+6]==b'\x02\x01'
 assert struct.unpack_from('<HH',native,elf+16)==(0xfe10,62)
 assert struct.unpack_from('<Q',native,elf+24)[0]!=0
 markers={text:text.encode() in native for text in ['/oauth2/device','/oauth2/token','simkl_at_','media:read media:write']}
 assert all(markers.values()),markers
 applications=json.loads(archive.read('PPSA99288/tracker-applications.json'))
 source_applications=json.loads((root/'app/tracker-applications.json').read_text())
 assert applications==source_applications
 assert set(applications['simkl'])=={'client_id'} and re.fullmatch('[a-f0-9]{64}',applications['simkl']['client_id'])
 assert all(set(record)=={'client_id'} for record in applications.values())
 info=json.loads(archive.read('PPSA99288/build-info.json'))
 assert info['sourceCommit']==expected_commit and info['sourceDirty'] is False
 assert info['dependencies']['hostToolchain']['version']=='23.1.5'
 assert not info['buildIntegrity']['signedRelease'] and 'release-manifest.sig' not in files
 param=json.loads(archive.read('PPSA99288/sce_sys/param.json'))
 assert param['titleId']=='PPSA99288' and param['localizedParameters']['en-US']['titleName']=="Nuvio Max'd"
 assert param==json.loads((root/'app/sce_sys/param.json').read_text())
 manifest=json.loads(archive.read('PPSA99288/release-manifest.json'))
 assert manifest['sourceCommit']==expected_commit and manifest['version']==info['version']
 records=manifest['files'];names=[r['path'] for r in records]
 assert names==sorted(set(names)) and set(names)==set(files)-{'release-manifest.json'}
 for record in records:
  data=archive.read('PPSA99288/'+record['path'])
  assert len(data)==record['size'] and hashlib.sha256(data).hexdigest()==record['sha256']
image=output/'PPSA99288.ffpfsc';mkpfs='/workspace/nuvio-deps24/host-venv/bin/mkpfs'
checked=subprocess.check_output([mkpfs,'verify',str(image)],text=True)
assert re.search(r'Errors:\s+0',checked) and re.search(r'Warnings:\s+0',checked)
with tempfile.TemporaryDirectory(prefix='free-tv-image-') as temp:
 target=Path(temp)/'app'
 subprocess.run([mkpfs,'unpack','--deep','--no-progress',str(image),str(target)],check=True,capture_output=True)
 unpacked={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()}
 assert unpacked==files,'Native image and ZIP differ'
import tarfile,shutil
assert info['version']=='1.0.1.3-native-experimental' and param['contentVersion']=='07.001.003'
unified=info['unifiedTvGuide']
assert unified['allSourcesView'] and unified['sourceIdentitiesPreserved'] and unified['perSourceGuideAndClock']
assert unified['maxSources']==17 and unified['maxChannels']==100000 and unified['maxRetainedChannelBytes']==32<<20
assert unified['loadTimeoutSeconds']==120 and unified['sourceLoadTimeoutSeconds']==30
assert unified['guideConcurrency']==2 and unified['guideCacheChannels']==256 and unified['retainedXmlDocumentsBytes']==32<<20
assert b'builtin-all-tv' in native and b'Unified TV guide' in native
notice=re.search(r'disclaimer="([^"]+)";', (root/'src/free_tv.h').read_text()).group(1)
for locale in ['en','de','es','fr','ar','zh-Hans','ja','ko']:
 text=notice if locale=='en' else json.loads((root/f'ui/locales/{locale}.json').read_text())['strings'][notice]
 assert text.encode() in native,locale
free=info['freeLiveTv']
assert free['defaultEnabled'] is False and free['profileLocal'] and free['disclaimerRequired'] and free['disclaimerVersion']==1
assert free['subscriptionRequired'] is False and free['credentialsUploaded'] is False
assert set(free['disclaimerLanguages'])=={'en','de','es','fr','ar','zh-Hans','ja','ko'} and len(free['sources'])==7
assert free['maxCatalogueBytes']==8<<20 and free['maxCountryBytes']==256<<10 and free['maxCountryChannels']==10000 and free['maxChannels']==100000
assert free['multipleCountries'] and free['maxSelectedCountries']==256 and free['maxCombinedCatalogueBytes']==32<<20 and free['loadTimeoutMs']==90000
assert free['perChannelGuide'] and free['maxGuideBytes']==1<<20 and free['guideConcurrency']==2
assert 'licenses/free-live-tv/NOTICE.txt' in files
assert b'Please do not report dead channels or buffering' in native and b'nuvio-livetv/data/' in native
assert (app/'licenses/free-live-tv/NOTICE.txt').read_bytes()==(root/'app/licenses/free-live-tv/NOTICE.txt').read_bytes()
symbols=subprocess.check_output(['/workspace/nuvio-deps24/llvm23/bin/llvm-nm','--demangle','/workspace/nuvio-cloudstream-build/app/pie.elf'],text=True)
assert re.search(r' [tT] sx::Session::free_tv_toggle\(',symbols)
assert re.search(r' [tT] sx::Session::free_tv_country_choices\(',symbols)
assert b'free_tv_countries' in native and b'Free TV countries' in native
assert re.search(r' [tT] free_tv::fetch\(',symbols)
assert re.search(r' [tT] free_tv::guide\(',symbols)
assert re.search(r' [tT] unified_tv::fetch\(',symbols)
assert re.search(r' [tT] sx::Session::live_guide_unified_update\(',symbols)
assert re.search(r' [tT] sx::Session::live_unified_open\(',symbols)
thai=info['thaiSubtitleFix']
pins=json.loads((root/'dependencies.json').read_text())['subtitleStack']['fallbackPreparation']
assert thai['issue']==27 and thai['untaggedAndMislabeledUtf8'] and thai['genericAssFontFallback'] and thai['nativeAndEvo'] and thai['vlcBundledFonts']
assert thai['legacyDecodingRequiresThaiHint'] and thai['utf8AndBomPrecedence'] and not thai['hardwareVerified']
assert files[pins['output'].removeprefix('app/')]==thai['fallbackFontSha256']==pins['sha256']
assert files[pins['notice'].removeprefix('app/')]==pins['noticeSha256']
for name,digest in pins['inputs'].items():assert files['fonts/'+name]==digest
assert b'NuvioSubtitleFallback-Regular.ttf' in native and b'Nuvio Subtitle Fallback' in native
assert b':ssa-fontsdir=/app0/fonts' in native

search=info['searchNavigation']
assert search['issue']==19 and search['configuredCatalogSuggestions'] and search['optionalTopNavigation']
assert search['dedicatedSearchTab'] and search['allEnabledSectionsReachable'] and search['profileScopedPreferences']
assert search['maximumSuggestionCatalogs']==8 and search['maximumSuggestionRequests']==2 and search['maximumSuggestions']==6
assert search['suggestionResponseLimit']==524288 and search['suggestionRequestTimeoutMs']==4000 and search['suggestionDeadlineMs']==12000
assert search['privateEntryUnchanged'] and search['playbackShortcutsPreserved'] and not search['hardwareVerified']
assert search['chineseAndKanjiConversion']=='PS5 system IME' and search['hangulComposition']
assert set(search['keyboardLanguages'])=={'en','de','es','fr','he','ar','ru','th','ja','ko','zh-Hans'}
for name in ['sx::Session::ask_search(', 'sx::Session::search_keyboard_input(', 'sx::Session::search_suggestions_tick(', 'sx::Shell::route_top_navigation(', 'sx::draw_search_keyboard(']:
 assert name in symbols,name
for locale in ['en','de','es','fr','ar','zh-Hans','ja','ko']:
 for key in ['Top navigation','Title suggestions','Type at least two characters']:
  text=key if locale=='en' else json.loads((root/f'ui/locales/{locale}.json').read_text())['strings'][key]
  assert text.encode() in native,(locale,key)
inputs=json.loads((evidence/'validated-build-inputs.json').read_text())
assert inputs['exactOutputPins']==738 and len(inputs['unusedTargetUtilityExceptions'])==3
source_archive=delivery/'NuvioMaxd-1.0.1.3-source.tar.gz'
source_paths=['source/README.md','source/native-app','source/ps5/account-config.json','source/ps5/GITHUB.md','source/.github/workflows/native-checks.yml']
tree=subprocess.check_output(['git','ls-tree','-rz',expected_commit,'--',*source_paths],cwd=repo).split(b'\0')
expected={}
for row in tree:
 if not row:continue
 meta,name=row.split(b'\t',1);mode,kind,oid=meta.split();assert kind==b'blob';expected[name.decode().removeprefix('source/')]=oid.decode()
actual={};prefix='NuvioMaxd-1.0.1.3-source/'
with tarfile.open(source_archive,'r:gz') as archive:
 for member in archive:
  if member.isdir():continue
  assert member.isfile() and member.name.startswith(prefix) and '..' not in PurePosixPath(member.name).parts
  name=member.name.removeprefix(prefix);assert name not in actual
  actual[name]=archive.extractfile(member).read()
meta=json.loads(actual.pop('source-archive.json'))
assert meta['sourceCommit']==expected_commit and meta['version']=='1.0.1.3'
assert {name:hashlib.sha256(data).hexdigest() for name,data in actual.items()}==meta['fileSha256']
git_files={name:data for name,data in actual.items() if not name.startswith('vendor-sources/')}
assert set(git_files)==set(expected)
for name,data in git_files.items():
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==expected[name],name
provenance=json.loads((root/'third_party/vlc/UPSTREAM.json').read_text())
for pin in [provenance]+[provenance[k] for k in ['libdvbpsi','libebml','libmatroska','utf8cpp'] if k in provenance]:
 name='vendor-sources/'+pin['sourceUrl'].rsplit('/',1)[1]
 assert hashlib.sha256(actual[name]).hexdigest()==pin['sourceSha256']
refresh=info['channelListRefresh']
assert refresh['intervalHours']==[0,1,3,6,12,24] and refresh['defaultHours']==6
for flag in ['profileLocal','freeAndPersonalSources','unifiedView','startsAfterFirstLoad','whileAppOpenOnly','playbackAndMenusDeferred','automaticDownloadCancellation','failedOrPartialRefreshKeepsLastListAndGuide','manualRefreshWithOff','encryptedBackup']:
 assert refresh[flag],flag
assert refresh['retryBackoffSeconds']==[300,600,1200,2400,3600] and not refresh['hardwareVerified']
for name in ['sx::Session::live_refresh_tick(', 'sx::Session::live_refresh_menu(', 'sx::Session::live_refresh_complete(']:assert name in symbols,name
for locale in ['en','de','es','fr','ar','zh-Hans','ja','ko']:
 for key in ['Channel list auto-refresh','Every 6 hours','Every 24 hours']:
  text=key if locale=='en' else json.loads((root/f'ui/locales/{locale}.json').read_text())['strings'][key]
  assert text.encode() in native,(locale,key)
# Untagged test: the packaged app's metadata is checked against its exact source above.
# The stable catalog's public metadata is not a test delivery input.
followup=info['issue25Followup']
for key in ['denseAssActiveCuesPreserved','expiredEventsOnlyPruned','externalTimelineRetainedForSeeking','embeddedRetentionClockIndependentOfExternalSync','sharedDnsAddressesOnly','freshDnsOnSubtitleRetry','streamWorkerAllocationRecovery','existingReaderFallback']:
 assert followup[key],key
assert followup['maxEmbeddedEventsPerTrack']==10000 and followup['dnsTtlSeconds']==60
assert not followup['evoCrashRootCauseConfirmed'] and not followup['hardwareVerified']
assert 'ass_prune_events' in symbols
assert 'http_share_playback_dns' in symbols
assert followup['freshDnsOnStreamRetry'] and followup['accountAndAddonDnsUnchanged']
assert b'parallel workers unavailable; retaining one connection' in native
assert b'Could not start stream reader thread' in native
usb=info['usbTorrentCache']
assert usb['issue']==28 and usb['optIn'] and usb['defaultLocation']=='internal'
assert usb['usbMounts']==8 and usb['selectedMountOnly'] and usb['cacheGiB']==[1,2,4,6]
assert usb['cachedReaderStopAndCancelGuard']
assert usb['maxBytes']==6<<30 and usb['chunkBytes']==256<<10 and usb['ramFallbackMaxBytes']==384<<20
assert usb['operationDeadlineSeconds']==5 and usb['socketTimeoutSeconds']==1
for key in ['anonymousFileBeforeFirstWrite','noUserPathInput','directoryDescriptorsAndNoFollow','lostPiecesRequestedAgain','settingsAndMetadataRemainInternal','cachePrivatePerSession','closeOnStopAndProfileSwitch']:
 assert usb[key],key
assert not usb['fallbackWritesInternalRollingCache'] and not usb['extraListener'] and not usb['cacheChangesDuringPlayback'] and not usb['hardwareVerified']
recovery=info['missingProviderRecovery']
assert recovery['issue']==29 and recovery['triggerStatuses']==[404,410]
for key in ['originalRepositoryOnly','sameUniqueProviderId','preservesProfileSettingsAndEnablement','requestAndRedirectGuardsRetained']:assert recovery[key],key
assert recovery['maxManifestBytes']==recovery['maxScriptBytes']==1<<20
assert recovery['maxProviders']==256 and recovery['maxScriptRetries']==1 and recovery['totalDeadlineSeconds']==15
assert not recovery['testerRootCauseConfirmed'] and not recovery['hardwareVerified']
for name in ['sx::torrent_usb_cache_factory(', 'sx::Session::torrent_set_storage(', 'bt::Engine::set_cache_storage(', 'bt::Engine::cache_status(']:assert name in symbols,name
assert b'nuvio-usb-cache.elf' in native and b'retried missing script using refreshed repository metadata' in native
assert b'NUVIO_USB_CACHE_TEST_HELPER' not in native
helper=app/'nuvio-usb-cache.elf';payload=helper.read_bytes()
assert payload[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',payload,18)[0]==62
assert 0<len(payload)<=16<<20 and b'NUVIO_USB_CACHE_TEST_ROOT' not in payload
assert sha(helper)==sha(Path('/workspace/nuvio-cloudstream-build/app/nuvio-usb-cache.elf'))
assert 'nuvio-usb-cache.elf' in files and 'nuvio-usb-reader.elf' in files
for locale in ['en','de','es','fr','ar','zh-Hans','ja','ko']:
 for key in ['Internal SSD','Torrent cache location','Torrent cache location saved.']:
  text=key if locale=='en' else json.loads((root/f'ui/locales/{locale}.json').read_text())['strings'][key]
  assert text.encode() in native,(locale,key)

ports=json.loads((root/'third_party/cloudstream-ports/PORTS.json').read_text())
port_info=info['cloudstreamPorts']
assert port_info['standalonePs5'] and not port_info['androidBytecodeExecution']
assert port_info['bundledPorts']==len(ports['ports'])==20
for flag in ['sha256CheckedBeforeExecution','perProviderOptIn','profileIsolated','unsupportedProvidersVisible','unavailableSourcesVisible']:
 assert port_info[flag],flag
assert port_info['maxManifestBytes']==1<<20 and port_info['maxProviderEntries']==256
assert port_info['sourceRequestDeadlineSeconds']==15
expected_ports={'providers/cloudstream/'+p['id']+'.js':p['sha256'] for p in ports['ports']}
assert {name:digest for name,digest in files.items() if name.startswith('providers/cloudstream/')}==expected_ports
assert not any(name.endswith(('.cs3','.dex','.apk')) for name in files)
for p in ports['ports']:
 assert p['sha256'].encode() in native,p['id']
for name in ['LICENSE','UPSTREAM.json','PORTS.json']:
 assert files['licenses/cloudstream-ports/'+name]==sha(root/'third_party/cloudstream-ports'/name)
assert b'PS5 provider port verification failed' in native
assert b'Standalone PS5 ports' in native
assert b'http_get_manifest' in symbols.encode()
assert b'1.0.1.3' in native
assert b'Bnyro' in (app/'CREDITS.txt').read_bytes() and b'dw98-git' in (app/'CREDITS.txt').read_bytes()
preferred='native-app/third_party/cloudstream-ports/'
upstream=json.loads((root/'third_party/cloudstream-ports/UPSTREAM.json').read_text())
for name,digest in upstream['filesSha256'].items():
 assert hashlib.sha256(actual[preferred+name]).hexdigest()==digest,name
assert preferred+'src/kellerkino/index.js' in actual and preferred+'shared/http.js' in actual
assert info['version']=='1.0.1.3-native-experimental' and param['contentVersion']=='07.001.003'
logs=['providers-curl823.log','cloudstream-import.log','source-menus.log','account-source-tests.log',
      'live-repository-probe.log','localization.log','menu-previews.log','update-regression.log',
      'native-input-verification.log','native-build.log']
for name in logs:assert (evidence/name).is_file() and (evidence/name).stat().st_size,name
assert 'all 20 initialize without downloads' in (evidence/'providers-curl823.log').read_text()
assert 'KellerKino returns a stream with headers' in (evidence/'providers-curl823.log').read_text()
assert 'Ran 5 tests' in (evidence/'account-source-tests.log').read_text()
assert '"nativePorts":true,"ok":true,"providers":20,"unavailableEntries":7' in (evidence/'live-repository-probe.log').read_text()
assert '46 tests, 0 failures' in (evidence/'update-regression.log').read_text()
final=delivery/'PPSA99288.zip';shutil.copy2(package,final)
final_image=delivery/'NuvioMaxd-1.0.1.3-native.ffpfsc';shutil.copy2(image,final_image)
report={
 'version':info['version'],'sourceCommit':expected_commit,'sourceBranch':'builds/v1.0.1.3','workingTreeClean':True,
 'contentVersion':param['contentVersion'],'zip':final.name,'zipBytes':final.stat().st_size,'zipSha256':sha(final),
 'entryPoint':'PPSA99288/eboot.bin','ebootSha256':files['eboot.bin'],
 'packagedFiles':len(files),'containerErrors':0,'containerWarnings':0,'allExtractedFileHashesMatch':True,
 'zipMatchesNativeImage':True,'imageSha256':sha(image),'manifestFilesHashChecked':len(records),
 'sourceArchive':source_archive.name,'sourceArchiveSha256':sha(source_archive),'sourceArchiveMatchesGitBlobs':True,
 'sourceArchiveFiles':len(actual),'pinnedVlcSourcesIncluded':True,'preferredProviderSourceIncluded':True,
 'releaseFileSignaturePresent':False,'releaseSignatureNote':'Unsigned test. The original release key is unavailable; the dismissible Unverified build notice is retained.',
 'buildInputVerification':inputs,'externalBuildDriverSha256':sha(evidence/'build.sh'),
 'fullNativeApplicationCompiled':True,'hardwareVerified':False,'cloudstreamPorts':port_info,
 'livePublicRepositoryProbe':{'ok':True,'curl':'8.23.0','supportedPorts':20,'unavailableEntries':7,'mediaPlaybackTested':False},
 'verificationStatus':'Native app, ZIP/image/source and focused regressions verified; exact-source hosted CI and issue delivery pending',
 'githubReleaseCreated':False,'storeUpdateSubmitted':False,'issuesUnderTest':[29],'issuesClosed':[],
 'priorSubtitleAndUsbCacheFeaturesRetained':True,
 'fileSha256':files,'evidenceSha256':{name:sha(evidence/name) for name in logs}}
(delivery/'native-validation.json').write_text(json.dumps(report,indent=2)+'\n')
files_to_hash=[final,final_image,source_archive,delivery/'native-validation.json',delivery/'TESTING.md']
(delivery/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in files_to_hash))
print('Verified 20 bundled port hashes and preferred GPL source, clean source commit, complete PS5 app, ZIP/image round trip and exact source archive.')
print('Unconfirmed unsigned console test; no stable release or store update.')
print('ZIP SHA-256: '+sha(final))
