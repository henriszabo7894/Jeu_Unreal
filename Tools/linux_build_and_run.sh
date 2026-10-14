#!/usr/bin/env bash
# Compile le projet LastSignal puis ouvre l'éditeur Unreal (Linux).
# Utilisation :  ./Tools/linux_build_and_run.sh ~/UnrealEngine
#   (le chemin = le dossier où tu as décompressé Unreal, celui qui contient "Engine/")
set -euo pipefail

UE_DIR="${1:-${UE_DIR:-$HOME/UnrealEngine}}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/LastSignal.uproject"

if [ ! -x "$UE_DIR/Engine/Binaries/Linux/UnrealEditor" ]; then
  echo "Unreal introuvable dans : $UE_DIR"
  echo "Donne le bon dossier, ex : ./Tools/linux_build_and_run.sh ~/UnrealEngine"
  exit 1
fi

echo "==> Compilation de LastSignal (la première fois peut prendre plusieurs minutes)..."
"$UE_DIR/Engine/Build/BatchFiles/Linux/Build.sh" LastSignalEditor Linux Development \
  -Project="$UPROJECT" -WaitMutex

# PC portable avec 2 cartes graphiques (Intel + NVIDIA) : on force la NVIDIA
if command -v nvidia-smi >/dev/null 2>&1; then
  echo "==> Carte NVIDIA détectée : Unreal utilisera la NVIDIA"
  export __NV_PRIME_RENDER_OFFLOAD=1
  export __VK_LAYER_NV_optimus=NVIDIA_only
  export __GLX_VENDOR_LIBRARY_NAME=nvidia
fi

# Sous Wayland, la fenêtre Unreal peut se figer : on passe par X11 (XWayland)
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
  echo "==> Session Wayland détectée : affichage via X11 pour plus de stabilité"
  export SDL_VIDEODRIVER=x11
fi

echo "==> Ouverture de l'éditeur..."
exec "$UE_DIR/Engine/Binaries/Linux/UnrealEditor" "$UPROJECT"
