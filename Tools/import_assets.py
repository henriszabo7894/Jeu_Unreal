"""
Importe automatiquement les assets de Jeu_Unreal/Assets_Sources dans Unreal :
  - modèles 3D (.fbx, .obj, .glb, .gltf)
  - textures (.jpg, .png, .tga), réglées correctement (couleur, normal map, rugosité...)
  - crée un matériau par asset et l'applique sur ses modèles
  - crée BP_Player avec le pistolet branché

Utilisation dans l'éditeur Unreal :
    Tools -> Execute Python Script...  puis choisir ce fichier.

Relançable : ce qui est déjà importé est sauté.

Organisation attendue de Assets_Sources :
    pistol_001_fbx/Pistol_001.fbx       -> /Game/Assets/Armes/Pistol
    texture/<partie>/*.png              -> textures du pistolet (une sous-partie par matériau)
    unfinished_building_raw/<id>/...    -> /Game/Assets/Batiment/<id>  (format Megascans)
    <autre_dossier>/...                 -> /Game/Assets/<autre_dossier>/...
"""
import os
import re
import struct
import tempfile
import zlib

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCES = os.path.join(PROJECT_DIR, "Assets_Sources")
GAME_ROOT = "/Game/Assets"
MATERIALS_DIR = GAME_ROOT + "/Materials"
MAX_TEXTURE_SIZE = 2048

MESH_EXT = (".fbx", ".obj", ".glb", ".gltf")
TEX_EXT = (".jpg", ".jpeg", ".png", ".tga")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Import LastSignal] " + msg)


def warn(msg):
    unreal.log_warning("[Import LastSignal] " + msg)


# ---------------------------------------------------------------------------
#  Identification des textures d'après leur nom
# ---------------------------------------------------------------------------

def texture_role(filename):
    """Renvoie le rôle d'une texture : basecolor, normal, roughness, metallic, ao, opacity, emissive, gloss ou None."""
    name = os.path.splitext(filename)[0].lower()
    tokens = set(t for t in re.split(r"[_\.\-\s]+", name) if t)
    if tokens & {"albedo", "basecolor", "diffuse", "color", "col", "diff"} or "base_color" in name:
        return "basecolor"
    if tokens & {"normal", "nor", "nrm", "norm"} or "normal" in name:
        return "normal"
    if tokens & {"roughness", "rough", "rgh"}:
        return "roughness"
    if tokens & {"gloss", "glossiness"}:
        return "gloss"
    if tokens & {"metal", "metallic", "metalness"}:
        return "metallic"
    if tokens & {"ao", "occlusion", "ambientocclusion"}:
        return "ao"
    if tokens & {"opacity", "alpha", "mask", "transparency"}:
        return "opacity"
    if tokens & {"emission", "emissive", "emit"}:
        return "emissive"
    return None  # specular, displacement, cavity, bump... : non utilisés


# ---------------------------------------------------------------------------
#  Import
# ---------------------------------------------------------------------------

def asset_name_for_file(path):
    base = os.path.splitext(os.path.basename(path))[0]
    return re.sub(r"[^A-Za-z0-9_]", "_", base)


def import_files(files, destination):
    """Importe une liste de fichiers dans un dossier Unreal. Renvoie les objets créés."""
    tasks = []
    for f in files:
        name = asset_name_for_file(f)
        if eal.does_asset_exist(destination + "/" + name) and not f.lower().endswith(MESH_EXT):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f)
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    if not tasks:
        return []
    asset_tools.import_asset_tasks(tasks)
    objects = []
    for task in tasks:
        for path in task.get_editor_property("imported_object_paths"):
            obj = unreal.load_asset(path)
            if obj:
                objects.append(obj)
    return objects


