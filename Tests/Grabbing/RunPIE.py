"""Run from the editor Python console during a disposable, single-player PIE session."""
import unreal,json,time,traceback
from pathlib import Path
saved=Path(unreal.Paths.project_saved_dir());root=Path(unreal.Paths.project_dir())/'Tests/Grabbing'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world,'Start single-player PIE first'
jobs=[('BasicPIE.py','GrabPIEResults.json'),('WallMassPIE.py','GrabWallMassPIEResults.json'),('FragmentsPIE.py','GrabFragmentPIEResults.json')]
summary={'completed':False,'passed':False,'mode':'real single-player PIE','suites':[]}
index=0;started=0;baseline=0;environments=[]
def run_next():
    global started,baseline
    script,output=jobs[index];path=saved/output
    baseline=path.stat().st_mtime_ns if path.exists() else 0
    started=time.monotonic();env={'__name__':'__main__'};environments.append(env)
    exec((root/script).read_text(encoding='utf-8-sig'),env)
def complete(error=None):
    summary['completed']=True;summary['passed']=error is None and all(s['passed'] for s in summary['suites'])
    if error:summary['error']=error
    (saved/'GrabAcceptanceResults.json').write_text(json.dumps(summary,indent=2))
    unreal.unregister_slate_post_tick_callback(runner)
def poll(dt):
    global index
    try:
        path=saved/jobs[index][1]
        if path.exists() and path.stat().st_mtime_ns>baseline:
            data=json.loads(path.read_text());summary['suites'].append({'script':jobs[index][0],'output':jobs[index][1],'passed':data['passed'],'checks':len(data.get('checks',[]))})
            if not data['passed']:complete(data.get('error','suite failed'));return
            index+=1
            if index==len(jobs):complete();return
            run_next()
        elif time.monotonic()-started>100:complete('suite timeout: '+jobs[index][0])
    except Exception:complete(traceback.format_exc())
runner=unreal.register_slate_post_tick_callback(poll)
run_next()
