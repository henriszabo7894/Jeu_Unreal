#!/usr/bin/env bash
# Réduit toutes les textures de Assets_Sources à 2048 px maximum (au lieu de 8K).
# 8K = import très long, des dizaines de Go, et aucune différence visible en jeu.
# Les images plus petites que 2048 ne sont pas touchées.
# Utilisation :  ./Tools/reduce_textures.sh            (réduit à 2048)
#                ./Tools/reduce_textures.sh 1024       (réduit à 1024)
set -euo pipefail

SIZE="${1:-2048}"
SRC="$(cd "$(dirname "$0")/.." && pwd)/Assets_Sources"

if ! command -v mogrify >/dev/null 2>&1; then
  echo "ImageMagick n'est pas installé. Lance :  sudo apt install -y imagemagick"
  exit 1
fi

COUNT=$(find "$SRC" -type f \( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' -o -iname '*.tga' \) | wc -l)
echo "==> $COUNT images à vérifier dans $SRC (taille max : ${SIZE}px)"
echo "==> Ça peut prendre plusieurs minutes..."

find "$SRC" -type f \( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' -o -iname '*.tga' \) -print0 \
  | xargs -0 -n 8 -P "$(nproc)" mogrify -resize "${SIZE}x${SIZE}>"

echo "==> Terminé."
du -sh "$SRC"/*