def configure_texture(tex, role):
    if not isinstance(tex, unreal.Texture2D):
        return
    if role == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif role in ("basecolor", "emissive"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", False)
    tex.set_editor_property("max_texture_size", MAX_TEXTURE_SIZE)
    eal.save_loaded_asset(tex)


def import_textures(files, destination):
    """Importe les textures et renvoie {rôle: texture}."""
    by_role = {}
    for f in files:
        role = texture_role(os.path.basename(f))
        if role is None:
            continue
        name = asset_name_for_file(f)
        asset_path = destination + "/" + name
        if eal.does_asset_exist(asset_path):
            tex = unreal.load_asset(asset_path)
        else:
            created = import_files([f], destination)
            tex = next((o for o in created if isinstance(o, unreal.Texture2D)), None)
        if tex:
            configure_texture(tex, role)
            by_role.setdefault(role, tex)
    return by_role


# ---------------------------------------------------------------------------
#  Textures par défaut (petites images générées) et matériaux maîtres
# ---------------------------------------------------------------------------

def write_png(path, rgb):
    """Écrit une image PNG 4x4 d'une seule couleur (sans bibliothèque externe)."""
    w = h = 4
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def default_texture(name, rgb, role):
    dest = MATERIALS_DIR + "/Defaults"
    path = dest + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    tmp = os.path.join(tempfile.gettempdir(), name + ".png")
    write_png(tmp, rgb)
    created = import_files([tmp], dest)
    tex = next((o for o in created if isinstance(o, unreal.Texture2D)), None)
    configure_texture(tex, role)
    return tex


def get_or_create_master(masked):
    name = "M_Master_Masked" if masked else "M_Master"
    path = MATERIALS_DIR + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)

    defaults = {
        "basecolor": default_texture("T_Default_White_sRGB", (255, 255, 255), "basecolor"),
        "linear_white": default_texture("T_Default_White_Linear", (255, 255, 255), "roughness"),
        "linear_black": default_texture("T_Default_Black_Linear", (0, 0, 0), "metallic"),
        "emissive": default_texture("T_Default_Black_sRGB", (0, 0, 0), "emissive"),
        "normal": default_texture("T_Default_Normal", (128, 128, 255), "normal"),
    }

    mat = asset_tools.create_asset(name, MATERIALS_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    normal = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL

    def tex_param(param, tex, sampler, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("texture", tex)
        node.set_editor_property("sampler_type", sampler)
        return node

    def scalar_param(param, value, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -450, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("default_value", value)
        return node

    def multiply(a, a_out, b, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, y)
        mel.connect_material_expressions(a, a_out, node, "A")
        mel.connect_material_expressions(b, "", node, "B")
        return node

    base = tex_param("BaseColorTex", defaults["basecolor"], color, -400)
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    nrm = tex_param("NormalTex", defaults["normal"], normal, -100)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

    rough = tex_param("RoughnessTex", defaults["linear_white"], linear, 200)
    rough_mul = multiply(rough, "R", scalar_param("RoughnessScale", 1.0, 250), 200)
    mel.connect_material_property(rough_mul, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = tex_param("MetallicTex", defaults["linear_black"], linear, 500)
    mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)

    ao = tex_param("AOTex", defaults["linear_white"], linear, 800)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    emis = tex_param("EmissiveTex", defaults["emissive"], color, 1100)
    emis_mul = multiply(emis, "RGB", scalar_param("EmissiveStrength", 1.0, 1150), 1100)
    mel.connect_material_property(emis_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if masked:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
        opacity = tex_param("OpacityTex", defaults["linear_white"], linear, 1400)
        mel.connect_material_property(opacity, "R", unreal.MaterialProperty.MP_OPACITY_MASK)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("Matériau maître créé : " + path)
    return mat


PARAM_FOR_ROLE = {
    "basecolor": "BaseColorTex",
    "normal": "NormalTex",
    "roughness": "RoughnessTex",
    "metallic": "MetallicTex",
    "ao": "AOTex",
    "emissive": "EmissiveTex",
    "opacity": "OpacityTex",
}


def make_material_instance(name, destination, textures):
    path = destination + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    masked = "opacity" in textures
    parent = get_or_create_master(masked)
    mi = asset_tools.create_asset(name, destination, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for role, tex in textures.items():
        param = PARAM_FOR_ROLE.get(role)
        if param:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    if "roughness" not in textures:
        # Pas de carte de rugosité (ex : Megascans "Specular") : surface assez mate
        mel.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", 0.8)
    if "emissive" in textures:
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 2.0)
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------------------
#  Application des matériaux sur les modèles
# ---------------------------------------------------------------------------

def assign_material(mesh, pick_material):
    """pick_material(nom_du_slot) -> matériau à appliquer (ou None pour laisser tel quel)."""
    if isinstance(mesh, unreal.StaticMesh):
        slots = mesh.get_editor_property("static_materials")
        for i, slot in enumerate(slots):
            mat = pick_material(str(slot.get_editor_property("material_slot_name")))
            if mat:
                mesh.set_material(i, mat)
        eal.save_loaded_asset(mesh)
    elif isinstance(mesh, unreal.SkeletalMesh):
        slots = mesh.get_editor_property("materials")
        for slot in slots:
            mat = pick_material(str(slot.get_editor_property("material_slot_name")))
            if mat:
                slot.set_editor_property("material_interface", mat)
        mesh.set_editor_property("materials", slots)
        eal.save_loaded_asset(mesh)


def meshes_in(objects):
    return [o for o in objects if isinstance(o, (unreal.StaticMesh, unreal.SkeletalMesh))]


def existing_meshes(destination):
    meshes = []
    for path in eal.list_assets(destination, recursive=False, include_folder=False):
        obj = unreal.load_asset(path)
        if isinstance(obj, (unreal.StaticMesh, unreal.SkeletalMesh)):
            meshes.append(obj)
    return meshes


def list_files(folder, extensions):
    out = []
    for root, _dirs, files in os.walk(folder):
        for f in sorted(files):
            if f.lower().endswith(extensions):
                out.append(os.path.join(root, f))
    return out


# ---------------------------------------------------------------------------
#  Cas 1 : le pistolet
# ---------------------------------------------------------------------------

def import_pistol(task):
    pistol_dir = os.path.join(SOURCES, "pistol_001_fbx")
    textures_dir = os.path.join(SOURCES, "texture")
    if not os.path.isdir(pistol_dir):
        return None
    dest = GAME_ROOT + "/Armes/Pistol"
    task.enter_progress_frame(1, "Pistolet")

    meshes = existing_meshes(dest)
    if not meshes:
        meshes = meshes_in(import_files(list_files(pistol_dir, MESH_EXT), dest))

    # Un matériau par sous-dossier de "texture" (pistul, light, reddot...)
    parts = {}
    if os.path.isdir(textures_dir):
        for part in sorted(os.listdir(textures_dir)):
            part_dir = os.path.join(textures_dir, part)
            if os.path.isdir(part_dir):
                tex = import_textures(list_files(part_dir, TEX_EXT), dest + "/Textures/" + part)
                if tex:
                    parts[part.lower()] = make_material_instance("MI_Pistol_" + part, dest, tex)

    main = parts.get("pistul") or parts.get("pistol") or (list(parts.values())[0] if parts else None)

    def pick(slot_name):
        s = slot_name.lower()
        for part, mat in parts.items():
            if part in s:
                return mat
        return main

    for mesh in meshes:
        assign_material(mesh, pick)
        log("Pistolet : %s (%s), matériaux par slot : %s" % (mesh.get_name(), type(mesh).__name__, ", ".join(parts.keys())))
    return meshes[0] if meshes else None


# ---------------------------------------------------------------------------
#  Cas 2 : dossiers au format Megascans (un dossier = un asset)
# ---------------------------------------------------------------------------

def import_asset_folder(folder, dest):
    """Un dossier = un asset : ses modèles + ses textures -> un matériau appliqué aux modèles."""
    meshes = existing_meshes(dest)
    if not meshes:
        mesh_files = list_files(folder, MESH_EXT)
        # Megascans fournit parfois un fichier par niveau de détail : on ne garde que le LOD0
        lod_files = [f for f in mesh_files if re.search(r"lod\d", os.path.basename(f), re.I)]
        if lod_files:
            lod0 = [f for f in lod_files if re.search(r"lod0", os.path.basename(f), re.I)]
            mesh_files = [f for f in mesh_files if f not in lod_files] + (lod0 or lod_files[:1])
        meshes = meshes_in(import_files(mesh_files, dest))
    tex = import_textures(list_files(folder, TEX_EXT), dest + "/Textures")
    if not tex:
        return 0
    mi = make_material_instance("MI_" + os.path.basename(folder), dest, tex)
    for mesh in meshes:
        assign_material(mesh, lambda _slot: mi)
    return len(meshes)


def import_pack(pack_name, game_folder, task):
    pack_dir = os.path.join(SOURCES, pack_name)
    subfolders = [d for d in sorted(os.listdir(pack_dir)) if os.path.isdir(os.path.join(pack_dir, d))]
    total_meshes = 0
    for i, sub in enumerate(subfolders):
        if task.should_cancel():
            warn("Import annulé.")
            break
        task.enter_progress_frame(1, "%s : %d / %d (%s)" % (pack_name, i + 1, len(subfolders), sub))
        try:
            total_meshes += import_asset_folder(os.path.join(pack_dir, sub), game_folder + "/" + sub)
        except Exception as exc:  # un asset cassé ne doit pas bloquer les autres
            warn("Échec sur %s : %s" % (sub, exc))
    # Modèles posés directement à la racine du pack
    if list_files(pack_dir, MESH_EXT) and not subfolders:
        total_meshes += import_asset_folder(pack_dir, game_folder)
    log("%s : %d dossiers, %d modèles 3D" % (pack_name, len(subfolders), total_meshes))


# ---------------------------------------------------------------------------
#  BP_Player avec l'arme branchée
# ---------------------------------------------------------------------------

def setup_player_blueprint(weapon_mesh):
    if weapon_mesh is None:
        return
    bp_path = "/Game/Blueprints/BP_Player"
    if eal.does_asset_exist(bp_path):
        bp = unreal.load_asset(bp_path)
    else:
        parent = unreal.load_class(None, "/Script/LastSignal.BodycamCharacter")
        if parent is None:
            warn("Classe BodycamCharacter introuvable : BP_Player non créé.")
            return
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = asset_tools.create_asset("BP_Player", "/Game/Blueprints", unreal.Blueprint, factory)

    cdo = unreal.get_default_object(bp.generated_class())
    if isinstance(weapon_mesh, unreal.SkeletalMesh):
        cdo.set_editor_property("weapon_skeletal_model", weapon_mesh)
    else:
        cdo.set_editor_property("weapon_model", weapon_mesh)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        warn("Compilation de BP_Player à faire à la main (bouton Compile) : %s" % exc)
    eal.save_loaded_asset(bp)
    log("BP_Player prêt avec l'arme : " + weapon_mesh.get_name())


# ---------------------------------------------------------------------------

def main():
    if not os.path.isdir(SOURCES):
        unreal.log_error("[Import LastSignal] Dossier introuvable : " + SOURCES)
        return

    packs = [d for d in sorted(os.listdir(SOURCES))
             if os.path.isdir(os.path.join(SOURCES, d)) and d not in ("pistol_001_fbx", "texture")]
    steps = 1 + sum(max(1, len([s for s in os.listdir(os.path.join(SOURCES, p)) if os.path.isdir(os.path.join(SOURCES, p, s))])) for p in packs)

    with unreal.ScopedSlowTask(steps, "Import des assets LastSignal...") as task:
        task.make_dialog(True)
        pistol = import_pistol(task)
        setup_player_blueprint(pistol)
        for pack in packs:
            game_folder = GAME_ROOT + ("/Batiment" if pack == "unfinished_building_raw" else "/" + asset_name_for_file(pack))
            import_pack(pack, game_folder, task)

    log("Terminé ! Regarde dans le Content Drawer : Content/Assets")


main()
