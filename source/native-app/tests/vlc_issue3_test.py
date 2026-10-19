from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parents[1];f=r/'test-output/vlc-issue3';f.mkdir(exist_ok=True)
paths=[]
for i,c in enumerate(['red','lime','blue','blue']):
 p=f/(str(i)+'.mkv');paths.append(str(p))
 subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','lavfi','-i',f'color={c}:size=192x108:rate=24','-t','2','-c:v','ffv1','-pix_fmt','yuv420p10le' if i==3 else 'yuv420p',str(p)],check=True)
a=f/'decoder-attempts.txt';a.write_text('');env=os.environ.copy();env['NUVIO_DECODER_ATTEMPTS']=str(a)
subprocess.run([str(r/'test-output/vlc-issue3-host'),*paths],env=env,check=True,timeout=35)
