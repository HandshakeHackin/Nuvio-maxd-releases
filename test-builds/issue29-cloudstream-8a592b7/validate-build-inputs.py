"""Verify exact app inputs; report SDK's unused, random-build-ID target CLIs.

The stock 741-output check was run and its three failures are retained. No source
pins are changed. Three target executables are not host tools or app link inputs;
relink proofs establish that their only non-reproducible bytes are 16-byte UUIDs.
"""
from pathlib import Path
import hashlib,json,struct,sys
root=Path('/workspace/nuvio-cloudstream/source/native-app')
evidence=Path('/workspace/nuvio-cloudstream-evidence')
sdk,gl=map(Path,sys.argv[1:3])
pins=json.loads((root/'dependencies.json').read_text())
maintenance=pins['maintenanceLibraries']
def sha(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
assert sha(gl/'manifest.sha256')==pins['opengl']['manifestSha256']
for line in (gl/'manifest.sha256').read_text().splitlines():
 digest,name=line.split('  ',1);assert not Path(name).is_absolute() and '..' not in Path(name).parts
 assert sha(gl/name)==digest,name
for name,digest in pins['sdk']['coreFileSha256'].items():
 if name not in maintenance['targetFileSha256']:assert sha(sdk/name)==digest,name
assert sha(root/maintenance['recipe'])==maintenance['recipeSha256']
for name,digest in maintenance.get('extraRecipeSha256',{}).items():assert sha(root/name)==digest,name
for patch in maintenance.get('libassPatches',[])+maintenance.get('libxml2Patches',[]):assert sha(root/patch['path'])==patch['sha256']
if rebuild:=maintenance.get('artifactRebuild'):
 assert sha(root/rebuild['receipt'])==rebuild['receiptSha256']
 receipt=json.loads((root/rebuild['receipt']).read_text());assert receipt['source']==maintenance['sources']['ffmpeg']
 assert receipt['independentCleanRebuildsIdentical'] is True
 for name,output in receipt['outputs'].items():assert output['sha256']==output['independentCleanBuildSha256']==maintenance['targetFileSha256'][name]
proof=json.loads((evidence/'volatile-target-cli-proof.json').read_text())
assert set(proof)=={'target/user/homebrew/bin/'+name for name in ['curl','iconv','psl']}
exact=0;exceptions=[]
for name,expected in maintenance['targetFileSha256'].items():
 actual=sha(sdk/name)
 if name not in proof:
  assert actual==expected,'App dependency differs: '+name;exact+=1;continue
 record=proof[name];data=(sdk/name).read_bytes()
 assert record['allOtherBytesIdentical'] and record['appBuildInput'] is False and record['packaged'] is False
 assert len(data)==record['sameSize'] and actual==record['originalSha256'] and record['buildIdBytes']==16
 # Independently locate the ELF build-ID descriptor; do not trust an arbitrary
 # ignored byte range from the proof file.
 offset=struct.unpack_from('<Q',data,40)[0];count=struct.unpack_from('<H',data,60)[0];index=struct.unpack_from('<H',data,62)[0]
 sections=[struct.unpack_from('<IIQQQQIIQQ',data,offset+n*64) for n in range(count)]
 section=sections[index];names=data[section[4]:section[4]+section[5]]
 notes=[s for s in sections if names[s[0]:].split(b'\0',1)[0]==b'.note.gnu.build-id'];assert len(notes)==1
 note=notes[0];ns,ds,kind=struct.unpack_from('<III',data,note[4]);start=note[4]+12+((ns+3)&~3)
 assert kind==3 and ds==16 and start==record['buildIdOffset']
 assert hashlib.sha256(data[:start]+bytes(16)+data[start+16:]).hexdigest()==record['normalizedSha256']
 exceptions.append({'path':name,'pinnedSha256':expected,'actualSha256':actual,'reason':'Unused PS5 target CLI; SDK linker --build-id=uuid; independent relink differs only in this 16-byte ID. Not invoked, linked or packaged.'})
assert sha(root/'third_party/json.hpp')==pins['json']['sha256']
for name,digest in pins.get('dependencyRefresh20261113',{}).get('assetSha256',{}).items():assert sha(root/name)==digest,name
provider=pins['providerRuntime']
assert sha(root/'third_party/provider-runtime/runtime.js')==provider['bundleSha256']
assert sha(root/'third_party/provider-runtime/package-lock.json')==provider['packageLockSha256']
assert exact==738 and len(exceptions)==3
(evidence/'validated-build-inputs.json').write_text(json.dumps({'exactOutputPins':exact,'totalDeclaredOutputPins':741,'allAppLibrariesAndHeadersMatch':True,'originalFullOutputPinCheckPassed':False,'unusedTargetUtilityExceptions':exceptions,'sdkCoreGraphicsSourceRecipeAndAssetPinsMatch':True},indent=2)+'\n')
print('App build inputs verified: 738 exact output hashes plus SDK/core/graphics/source/recipe/assets. Three unused PS5 target CLI UUID variations documented; original full-output check remains recorded as failed.')
