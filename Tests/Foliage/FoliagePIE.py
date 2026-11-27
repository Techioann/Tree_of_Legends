"""Foliage -> cuttable tree acceptance test. Run from the editor Cmd box (Python) during single-player PIE on World:
    py "<project>/Tests/Foliage/FoliagePIE.py"
Writes Saved/FoliagePIEResults.json. Only touches the PIE world."""
import unreal,json,math,traceback
from pathlib import Path
exec((Path(unreal.Paths.project_dir())/'Tests/Grabbing/Fixtures.py').read_text(encoding='utf-8-sig'))
gs=unreal.get_default_object(unreal.GameplayStatics)
r={'completed':False,'passed':False,'checks':[],'mode':'real PIE, foliage instance'}
out=p/'FoliagePIEResults.json'

def hit_fields(h):
    t=gs.call_method('BreakHitResult',(h,))
    # BreakHitResult: blocking, initial overlap, time, distance, location, impact point, normal, impact normal,
    # phys mat, actor, component, bone, bone, item, element, face, start, end
    return {'component':t[10],'item':t[13]}

def trace_instance(ism,index,tf,heights=(150,100,200,60,250)):
    for z in heights:
        for angle in [0,math.pi/2,math.pi,-math.pi/2,math.pi/4,-math.pi/4]:
            c=tf.transform_location(unreal.Vector(0,0,z))
            d=unreal.Vector(math.cos(angle)*400,math.sin(angle)*400,0)
            h=unreal.SystemLibrary.line_trace_single(world,c+d,c-d,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[player],unreal.DrawDebugTrace.NONE)
            if h is None:continue
            f=hit_fields(h)
            if f['component']==ism and f['item']==index:return h,c+d,c-d
    return None

try:
    ifas=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.InstancedFoliageActor)
    comps=[c for a in ifas for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent)
           if c.static_mesh==wood and c.get_instance_count()>0]
    r['foliage_components']=len(comps);r['foliage_instances']=sum(c.get_instance_count() for c in comps)
    assert comps,'No SM_Tree_Wood foliage instances in the PIE world'
    ok('foliage instances found')
    target=None
    for ism in comps:
        for i in range(min(ism.get_instance_count(),40)):
            tf=ism.get_instance_transform(i,True)
            if isinstance(tf,tuple):tf=tf[-1]
            t=trace_instance(ism,i,tf)
            if t:target=(ism,i,tf)+t;break
        if target:break
    assert target,'Could not trace a single foliage instance without hitting something else first'
    ism,index,tf,h,start,end=target
    ok('trace hits foliage instance %d'%index)
    equip(10000)
    before=ism.get_instance_count();trees_before=active_count()
    chop=apply(h)
    assert chop.applied and chop.progress_gain>0,'first chop not applied'
    ok('first chop applied (gain %.3f)'%chop.progress_gain)
    assert ism.get_instance_count()==before-1,'foliage instance was not removed'
    ok('foliage instance removed')
    tree=cut.last_activated_tree
    assert tree and active_count()==trees_before+1,'no BP_Tree_Procedural spawned'
    assert close_tf(tree.get_actor_transform(),tf) or tree.get_actor_location().distance_to(tf.translation)<0.1,'tree transform differs from instance'
    ok('BP_Tree_Procedural spawned at the instance transform')
    r['tree_value']=str(data(tree).get_value_text())
    hits=1
    while not cut.last_chop_result.tree_cut and hits<400:
        h2=unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[player],unreal.DrawDebugTrace.NONE)
        assert h2 is not None,'second trace missed the activated tree'
        assert hit_fields(h2)['component'].get_owner()==tree,'follow-up chop does not hit the activated tree'
        apply(h2);hits+=1
    r['hits_to_cut']=hits
    assert cut.last_chop_result.tree_cut,'tree not cut after %d hits'%hits
    ok('tree cut after %d hits'%hits)
    frag=cut.last_cut_fragment
    assert frag and frag.cut_mesh.is_simulating_physics(),'no simulating fragment'
    ok('fragment simulates physics (grabbable)')
    r['fragment_value_minor']=data(frag).value_minor;r['fragment_value']=str(data(frag).get_value_text())
    r['fragment_mass_kg']=frag.cut_mesh.get_mass()
    assert data(frag).value_minor>0,'fragment has no coin value (check DT_Tree Price Per M3 for the SM_Tree_Wood row)'
    ok('fragment has coin value')
    r['passed']=True
except Exception:
    r['error']=traceback.format_exc()
r['completed']=True;r['checks']=report['checks']
out.write_text(json.dumps(r,indent=2))
unreal.log('FoliagePIE: '+('PASSED' if r['passed'] else 'FAILED'))
