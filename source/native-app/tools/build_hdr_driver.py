"""Compile an isolated HDR extension into the exact pinned native GL runtime."""
import hashlib, json, pathlib, shlex, subprocess, sys, tarfile
root=pathlib.Path(__file__).resolve().parents[1]
gl,sdk,build=map(pathlib.Path,sys.argv[1:4]); stage=build/'hdr-driver';stage.mkdir(parents=True,exist_ok=True)
bundle=gl.parent
pins=json.loads((root/"dependencies.json").read_text())["experimentalHdrDriver"]
provenance=json.loads((bundle/"provenance.json").read_text())
assert provenance["runtime_source_commit"]==pins["baseRuntimeCommit"]
for name in ('ps5-opengl.tar','mesa-26.2.0.tar.xz','opengnm-psbc.tar'):
    archive=bundle/'sources'/name
    assert hashlib.sha256(archive.read_bytes()).hexdigest()==pins["sourceArchiveSha256"][name], "Source archive differs: "+name
    with tarfile.open(archive) as t:
        # Bundled sources are hash-checked by validate_build_dependencies.
        for m in t:
            p=pathlib.PurePosixPath(m.name)
            if p.is_absolute() or '..' in p.parts or m.issym() or m.islnk():continue
            dest=stage/pathlib.Path(*p.parts)
            if m.isdir():dest.mkdir(parents=True,exist_ok=True)
            elif m.isfile():
                dest.parent.mkdir(parents=True,exist_ok=True)
                data=t.extractfile(m).read()
                if not dest.exists() or dest.read_bytes()!=data:dest.write_bytes(data)
project=stage/'ps5-opengl'
psbc=stage/'opengnm-psbc/libpsbc/psbc_compile.h'
if 'PsbcShaderMetadata' not in psbc.read_text():
    subprocess.run(['git','apply','--include=libpsbc/psbc_compile.h',str(project/'toolchain/opengnm-psbc-ps5.patch')],cwd=stage/'opengnm-psbc',check=True)
native=project/'src/platform/ps5_agc_native_runtime.c'
s=native.read_text()
marker='static uint64_t runtime_render_marker = (uint64_t)RENDER_MARKER;'
if '#include "scanout_driver_extension.inc"' not in s:
    assert s.count(marker)==1
    s=s.replace(marker,'#include "scanout_driver_extension.inc"\n'+marker)
    s=s.replace('runtime_video_registered = 1;','runtime_video_registered = 1;\n    runtime_scanout_format = VIDEO_OUT_PIXEL_FORMAT;')
    native.write_text(s)
(project/'src/platform/scanout_driver_extension.inc').write_bytes((root/'gl/scanout_driver_extension.inc').read_bytes())
args=[]
for token in shlex.split((bundle/'runtime-config.txt').read_text())[1:]:
    if token.startswith('-I') or token.startswith('-iquote'):
        if '/third_party/mesa-26.2.0/' in token:token=token.split('/third_party/mesa-26.2.0/',1)[1];args+=['-I'+str(stage/'mesa-26.2.0'/token)]
        elif '/src/gallium/ps5' in token:args+=['-I'+str(project/'src/gallium/ps5')]
        elif '/opengnm-psbc/' in token:args+=['-I'+str(stage/'opengnm-psbc'/token.split('/opengnm-psbc/',1)[1])]
        continue
    args.append(token)
args+=['-femulated-tls','-Wno-error=unused-function','-Dmain=ps5_agc_gate2_run','-DAGC_TRIANGLE_SUBMIT=1','-DAGC_RUNTIME_PACKAGES=1']
obj=build/'hdr_driver.o'
subprocess.run([str(sdk/'bin/prospero-clang'),*args,'-c',str(project/'src/platform/ps5_agc_runtime_backend.c'),'-o',str(obj)],check=True)
print('Experimental driver HDR object:',hashlib.sha256(obj.read_bytes()).hexdigest())
