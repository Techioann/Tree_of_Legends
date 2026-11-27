import unreal,json,traceback
from pathlib import Path
p=Path(unreal.Paths.project_saved_dir())
exec((Path(unreal.Paths.project_dir())/'Tests/Grabbing/BasicPIE.py').read_text().split('\ndef tick(dt):')[0].replace('GrabPIEResults.json','GrabWallMassPIEResults.json'))
result['measurements']={'mass_response':[]};step=0;elapsed=0;total=0;trial=0;start=None;frames=[];max_penetration=0
def tick(dt):
    global step,elapsed,total,body,wall,trial,start,max_penetration
    elapsed+=dt;total+=dt;frames.append(dt)
    try:
        if total>45:raise AssertionError('timeout at '+str(step))
        if step==0:
            clear();player.character_movement.disable_movement()
            player.set_actor_location(unreal.Vector(60000,60000,2000),False,False)
            pc.set_control_rotation(unreal.Rotator());step=1;elapsed=0
        elif step==1 and elapsed>.3:
            body=spawn(mass=[5,100][trial]);assert grab.try_grab_from_player_view();step=2;elapsed=0
        elif step==2 and elapsed>.4:
            start=body.get_world_location();grab.adjust_hold_distance(70);step=3;elapsed=0
        elif step==3 and elapsed>.15:
            result['measurements']['mass_response'].append({'kg':body.get_mass(),'after_seconds':elapsed,'moved_cm':dist(start,body.get_world_location())})
            clear();trial+=1
            if trial<2:step=1;elapsed=0
            else:
                light,heavy=result['measurements']['mass_response']
                assert light['moved_cm']>heavy['moved_cm']+1,str(result['measurements'])
                ok('same spring: 100 kg responds more slowly than 5 kg; real mass preserved')
                body=spawn(mass=5);assert grab.try_grab_from_player_view()
                wall=spawn(distance=330,simulate=False)
                wall.set_world_scale3d(unreal.Vector(.4,4,4))
                grab.adjust_hold_distance(100);step=4;elapsed=0
        elif step==4:
            forward=pc.player_camera_manager.get_camera_rotation().get_forward_vector()
            # Wall is perpendicular to +X; cube and wall have no rotation in this fixture.
            penetration=body.get_world_location().x+20-(wall.get_world_location().x-20)
            max_penetration=max(max_penetration,penetration)
            if elapsed>1.5:
                assert grab.held_component==body
                assert max_penetration<2,'held body penetrated wall: '+str(max_penetration)
                assert grab.target_location.x<=wall.get_world_location().x-27
                result['measurements']['max_wall_penetration_cm']=max_penetration
                ok('held body collides with wall; desired grip stays on near side; no attachment teleport')
                grab.release_grab();assert body.is_simulating_physics()
                ok('release after wall contact keeps physical simulation')
                result['measurements']['mean_dt']=sum(frames)/len(frames)
                clear();finish()
    except Exception:finish(traceback.format_exc())
tick_handle=unreal.register_slate_post_tick_callback(tick)

