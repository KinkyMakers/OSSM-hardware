#!/usr/bin/env bash
# Uploads one community build to the public ossm-firmware bucket: the commit's
# immutable folder (<dest>/<short sha>/) first, then the branch root (<dest>/)
# that the community flasher reads. Objects are replaced with x-upsert, so a
# failure never leaves the branch root half-deleted.
#
# Usage: publish_community_firmware.sh <dest> <build-dir> <version>
#   dest       bucket path, which must start with kinkymakers/
#   build-dir  folder holding firmware, bootloader, partitions, web-installer
#
# Required env: SUPABASE_ACCESS_TOKEN, SUPABASE_PROJECT_ID, GITHUB_SHA,
# GITHUB_REF_NAME, GITHUB_REPOSITORY
set -euo pipefail

DEST="${1:?destination path required}"
BUILD_DIR="${2:?build directory required}"
VERSION="${3:?version required}"
BUCKET="ossm-firmware"
BASE="https://${SUPABASE_PROJECT_ID}.supabase.co/storage/v1"
TEMPLATE="$(dirname "$0")/../artifacts/manifest.json"

# Community builds must never overwrite the legacy official channels.
if [[ ! "$DEST" =~ ^kinkymakers/[A-Za-z0-9._/-]+$ || "$DEST" == *..* ]]; then
  echo "ERROR: community firmware may only be published under kinkymakers/" >&2
  exit 1
fi

for file in firmware.bin bootloader.bin partitions.bin web-installer.bin; do
  [ -s "${BUILD_DIR}/${file}" ] || { echo "ERROR: missing ${BUILD_DIR}/${file}" >&2; exit 1; }
done

# The storage REST API needs a project JWT; exchange a platform token the same
# way rebuild_registry.sh does, and keep the derived key out of the logs.
if [[ "${SUPABASE_ACCESS_TOKEN}" == sbp_* ]]; then
  STORAGE_KEY=$(curl -sf "https://api.supabase.com/v1/projects/${SUPABASE_PROJECT_ID}/api-keys?reveal=true" \
      -H "Authorization: Bearer ${SUPABASE_ACCESS_TOKEN}" \
    | jq -re '.[] | select(.name == "service_role") | .api_key')
else
  STORAGE_KEY="${SUPABASE_ACCESS_TOKEN}"
fi
echo "::add-mask::${STORAGE_KEY}"

upload() {
  curl -sf -X POST "${BASE}/object/${BUCKET}/$2" \
    -H "Authorization: Bearer ${STORAGE_KEY}" \
    -H "apikey: ${STORAGE_KEY}" \
    -H "Content-Type: $3" \
    -H "x-upsert: true" \
    -H "cache-control: no-cache" \
    --data-binary "@$1" > /dev/null
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
IFS=. read -r MAJOR MINOR PATCH <<< "${VERSION%%[-+]*}"
jq -n --arg version "$VERSION" --argjson major "$MAJOR" --argjson minor "$MINOR" \
  --argjson patch "$PATCH" --arg build "$GITHUB_SHA" --arg branch "$GITHUB_REF_NAME" \
  --arg repository "$GITHUB_REPOSITORY" \
  '{version: $version, major: $major, minor: $minor, patch: $patch, buildSha: $build,
    branch: $branch, repository: $repository, official: false}' > "${WORK}/version.json"

for target in "${DEST}/${GITHUB_SHA::7}" "${DEST}"; do
  for file in firmware.bin bootloader.bin partitions.bin web-installer.bin; do
    upload "${BUILD_DIR}/${file}" "${target}/${file}" application/octet-stream
  done
  upload "${WORK}/version.json" "${target}/version.json" application/json
  sed "s#{{ branchName }}#${target}#g" "$TEMPLATE" > "${WORK}/manifest.json"
  upload "${WORK}/manifest.json" "${target}/manifest.json" application/json
  echo "Published ${VERSION} (${GITHUB_SHA::7}) to ${BUCKET}/${target}/"
done
