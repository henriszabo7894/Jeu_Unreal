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

echo "==> Ouverture de l'éditeur..."
exec "$UE_DIR/Engine/Binaries/Linux/UnrealEditor" "$UPROJECT"
