"""PIE acceptance tests on the actual map's dormant trees and disposable fixtures."""
import unreal,json,math,traceback
from pathlib import Path
p=Path(unreal.Paths.project_saved_dir())
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
player=unreal.GameplayStatics.get_player_character(world,0)
cut=player.get_component_by_class(unreal.TOLCutTraceComponent)
registry=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetWorldSubsystem',(world,unreal.TOLTreeActivationSubsystem))
tree_class=unreal.load_class(None,'/Game/Blueprints/Actors/BP_Tree_Procedural.BP_Tree_Procedural_C')
wood=unreal.load_asset('/Game/Meshes/Trees/SM_Tree_Wood')
iron=unreal.load_asset('/Game/Meshes/Trees/SM_Tree_Iron')
axes=unreal.load_asset('/Game/Data/DT_Axe')
report={'passed':False,'checks':[],'iron_axes':[]}
def active_count():return len(unreal.GameplayStatics.get_all_actors_of_class(world,tree_class))
def equip(durability=100):assert cut.equip_axe(unreal.DataTableRowHandle(data_table=axes,row_name='Wa'),durability)
def ok(name):report['checks'].append(name)
def xyz(v):return [v.x,v.y,v.z]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def norm(a):return [x/math.sqrt(dot(a,a)) for x in a]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def proc(actor):return next(c for c in actor.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name()=='ProceduralTree')
def data(actor):return actor.get_component_by_class(unreal.TOLCutMeshData)
def source(actor):return actor.get_component_by_class(unreal.StaticMeshComponent)
def close_tf(a,b):
    return all(abs(x-y)<1e-5 for x,y in zip(xyz(a.translation)+xyz(a.scale3d)+[a.rotation.x,a.rotation.y,a.rotation.z,a.rotation.w],xyz(b.translation)+xyz(b.scale3d)+[b.rotation.x,b.rotation.y,b.rotation.z,b.rotation.w]))
def trace_at(component,center,axis=(0,0,1),expected=None):
    radial=norm(cross(axis,[1,0,0] if abs(axis[2])>.9 else [0,0,1]))
    other=norm(cross(axis,radial));tf=component.get_world_transform()
    for angle in [0,math.pi,math.pi/2,-math.pi/2,math.pi/4,-math.pi/4]:
        r=[math.cos(angle)*radial[k]+math.sin(angle)*other[k] for k in range(3)]
        a=tf.transform_location(unreal.Vector(*[center[k]+r[k]*320 for k in range(3)]))
        b=tf.transform_location(unreal.Vector(*[center[k]-r[k]*320 for k in range(3)]))
        h=unreal.SystemLibrary.line_trace_single(world,a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[player],unreal.DrawDebugTrace.NONE)
        if h is None:continue
        if expected is not None:
            plane=cut.compute_cut_plane_from_hit(h)
            if plane is None:continue
            direction=tf.transform_location(unreal.Vector(*expected))-tf.transform_location(unreal.Vector())
            if abs(dot(norm(xyz(direction)),xyz(plane.world_plane_normal)))<.9999:continue
        return h
    raise AssertionError('No matching surface hit at '+str(center))
def apply(h):
    result=cut.apply_tree_hit(h)
    if result is None:
        report['active_diagnostics']=[{'actor':a.get_name(),'validation':data(a).validation_message,'valid':data(a).segments_valid,'progress':[c.progress for c in data(a).active_cuts]} for a in unreal.GameplayStatics.get_all_actors_of_class(world,tree_class)]
        report['durability_at_rejection']=cut.tool_durability
        report['rejected_hit']=str(unreal.get_default_object(unreal.GameplayStatics).call_method('BreakHitResult',(h,)))
        report['raw_plane_valid']=cut.compute_cut_plane_from_hit(h) is not None
    assert result is not None,'Hit rejected'
    return cut.last_chop_result
def spawn_dormant(mesh,location,rotation=unreal.Rotator(),scale=unreal.Vector(.25,.25,.25)):
    tf=unreal.Transform(location=location,rotation=rotation,scale=scale)
    gs=unreal.get_default_object(unreal.GameplayStatics)
    actor=gs.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.StaticMeshActor,tf,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    c=actor.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(mesh)
    return gs.call_method('FinishSpawningActor',(actor,tf,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))

