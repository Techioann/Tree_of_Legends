"""Cutting sweep: how often does a cut succeed, per segment and position, on fresh trees and on fragments.
Run from the editor Cmd box (Python) during single-player PIE on TEST:
    py "<project>/Tests/Cutting/SweepPIE.py"
Every attempt logs 'SWEEP <n> ...' right before the cut, so rejection reasons ('Cutting: rejected ...') in
Saved/Logs/Tree_of_Legends.log can be matched to attempts. Writes Saved/CutSweepResults.json. PIE world only."""
import unreal,json,math,traceback
from pathlib import Path
exec((Path(unreal.Paths.project_dir())/'Tests/Grabbing/Fixtures.py').read_text(encoding='utf-8-sig'))
fragment_class=unreal.TOLCutFragment
out=p/'CutSweepResults.json'
r={'completed':False,'attempts':[]}
FRACTIONS=(0.05,0.1,0.2,0.5,0.8,0.95)
attempt=[0]

def seg_list(actor):
    return [(s.start,s.end) for s in data(actor).segments]

def lerp(a,b,t):return [a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t]

def try_cut(actor,mesh,center,axis,label):
    attempt[0]+=1;n=attempt[0]
    rec={'n':n,'label':label,'actor':actor.get_name(),'center':center}
    try:
        h=trace_at(mesh,center,axis)
    except AssertionError:
        rec['result']='no hit';r['attempts'].append(rec);return None
    owner=unreal.get_default_object(unreal.GameplayStatics).call_method('BreakHitResult',(h,))[9]
    if owner!=actor:
        rec['result']='hit other actor';r['attempts'].append(rec);return None
    plane=cut.compute_cut_plane_from_hit(h)
    if plane is None:
        rec['result']='no plane';r['attempts'].append(rec);return None
    unreal.log('SWEEP %d %s'%(n,label))
    res=cut.cut_hit_segment(plane)
    frag=res if not isinstance(res,tuple) else res[-1]
    ok_=bool(res[0]) if isinstance(res,tuple) else frag is not None
    rec['result']='cut' if ok_ and frag else 'rejected'
    r['attempts'].append(rec)
    if frag:
        frag.cut_mesh.set_simulate_physics(False)
    return frag if ok_ else None

def cleanup():
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,fragment_class):a.destroy_actor()
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,tree_class):
        if a.get_name().startswith('SWEEP') or a.tags and 'SWEEP' in [str(t) for t in a.tags]:a.destroy_actor()

def fresh_tree(mesh,location):
    dormant=spawn_dormant(mesh,location,scale=unreal.Vector(.6,.6,.6))
    equip(100000)
    sc=source(dormant)
    tf=sc.get_world_transform()
    for z in (60,100,150,40,200):
        for angle in [0,math.pi/2,math.pi,-math.pi/2]:
            c=tf.transform_location(unreal.Vector(0,0,z))
            d=unreal.Vector(math.cos(angle)*600,math.sin(angle)*600,0)
            h=unreal.SystemLibrary.line_trace_single(world,c+d,c-d,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[player],unreal.DrawDebugTrace.NONE)
            if h is None:continue
            if unreal.get_default_object(unreal.GameplayStatics).call_method('BreakHitResult',(h,))[9]!=dormant:continue
            if cut.apply_tree_hit(h) is None:continue
            tree=cut.last_activated_tree
            if tree:
                tree.tags=[unreal.Name('SWEEP')]
                return tree
    dormant.destroy_actor()
    return None

