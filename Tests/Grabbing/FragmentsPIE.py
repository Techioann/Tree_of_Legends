import unreal,json,traceback
from pathlib import Path
p=Path(unreal.Paths.project_saved_dir())
exec((Path(unreal.Paths.project_dir())/'Tests/Grabbing/Fixtures.py').read_text(encoding='utf-8-sig'))
pc=unreal.GameplayStatics.get_player_controller(world,0)
grab=player.get_component_by_class(unreal.TOLGrabComponent)
sub=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',(pc,unreal.EnhancedInputLocalPlayerSubsystem))
original_profiles=cut.get_editor_property('dormant_tree_profiles')
assert cut.set_dormant_tree_unit_price(wood,100000)
assert cut.set_dormant_tree_unit_price(iron,100000)
r={'completed':False,'passed':False,'checks':[],'models':[],'mode':'real PIE'}
stage=0;elapsed=0;model_index=0;f=None;center=None;axis=None;record=None;target=None

def point(s,t):return s.start+(s.end-s.start)*t
def injection(action,v):sub.inject_input_vector_for_action(action,v,[],[])
def finish(error=None):
    grab.release_grab();cut.set_editor_property('dormant_tree_profiles',original_profiles);r['completed']=True;r['passed']=error is None
    if error:r['error']=error
    (p/'GrabFragmentPIEResults.json').write_text(json.dumps(r,indent=2))
    unreal.unregister_slate_post_tick_callback(h)
def tick(dt):
    global stage,elapsed,model_index,f,center,axis,record,target
    elapsed+=dt
    try:
        if stage==0:
            player.character_movement.disable_movement();pc.set_control_rotation(unreal.Rotator(0,90 if model_index else 0,0))
            mesh=[wood,iron][model_index]
            a=spawn_dormant(mesh,unreal.Vector(45000+model_index*3000,45000,1500))
            equip(10000)
            apply(trace_at(source(a),[0,0,400] if model_index==0 else [4.5,24,200],[0,0,1] if model_index==0 else [9,48,200]))
            active=registry.get_active_tree(source(a));m=proc(active)
            s=data(active).segments[[4,6][model_index]]
            center=point(s,.75);axis=s.end-s.start
            plane=cut.compute_cut_plane_from_hit(trace_at(m,xyz(point(s,.5)),xyz(axis),xyz(axis)))
            output=cut.cut_hit_segment(plane);f=output[-1] if isinstance(output,tuple) else output
            assert f and f.cut_mesh.is_simulating_physics()
            assert data(f).value_minor>0
            f.cut_mesh.set_simulate_physics(False)
            # Isolate the detached test piece from its standing source tree before aiming.
            # Otherwise the opposite branch can legitimately be the first blocking hit.
            f.set_actor_location(f.get_actor_location()+unreal.Vector(0,-2000,0),False,False)
            target=f.cut_mesh.get_world_transform().transform_location(center)
            record={'model':['Wood','Iron'][model_index],'mass_kg':f.cut_mesh.get_mass(),'material':str(f.cut_mesh.get_material(0)),'value_before':data(f).value_minor}
            r['models'].append(record);stage=.5;elapsed=0
        elif stage==.5 and elapsed>.4:
            offset=pc.player_camera_manager.get_camera_location()-player.get_actor_location()
            forward=pc.player_camera_manager.get_camera_rotation().get_forward_vector()
            player.set_actor_location(target-forward*250-offset,False,False)
            stage=1;elapsed=0
        elif stage==1 and elapsed>.4:
            f.cut_mesh.set_simulate_physics(True)
            assert grab.try_grab_from_player_view(),grab.last_status
            assert grab.held_component==f.cut_mesh
            record['held_mass_kg']=grab.held_mass_kg
            assert abs(grab.held_mass_kg-record['mass_kg'])<.01
            r['checks'].append(record['model']+': actual cut branch grabbed with original mass/material/value')
            record['camera_before']=[pc.get_control_rotation().pitch,pc.get_control_rotation().yaw]
            record['rotation_before']=f.get_actor_rotation().yaw
            stage=2;elapsed=0
        elif stage==2:
            injection(grab.rotate_action,unreal.Vector(1,0,0))
            injection(grab.look_action,unreal.Vector(5,0,0))
            if elapsed>1:
                assert grab.rotating and pc.is_look_input_ignored()
                rot=pc.get_control_rotation();assert abs(rot.pitch-record['camera_before'][0])+abs(rot.yaw-record['camera_before'][1])<.01
                assert abs(f.get_actor_rotation().yaw-record['rotation_before'])>5
                r['checks'].append(record['model']+': Enhanced Input rotate + mouse turns body while camera stays still')
                injection(grab.rotate_action,unreal.Vector());stage=3;elapsed=0
        elif stage==3 and elapsed>.4:
            assert not grab.rotating and not pc.is_look_input_ignored()
            assert f.cut_mesh.is_simulating_physics() and grab.held_component==f.cut_mesh
            assert f.cut_mesh.get_material(0) and data(f).value_minor==record['value_before']
            plane=cut.compute_cut_plane_from_hit(trace_at(f.cut_mesh,xyz(center),xyz(axis),xyz(axis)))
            output=cut.cut_hit_segment(plane);newf=output[-1] if isinstance(output,tuple) else output
            record['recut_created_fragment']=newf is not None
            record['recut_still_held']=str(grab.held_component)
            record['recut_plane']=str(plane)
            record['recut_status']=str(cut.get_editor_property('last_cut_message')) if hasattr(cut,'last_cut_message') else str(cut)
            assert newf and not grab.held_component,'recut must release old handle'
            assert data(f).value_minor+data(newf).value_minor==record['value_before']
            assert f.cut_mesh.is_simulating_physics() and newf.cut_mesh.is_simulating_physics()
            r['checks'].append(record['model']+': held branch recut safely releases handle; both pieces simulate and conserve value')
            grab.release_grab();f.destroy_actor();newf.destroy_actor();model_index+=1
            if model_index==2:finish()
            else:stage=0;elapsed=0
    except Exception:finish(traceback.format_exc())
h=unreal.register_slate_post_tick_callback(tick)

