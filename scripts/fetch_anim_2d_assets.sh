#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${ROOT_DIR}/examples/anim_2d/assets"

if [[ $# -lt 1 ]]; then
  cat <<'EOF'
Usage: scripts/fetch_anim_2d_assets.sh /path/to/craftpix_pack.zip
   or: scripts/fetch_anim_2d_assets.sh /path/to/unzipped_pack_dir

Download the Craftpix freebie zip in your browser, then pass its path here.
The script will extract the sprite folders into:
  examples/anim_2d/assets
EOF
  exit 1
fi

INPUT_PATH="$1"
BASE_DIR=""

if [[ -d "${INPUT_PATH}" ]]; then
  BASE_DIR="${INPUT_PATH}"
else
  if [[ ! -f "${INPUT_PATH}" ]]; then
    echo "Path not found: ${INPUT_PATH}" >&2
    exit 1
  fi

  if ! command -v unzip >/dev/null 2>&1; then
    echo "Missing 'unzip' command. Please install it and retry." >&2
    exit 1
  fi

  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "${TMP_DIR}"' EXIT

  unzip -q "${INPUT_PATH}" -d "${TMP_DIR}"

  IDLE_PATH="$(find "${TMP_DIR}" -type f -path "*/1/Idle.png" -print -quit || true)"
  if [[ -z "${IDLE_PATH}" ]]; then
    echo "Could not find expected '1/Idle.png' in the zip. Check the pack structure." >&2
    exit 1
  fi
  BASE_DIR="$(cd "$(dirname "${IDLE_PATH}")/.." && pwd)"
fi

if [[ ! -d "${BASE_DIR}/1" ]]; then
  echo "Expected a folder containing 1..6 sprite directories: ${BASE_DIR}" >&2
  exit 1
fi

if [[ ! -f "${BASE_DIR}/1/Idle.png" ]]; then
  echo "Could not find expected '1/Idle.png' in ${BASE_DIR}." >&2
  exit 1
fi
mkdir -p "${TARGET_DIR}"

for variant in 1 2 3 4 5 6; do
  if [[ -d "${BASE_DIR}/${variant}" ]]; then
    rm -rf "${TARGET_DIR:?}/${variant}"
    cp -R "${BASE_DIR}/${variant}" "${TARGET_DIR}/"
  else
    echo "Warning: missing variant folder ${variant} in ${BASE_DIR}" >&2
  fi
done

echo "Assets installed to ${TARGET_DIR}"
