"""Lists the PIE player's primitive components and their collision setup -> Saved/PlayerCollisionPIE.json."""
import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
player=unreal.GameplayStatics.get_player_character(world,0)
out=[]
actors=[player]+list(player.get_attached_actors())
for a in actors:
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        out.append({'actor':a.get_name(),'component':c.get_name(),'class':c.get_class().get_name(),
            'collision':str(c.get_collision_enabled()),'profile':str(c.get_collision_profile_name()),
            'object_type':str(c.get_collision_object_type()),
            'vs_physics_body':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PHYSICS_BODY)),
            'vs_world_dynamic':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_WORLD_DYNAMIC)),
            'simulating':c.is_simulating_physics(),'visible':c.is_visible()})
cmc=player.character_movement
extra={k:str(cmc.get_editor_property(k)) for k in ['enable_physics_interaction','push_force_factor','initial_push_force_factor',
    'standing_downward_force_scale','touch_force_factor','repulsion_force','max_step_height','mass']}
(Path(unreal.Paths.project_saved_dir())/'PlayerCollisionPIE.json').write_text(json.dumps({'components':out,'cmc':extra},indent=1))
