"""
Crée (ou met à jour) la map /Game/Maps/L_Zone01 avec le générateur de zone.

Utilisation dans l'éditeur Unreal :
    Menu  Outils (Tools)  ->  Exécuter un script Python (Execute Python Script...)
    puis choisir ce fichier.
"""
import unreal

MAP_PATH = "/Game/Maps/L_Zone01"

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

builder_class = unreal.load_class(None, "/Script/LastSignal.ZoneBuilder")
if builder_class is None:
    unreal.log_error("LastSignal : classe ZoneBuilder introuvable. Le projet C++ est-il bien compile ?")
else:
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        level_subsystem.load_level(MAP_PATH)
        unreal.log("LastSignal : map existante chargee -> " + MAP_PATH)
    else:
        if not level_subsystem.new_level(MAP_PATH):
            unreal.log_error("LastSignal : impossible de creer la map " + MAP_PATH)
        else:
            unreal.log("LastSignal : nouvelle map creee -> " + MAP_PATH)

    already_there = [a for a in actor_subsystem.get_all_level_actors() if a.get_class() == builder_class]
    if not already_there:
        actor_subsystem.spawn_actor_from_class(builder_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        unreal.log("LastSignal : ZoneBuilder ajoute a la map")

    level_subsystem.save_current_level()
    unreal.log("LastSignal : map sauvegardee. Appuie sur Play !")