try:
    base=player.get_actor_location()+unreal.Vector(3000,3000,0)
    base.z=0
    summary={}
    for mesh_name,mesh in (('Wood',wood),('Iron',iron)):
        probe=fresh_tree(mesh,base)
        assert probe,'could not activate a %s tree'%mesh_name
        segments=seg_list(probe)
        r[mesh_name+'_segments']=len(segments)
        probe.destroy_actor();cleanup()
        for si in range(len(segments)):
            for t in FRACTIONS:
                tree=fresh_tree(mesh,base)
                if not tree:continue
                m=proc(tree)
                s,e=seg_list(tree)[si]
                axis=[e.x-s.x,e.y-s.y,e.z-s.z]
                if dot(axis,axis)<1e-6:tree.destroy_actor();continue
                frag=try_cut(tree,m,lerp(s,e,t),norm(axis),'%s tree seg%d t%.2f'%(mesh_name,si,t))
                key='%s tree'%mesh_name;summary.setdefault(key,[0,0]);summary[key][1]+=1
                if frag:summary[key][0]+=1
                # Second cut on the detached fragment and on the remaining tree, each segment midpoint.
                for target,label in ((frag,'frag'),(tree,'stump')):
                    if not target:continue
                    tm=target.cut_mesh if label=='frag' else m
                    for sj,(s2,e2) in enumerate(seg_list(target)):
                        ax=[e2.x-s2.x,e2.y-s2.y,e2.z-s2.z]
                        if dot(ax,ax)<1e-6:continue
                        f2=try_cut(target,tm,lerp(s2,e2,0.35),norm(ax),'%s %s after seg%d t%.2f: seg%d'%(mesh_name,label,si,t,sj))
                        rec=r['attempts'][-1]
                        if rec['result'] in ('cut','rejected'):
                            key='%s %s'%(mesh_name,label);summary.setdefault(key,[0,0]);summary[key][1]+=1
                            if f2:summary[key][0]+=1
                        if f2:break  # geometry changed; move on
                tree.destroy_actor();cleanup()
    # Real game path: repeated axe hits (ApplyTreeHit) right next to forks until the cut completes.
    r['chops']=[]
    for mesh_name,mesh in (('Wood',wood),('Iron',iron)):
        for si in range(r[mesh_name+'_segments']):
            for t in (0.05,0.1,0.2):
                tree=fresh_tree(mesh,base)
                if not tree:continue
                m=proc(tree);s,e=seg_list(tree)[si]
                axis=[e.x-s.x,e.y-s.y,e.z-s.z]
                rec={'label':'%s seg%d t%.2f'%(mesh_name,si,t)}
                try:h=trace_at(m,lerp(s,e,t),norm(axis))
                except AssertionError:h=None
                if h is None or unreal.get_default_object(unreal.GameplayStatics).call_method('BreakHitResult',(h,))[9]!=tree:
                    rec['result']='no hit'
                else:
                    unreal.log('SWEEP chop '+rec['label'])
                    hits=0;rejected=0
                    while hits<400:
                        hits+=1
                        if cut.apply_tree_hit(h) is None:rejected+=1
                        if cut.last_chop_result.tree_cut or rejected>=3:break
                    rec['hits']=hits;rec['rejected']=rejected
                    rec['progress']=round(cut.last_chop_result.progress_normalized,4)
                    rec['gain']=cut.last_chop_result.progress_gain
                    rec['result']='cut' if cut.last_chop_result.tree_cut else 'NOT CUT'
                    if rec['result']=='cut':
                        # The player keeps clicking the same spot after the cut: no extra thin slices allowed.
                        before=len(unreal.GameplayStatics.get_all_actors_of_class(world,fragment_class))
                        for k in range(6):
                            try:h2=trace_at(m,lerp(s,e,t),norm(axis))
                            except AssertionError:continue
                            for _ in range(40):
                                if cut.apply_tree_hit(h2) is None or cut.last_chop_result.tree_cut:break
                        rec['extra_slices']=len(unreal.GameplayStatics.get_all_actors_of_class(world,fragment_class))-before
                r['chops'].append(rec)
                tree.destroy_actor();cleanup()
    chops=[c for c in r['chops'] if c['result']!='no hit']
    summary['axe chops near forks']=[sum(c['result']=='cut' for c in chops),len(chops)]
    r['summary']={k:'%d/%d cut'%(v[0],v[1]) for k,v in summary.items()}
except Exception:
    r['error']=traceback.format_exc()
r['completed']=True
out.write_text(json.dumps(r,indent=1))
unreal.log('CutSweep: done '+json.dumps(r.get('summary')))
