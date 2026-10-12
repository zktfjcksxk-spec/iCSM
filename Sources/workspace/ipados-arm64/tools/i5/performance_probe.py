#!/usr/bin/env python3
"""Collect explicitly requested, finite I5 performance observations on a device.

Commands are acknowledged before another command-file write. The game polls
that file once per second, so unacknowledged consecutive writes lose commands.
No configuration is archived, no assets are replaced, no test runs on launch.
"""
import argparse,json,subprocess,time,datetime
import hashlib
from pathlib import Path
P=Path(__file__).resolve().parents[2]
W=P.parent
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'
PY=W/'macos-arm64/.tools/bin/python'
parser=argparse.ArgumentParser()
parser.add_argument('--device',required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('action',choices=['command','sample'])
parser.add_argument('--command')
parser.add_argument('--tag')
parser.add_argument('--frames',type=int,default=1800)
parser.add_argument('--expect-pass',type=int,choices=[0,1])
parser.add_argument('--expect-temporary',type=int,choices=[0,1])
parser.add_argument('--expect-lightmap',type=int,choices=[0,1])
parser.add_argument('--begin-command',help='Explicit action queued after sampling begins, e.g. first firing')
a=parser.parse_args();a.output.mkdir(parents=True,exist_ok=True)

def copy(remote,local):
    return subprocess.run(['xcrun','devicectl','device','copy','from','--device',a.device,
        '--source',remote,'--destination',str(local),'--domain-type','appDataContainer',
        '--domain-identifier','local.csgo.ipados.i0probe','--quiet'],capture_output=True)

def send(command):
    # Use a device-specific source file: I5's original shared command.json
    # would race when collecting iPad and iPhone observations concurrently.
    seq=time.time_ns()//1000000
    source=a.output/('probe-command-'+hashlib.sha256(a.device.encode()).hexdigest()[:12]+'.json')
    source.write_text(json.dumps({'sequence':seq,'command':command})+'\n')
    subprocess.run(['xcrun','devicectl','device','copy','to','--device',a.device,
        '--source',str(source),'--destination','Documents/I5/command.json',
        '--domain-type','appDataContainer','--domain-identifier','local.csgo.ipados.i0probe','--quiet'],check=True)
    deadline=time.monotonic()+120
    while time.monotonic()<deadline:
        f=a.output/'current-status.json'
        if copy('Documents/I5/status.json',f).returncode==0:
            j=json.loads(f.read_text())
            observed=j.get('command_sequence',0)
            if observed>seq:
                raise RuntimeError('Command was superseded before acknowledgement')
            if observed==seq:
                print('Command acknowledged:',command,flush=True);return seq
        time.sleep(.5)
    raise RuntimeError('Command was not acknowledged')

if a.action=='command':
    if not a.command:parser.error('--command required')
    send(a.command)
else:
    if not a.tag:parser.error('--tag required')
    start_command=f'icsm_metal_profile {a.frames} {a.tag}'
    if a.begin_command:start_command+='; '+a.begin_command
    started=send(start_command)
    initial=json.loads((a.output/'current-status.json').read_text())
    (a.output/(a.tag+'-initial-status.json')).write_text(json.dumps(initial,indent=2)+'\n')
    if initial['startup']['visible']:raise RuntimeError('Startup cover is still visible; measurement excluded')
    if a.expect_lightmap is not None and bool(initial['metal']['combat']['ordered_lightmap_uploads'])!=bool(a.expect_lightmap):raise RuntimeError('Lightmap switch did not take effect')
    deadline=time.monotonic()+max(180,a.frames/30+30)
    while time.monotonic()<deadline:
        target=a.output/(a.tag+'.json')
        if copy('Documents/I5/performance/'+a.tag+'.json',target).returncode==0:
            j=json.loads(target.read_text())
            produced=datetime.datetime.fromisoformat(j.get('timestamp','1970-01-01T00:00:00+00:00')).timestamp()
            if j.get('tag')==a.tag and j.get('frames')==a.frames and produced>=started//1000:
                if a.expect_pass is not None and bool(j['pass_reuse'])!=bool(a.expect_pass):raise RuntimeError('Pass A/B switch did not take effect')
                if a.expect_temporary is not None and bool(j['temporary_reuse'])!=bool(a.expect_temporary):raise RuntimeError('Temporary A/B switch did not take effect')
                result=copy('Documents/I5/performance/'+a.tag+'.csv',a.output/(a.tag+'.csv'))
                if result.returncode:raise RuntimeError('Raw frame CSV missing')
                if copy('Documents/I5/status.json',a.output/(a.tag+'-status.json')).returncode:raise RuntimeError('Final status unavailable')
                final=json.loads((a.output/(a.tag+'-status.json')).read_text())
                if final['pid']!=initial['pid'] or final['startup']['visible']:raise RuntimeError('App restarted or startup cover returned during measurement')
                if a.expect_lightmap is not None and bool(final['metal']['combat']['ordered_lightmap_uploads'])!=bool(a.expect_lightmap):raise RuntimeError('Lightmap switch changed during observation')
                print(json.dumps({'tag':a.tag,'host_p50':j['metrics']['host_ms']['p50'],
                    'host_p95':j['metrics']['host_ms']['p95'],'scene_gpu_p50':j['metrics']['scene_gpu_ms'].get('p50'),
                    'passes':j['counts']['passes']['mean'],'visibility_allocations':j['counts']['visibility_allocations']['mean'],
                    'wait_ms':j['metrics']['wait_ms']['mean'],'thermal':[j['thermal_start'],j['thermal_end']]}),flush=True)
                break
        time.sleep(2)
    else:raise RuntimeError('Observation did not complete')
