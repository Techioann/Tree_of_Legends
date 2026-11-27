"""Does SetMassOverrideInKg actually reach the physics solver? Same impulse on bodies of different mass;
dv must scale as 1/mass. Also checks a real cut fragment (ProceduralMesh) the same way. -> Saved/MassPIEResults.json"""
import unreal,json,traceback
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
gs=unreal.get_default_object(unreal.GameplayStatics)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
BASE=unreal.Vector(-60000,-60000,20000)  # in the air, far from everything; gravity only affects Z
IMPULSE=100000.0  # kg*cm/s along +X  ->  dv = IMPULSE/mass cm/s
r={'completed':False,'impulse':IMPULSE,'bodies':[]}
bodies=[];stage=0;t=0
def spawn(i,mass):
    tf=unreal.Transform(location=BASE+unreal.Vector(0,i*500,0),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1))
    a=gs.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.StaticMeshActor,tf,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    a.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);a.static_mesh_component.set_static_mesh(cube)
    a=gs.call_method('FinishSpawningActor',(a,tf,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    c=a.static_mesh_component;c.set_simulate_physics(True);c.set_enable_gravity(False)
    if mass:c.set_mass_override_in_kg('',mass,True)
    return a,c
def tick(dt):
    global stage,t,bodies
    try:
        t+=dt
        if stage==0:
            for i,m in enumerate([None,40.0,800.0,5000.0]):bodies.append((m,)+spawn(i,m))
            frags=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,'/Script/Tree_of_Legends.TOLCutFragment'))
            for f in frags[:3]:
                c=f.cut_mesh
                if c.is_simulating_physics():bodies.append(('fragment',f,c))
            stage=1;t=0
        elif stage==1 and t>0.3:
            for b in bodies:
                c=b[2];c.set_physics_linear_velocity(unreal.Vector(0,0,0));c.set_physics_angular_velocity_in_degrees(unreal.Vector(0,0,0))
            stage=2;t=0
        elif stage==2 and t>0.1:
            for b in bodies:b[2].add_impulse(unreal.Vector(IMPULSE,0,0),'',False)
            stage=3;t=0
        elif stage==3 and t>0.2:
            for m,a,c in bodies:
                mass=c.get_mass();v=c.get_physics_linear_velocity().x
                r['bodies'].append({'override':m,'get_mass_kg':round(mass,2),'dv_x_cm_s':round(v,3),
                    'expected_dv':round(IMPULSE/mass,3),'effective_mass_kg':round(IMPULSE/v,2) if abs(v)>1e-6 else None,
                    'name':a.get_name()})
                if m!='fragment':a.destroy_actor()
            r['completed']=True
            Path(unreal.Paths.project_saved_dir(),'MassPIEResults.json').write_text(json.dumps(r,indent=1))
            unreal.unregister_slate_post_tick_callback(h)
    except Exception:
        r['error']=traceback.format_exc();Path(unreal.Paths.project_saved_dir(),'MassPIEResults.json').write_text(json.dumps(r,indent=1))
        unreal.unregister_slate_post_tick_callback(h)
h=unreal.register_slate_post_tick_callback(tick)
