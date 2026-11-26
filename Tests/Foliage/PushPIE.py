"""How far does the player push logs of known mass? Run from the editor Cmd box during single-player PIE:
    py "<project>/Tests/Foliage/PushPIE.py"
Writes Saved/PushPIEResults.json. Spawns disposable actors in the PIE world only; restores the player afterwards.
Each case: the player walks (+X) into a 60 cm x 4 m log lying across the path for 3 s of WORLD time."""
import unreal,json,traceback
from pathlib import Path
saved=Path(unreal.Paths.project_saved_dir())
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
player=unreal.GameplayStatics.get_player_character(world,0)
pc=unreal.GameplayStatics.get_player_controller(world,0)
cube=unreal.load_asset('/Engine/BasicShapes/Cube');cyl=unreal.load_asset('/Engine/BasicShapes/Cylinder')
cmc=player.character_movement;capsule=player.capsule_component
gs=unreal.get_default_object(unreal.GameplayStatics)
axe=next((c for c in player.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Axe'),None)
orig={'push':cmc.push_force_factor,'initial':cmc.initial_push_force_factor,'axe':axe.get_collision_enabled() if axe else None,'capsule':capsule.get_collision_enabled()}
# Configurations: 'current' = whatever the character has now; 'proposed' = axe without collision + ~600 N push.
configs={'current':{},'push600N_axe_off':{'axe_off':True,'push':60000.0,'initial':100.0},
         'push600N_axe_off_capsule_query':{'axe_off':True,'capsule_query_only':True,'push':60000.0,'initial':100.0}}
cases=[(cfg,m) for cfg in configs for m in (40.0,200.0,800.0,5000.0)]
configs['nothing_pushes']={'axe_off':True,'capsule_query_only':True,'push':0.0,'initial':0.0,'interaction_off':True}
cases=[(cfg,m) for cfg in ('current','push600N_axe_off') for m in (40.0,200.0,800.0,5000.0)]
RADIUS=60.0  # 1.2 m thick: too high to step onto, so the player has to push it
r={'completed':False,'original':{k:str(v) for k,v in orig.items()},'configs':configs,'results':[]}
BASE=unreal.Vector(60000,60000,3000);DURATION=3.0
spawned=[]
def now():return unreal.GameplayStatics.get_time_seconds(world)
def spawn(mesh,loc,rot,scale,mass=None):
    tf=unreal.Transform(location=loc,rotation=rot,scale=scale)
    a=gs.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.StaticMeshActor,tf,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    a.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);a.static_mesh_component.set_static_mesh(mesh)
    a=gs.call_method('FinishSpawningActor',(a,tf,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE));c=a.static_mesh_component
    if mass:c.set_collision_profile_name('PhysicsActor');c.set_simulate_physics(True);c.set_mass_override_in_kg('',mass,True)
    spawned.append(a);return a
def apply_config(cfg):
    c=configs[cfg]
    cmc.set_editor_property('push_force_factor',c.get('push',orig['push']))
    cmc.set_editor_property('initial_push_force_factor',c.get('initial',orig['initial']))
    if axe:axe.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION if c.get('axe_off') else orig['axe'])
    capsule.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY if c.get('capsule_query_only') else orig['capsule'])
    cmc.set_editor_property('enable_physics_interaction',not c.get('interaction_off'))
floor=spawn(cube,BASE,unreal.Rotator(0,0,0),unreal.Vector(200,200,1))  # 200 m x 200 m
log=None;stage=0;t0=0;idx=0;start=None;track=[]
def finish(err=None):
    r['completed']=True
    if err:r['error']=err
    try:
        cmc.set_editor_property('push_force_factor',orig['push']);cmc.set_editor_property('initial_push_force_factor',orig['initial'])
        if axe:axe.set_collision_enabled(orig['axe'])
        capsule.set_collision_enabled(orig['capsule']);cmc.set_editor_property('enable_physics_interaction',True)
    except Exception:r['restore_error']=traceback.format_exc()
    for a in spawned:
        try:a.destroy_actor()
        except Exception:pass
    (saved/'PushPIEResults.json').write_text(json.dumps(r,indent=1))
    unreal.unregister_slate_post_tick_callback(h)
def tick(dt):
    global stage,t0,idx,start,log
    try:
        if stage==0:
            cfg,mass=cases[idx];apply_config(cfg)
            log=spawn(cyl,BASE+unreal.Vector(0,0,50+RADIUS+1),unreal.Rotator(roll=90,pitch=0,yaw=0),unreal.Vector(RADIUS/50,RADIUS/50,4),mass)
            player.set_actor_location_and_rotation(BASE+unreal.Vector(-200,0,50+100),unreal.Rotator(0,0,0),False,True)
            cmc.stop_movement_immediately();pc.set_control_rotation(unreal.Rotator(0,0,0))
            stage=1;t0=now()
        elif stage==1 and now()-t0>1.0:  # settle
            start=log.get_actor_location();stage=2;t0=now();track.clear()
        elif stage==2:
            t=now()-t0
            player.add_movement_input(globals().get('DIRECTION',unreal.Vector(1,0,0)),1.0)
            if len(track)<=t/0.5:
                pl=player.get_actor_location()
                track.append([round(t,2),round(log.get_actor_location().x-start.x,1),round(pl.x-BASE.x,1),round(pl.z-BASE.z,1),
                    None])
            if 1.4<t<1.5 and 'probe' not in r:
                # What is between the log and the player? Trace from in front of the log back toward the player.
                probe=[];ll=log.get_actor_location();pl=player.get_actor_location()
                for dz in range(-60,161,20):
                    for dy in (-30,0,30):
                        a=unreal.Vector(ll.x,pl.y+dy,BASE.z+50+RADIUS+dz);b=unreal.Vector(pl.x-80,pl.y+dy,BASE.z+50+RADIUS+dz)
                        hs=unreal.SystemLibrary.line_trace_multi(world,a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[log],unreal.DrawDebugTrace.NONE,True)
                        for hh in hs or []:
                            f=gs.call_method('BreakHitResult',(hh,))
                            probe.append({'z':dz,'y':dy,'comp':f[10].get_name() if f[10] else None,'actor':f[9].get_name() if f[9] else None,'x_rel_player':round(f[4].x-pl.x,1)})
                r['probe']=probe;r['probe_gap_log_center_minus_player']=round(ll.x-pl.x,1)
            if t>=DURATION:
                c=log.static_mesh_component
                r['results'].append({'config':cases[idx][0],'mass_kg':round(c.get_mass(),1),
                    'log_moved_cm':round(log.get_actor_location().x-start.x,1),
                    'log_speed_cm_s':round(c.get_physics_linear_velocity().length(),1),
                    'track [t, log dx, player x, player z]':list(track)})
                log.destroy_actor();spawned.remove(log);idx+=1
                if idx==len(cases):finish();return
                stage=0
    except Exception:finish(traceback.format_exc())
h=unreal.register_slate_post_tick_callback(tick)
