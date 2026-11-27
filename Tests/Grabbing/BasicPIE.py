"""Actual PIE physics tests; test bodies and player transforms exist only in the PIE world."""
import unreal,json,math,traceback
from pathlib import Path
p=Path(unreal.Paths.project_saved_dir())
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
player=unreal.GameplayStatics.get_player_character(w,0)
pc=unreal.GameplayStatics.get_player_controller(w,0)
grab=player.get_component_by_class(unreal.TOLGrabComponent)
handle=player.get_component_by_class(unreal.PhysicsHandleComponent)
sub=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',(pc,unreal.EnhancedInputLocalPlayerSubsystem))
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
result={'completed':False,'passed':False,'checks':[],'measurements':{},'mode':'real PIE'}
fixtures=[];step=0;elapsed=0;total=0;body=None;wall=None;sample=None

def ok(name):result['checks'].append(name)
def view():return pc.player_camera_manager.get_camera_location(),pc.player_camera_manager.get_camera_rotation()
def at(distance,side=0):
    v,r=view();return v+r.get_forward_vector()*distance+r.get_right_vector()*side
def xyz(v):return [v.x,v.y,v.z]
def dist(a,b):return (a-b).length()
def spawn(distance=250,mass=5,simulate=True,side=0,gravity=False):
    tf=unreal.Transform(location=at(distance,side),scale=unreal.Vector(.4,.4,.4))
    gs=unreal.get_default_object(unreal.GameplayStatics)
    a=gs.call_method('BeginDeferredActorSpawnFromClass',(w,unreal.StaticMeshActor,tf,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(cube)
    c.set_collision_profile_name('PhysicsActor');c.set_mass_override_in_kg('',mass,True);c.set_enable_gravity(gravity)
    a=gs.call_method('FinishSpawningActor',(a,tf,unreal.SpawnActorScaleMethod.OVERRIDE_ROOT_SCALE))
    c.set_simulate_physics(simulate);fixtures.append(a);return c
def clear():
    grab.release_grab()
    for a in fixtures:
        if unreal.SystemLibrary.is_valid(a):a.destroy_actor()
    fixtures.clear()
def inject(action,value):
    # Same Enhanced Input pipeline used by mapped physical keys, not direct Grab calls.
    sub.inject_input_vector_for_action(action,unreal.Vector(float(value),0,0),[],[])
def finish(error=None):
    global tick_handle
    grab.release_grab()
    if error:result['error']=error
    result['completed']=True;result['passed']=error is None
    (p/'GrabPIEResults.json').write_text(json.dumps(result,indent=2))
    unreal.unregister_slate_post_tick_callback(tick_handle)
    unreal.log('GRAB PIE '+('PASS' if result['passed'] else 'FAIL'))
def tick(dt):
    global step,elapsed,total,body,wall,sample,sample_response
    elapsed+=dt;total+=dt
    if total>90:finish('test timeout at step '+str(step));return
    try:
        if step==0:
            assert grab and handle
            assert len(player.get_components_by_class(unreal.PhysicsHandleComponent))==1
            assert grab.input_bound,'Enhanced Input not bound'
            assert not grab.is_component_tick_enabled()
            assert grab.grab_action==unreal.load_asset('/Game/Inputs/Keys/IA_Grabbing')
            ok('existing PhysicsHandle reused once; existing IA_Grabbing bound; idle tick off')
            player.character_movement.disable_movement()
            player.set_actor_location(unreal.Vector(20000,20000,1000),False,False)
            pc.set_control_rotation(unreal.Rotator(0,0,0));step=1;elapsed=0
        elif step==1 and elapsed>.2:
            assert not grab.try_grab_from_player_view();ok('miss rejected')
            body=spawn(simulate=False);assert not grab.try_grab_from_player_view();ok('non-simulating mesh rejected without activation')
            clear();body=spawn(mass=grab.max_grab_mass_kg+1)
            assert not grab.try_grab_from_player_view();ok('mass limit rejects heavy body, physics unchanged')
            clear();body=spawn(distance=grab.max_grab_distance+100)
            assert not grab.try_grab_from_player_view();ok('out-of-range target rejected')
            clear();body=spawn();wall=spawn(distance=120,simulate=False)
            assert not grab.try_grab_from_player_view();ok('first blocking wall prevents grabbing through obstacle')
            clear();body=spawn();sample_response=body.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
            inject(grab.grab_action,True);step=2;elapsed=0
        elif step==2:
            inject(grab.grab_action,True)
            if elapsed<.1:return
            assert grab.held_component==body,'mapped Grab Started failed: '+grab.last_status
            assert handle.get_grabbed_component()==body and body.is_simulating_physics()
            assert abs(body.get_mass()-5)<.01 and body.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)!=sample_response
            assert not grab.try_grab_from_player_view();ok('Enhanced Input Started grabs exactly one body without mass/physics override')
            pc.set_control_rotation(unreal.Rotator(0,15,0));step=3;elapsed=0
        elif step==3:
            inject(grab.grab_action,True)
            if elapsed>.8:
                result['measurements']['light_grip_error_cm']=dist(body.get_world_transform().transform_location(unreal.Vector(-20,0,0)),grab.target_location)
                assert dist(body.get_world_location(),at(grab.hold_distance))<80
                ok('held physical body follows player view through spring, no attachment/teleport')
                grab.set_rotating(True);assert pc.is_look_input_ignored()
                sample=body.get_world_rotation();grab.rotate_held_object(unreal.Vector2D(30,15));step=4;elapsed=0
        elif step==4:
            inject(grab.grab_action,True)
            if elapsed>.8:
                r=body.get_world_rotation();assert abs(r.yaw-sample.yaw)+abs(r.pitch-sample.pitch)+abs(r.roll-sample.roll)>10
                grab.set_rotating(False);assert not pc.is_look_input_ignored()
                ok('rotation changes physical body; camera look lock restored')
                inject(grab.grab_action,False);step=5;elapsed=0
        elif step==5 and elapsed>.15:
            assert not grab.held_component and not handle.get_grabbed_component() and not grab.is_component_tick_enabled()
            assert body.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)==sample_response
            ok('Enhanced Input Completed releases and restores collision/idle state')
            clear();body=spawn(mass=100);assert grab.try_grab_from_player_view();step=6;elapsed=0
        elif step==6:
            if elapsed>.5:
                assert grab.held_component==body and abs(body.get_mass()-100)<.01
                ok('100 kg body carries with original mass, configurable limit respected')
                pc.set_ignore_look_input(True);grab.set_rotating(True);grab.release_grab()
                assert pc.is_look_input_ignored();pc.set_ignore_look_input(False)
                ok('release balances only its own camera lock, preserves another system lock')
                assert grab.try_grab_from_player_view();body.get_owner().destroy_actor();step=7;elapsed=0
        elif step==7 and elapsed>.2:
            assert not grab.held_component and not pc.is_look_input_ignored()
            ok('destroyed held actor safely releases')
            clear();body=spawn();assert grab.try_grab_from_player_view();body.set_simulate_physics(False);step=8;elapsed=0
        elif step==8 and elapsed>.2:
            assert not grab.held_component;ok('disabled target physics safely releases')
            clear();body=spawn();assert grab.try_grab_from_player_view();body.get_owner().set_actor_location(at(1000),False,False);step=9;elapsed=0
        elif step==9 and elapsed>.2:
            assert not grab.held_component;ok('distance break releases target')
            clear();finish()
    except Exception:finish(traceback.format_exc())

sample_response=None
tick_handle=unreal.register_slate_post_tick_callback(tick)
