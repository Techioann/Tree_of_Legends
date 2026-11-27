"""Sprint + stamina + smooth HUD bar acceptance test. Run from the editor Cmd box during single-player PIE (TEST map):
    py "<project>/Tests/Sprint/SprintPIE.py"
Drives the real character over several seconds of world time (walk, sprint until exhausted, rest) and writes
Saved/SprintPIEResults.json. Only touches the PIE world."""
import unreal,json,math,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'SprintPIEResults.json'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
player=unreal.GameplayStatics.get_player_character(world,0)
r={'completed':False,'samples':[]}
state={'phase':'walk','t0':None,'handle':None,'dir':1}

def now():return unreal.GameplayStatics.get_time_seconds(world)
def stats():
    for c in player.get_components_by_class(unreal.ActorComponent):
        if 'PlayerStats' in c.get_class().get_name():return c
def bar():
    hud=player.get_editor_property('HUD')
    return hud.get_editor_property('Stamina') if hud else None

def finish(err=None):
    if err:r['error']=err
    r['completed']=True
    unreal.unregister_slate_post_tick_callback(state['handle'])
    sprint.set_sprint_held(False)
    out.write_text(json.dumps(r,indent=1))
    unreal.log('SprintPIE: done')

def tick(dt):
    try:
        t=now()
        if state['t0'] is None:state['t0']=t
        el=t-state['t0']
        # Run back and forth along X so the character stays on the map.
        if el>0 and int(el/3.0)%2==1:state['dir']=-1
        else:state['dir']=1
        player.add_movement_input(unreal.Vector(state['dir'],0,0),1.0,False)
        st=stats();b=bar()
        v=player.get_velocity();speed=math.hypot(v.x,v.y)
        s={'t':round(el,3),'phase':state['phase'],'speed':round(speed,1),'max_speed':round(player.character_movement.get_max_speed(),1),
           'stamina':round(st.get_editor_property('Stamina'),3),'sprinting':sprint.sprinting,'exhausted':sprint.exhausted,
           'bar':round(b.percent,4) if b else None}
        r['samples'].append(s)
        if state['phase']=='walk' and el>2.0:
            state['phase']='sprint';sprint.set_sprint_held(True)
        elif state['phase']=='sprint' and (sprint.exhausted or el>15):
            state['phase']='rest';state['rest_t']=el;sprint.set_sprint_held(False)
        elif state['phase']=='rest' and el-state['rest_t']>4.0:
            finish()
    except Exception:
        finish(traceback.format_exc())

try:
    sprint=player.get_component_by_class(unreal.TOLSprintComponent)
    assert sprint,'no TOLSprintComponent on the player'
    r['multiplier']=sprint.speed_multiplier
    r['max_walk_speed']=player.character_movement.max_walk_speed
    state['handle']=unreal.register_slate_post_tick_callback(tick)
except Exception:
    r['error']=traceback.format_exc();r['completed']=True
    out.write_text(json.dumps(r,indent=1))
