#!/bin/bash

set -e
set -o pipefail

# Enable debug tracing if GITHUB_ACTIONS_DEBUG is set
if [[ "$GITHUB_ACTIONS_DEBUG" == "1" ]]; then
  set -x
fi

echo "::group::[OSSM] Starting get_next_version.sh"
echo "[INFO] Script invoked with arguments: $@"

commitMsg="$1"

echo "[INFO] Commit message: $commitMsg"

if [[ -z "$commitMsg" ]]; then
  echo "::error::[ERROR] Usage: $0 <commitMsg>"
  exit 1
fi

echo "::group::[OSSM] Determine bump type from commit message"
shopt -s nocasematch
if [[ "$commitMsg" == major* ]]; then
  BUMP_TYPE="major"
  echo "[INFO] Detected MAJOR version bump."
elif [[ "$commitMsg" == minor* ]]; then
  BUMP_TYPE="minor"
  echo "[INFO] Detected MINOR version bump."
else
  BUMP_TYPE="patch"
  echo "[INFO] Defaulting to PATCH version bump."
fi
shopt -u nocasematch
echo "[INFO] Bump type: $BUMP_TYPE"
echo "::endgroup::"

# Function to parse version string into major, minor, patch
parse_version() {
  local ver="$1"
  # Remove whitespace
  ver="${ver//[[:space:]]/}"
  # Extract numbers, default to 0 if missing
  local ver_major=0
  local ver_minor=0
  local ver_patch=0
  if [[ "$ver" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    ver_major="${BASH_REMATCH[1]}"
    ver_minor="${BASH_REMATCH[2]}"
    ver_patch="${BASH_REMATCH[3]}"
  fi
  echo "$ver_major $ver_minor $ver_patch"
}

# Function to compare two versions, returns 0 if v1 >= v2, 1 otherwise
version_gte() {
  local v1=($1)
  local v2=($2)
  for i in 0 1 2; do
    if (( ${v1[$i]} > ${v2[$i]} )); then
      return 0
    elif (( ${v1[$i]} < ${v2[$i]} )); then
      return 1
    fi
  done
  return 0
}

echo "::group::[OSSM] Fetch remote version from Supabase"
REMOTE_JSON=$(curl -sfL "https://acjajruwevyyatztbkdf.supabase.co/storage/v1/object/public/ossm-firmware/master/version.json" || echo "null")
REMOTE_VERSION=""
if [[ "$REMOTE_JSON" != "null" ]]; then
  REMOTE_VERSION=$(echo "$REMOTE_JSON" | grep -oE '"version" *: *"[0-9]+\.[0-9]+\.[0-9]+"' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
  echo "[INFO] Remote version found: $REMOTE_VERSION"
else
  echo "::warning::[WARN] Could not fetch remote version. Proceeding with local version only."
fi
echo "::endgroup::"

echo "::group::[OSSM] Read local version from Version.h"
VERSION_H_FILE="Software/src/constants/Version.h"
if [[ ! -f "$VERSION_H_FILE" ]]; then
  echo "::error::[ERROR] Local version file $VERSION_H_FILE not found!"
  exit 1
fi
LOCAL_VERSION=$(grep -oE '#define VERSION "[0-9]+\.[0-9]+\.[0-9]+"' "$VERSION_H_FILE" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
echo "[INFO] Local version: $LOCAL_VERSION"
echo "::endgroup::"

echo "::group::[OSSM] Compare remote and local versions"
USE_VERSION="$LOCAL_VERSION"
if [[ -n "$REMOTE_VERSION" ]]; then
  v1=($(parse_version "$REMOTE_VERSION"))
  v2=($(parse_version "$LOCAL_VERSION"))
  if version_gte "${v1[*]}" "${v2[*]}"; then
    USE_VERSION="$REMOTE_VERSION"
    echo "[INFO] Using remote version: $USE_VERSION"
  else
    echo "[INFO] Using local version: $USE_VERSION"
  fi
else
  echo "[INFO] No remote version available, using local version: $USE_VERSION"
fi
echo "::endgroup::"

echo "::group::[OSSM] Bump version"
parse_out="$(parse_version "$USE_VERSION")"
read ver_major ver_minor ver_patch <<< "$parse_out"
echo "[INFO] Current version: $ver_major.$ver_minor.$ver_patch"
if [[ "$BUMP_TYPE" == "major" ]]; then
  echo "[INFO] Bumping MAJOR version."
  ((ver_major = ver_major + 1))
  ver_minor=0
  ver_patch=0
  echo "[INFO] Bumping MAJOR version. New version: $ver_major.0.0"
elif [[ "$BUMP_TYPE" == "minor" ]]; then
  echo "[INFO] Bumping MINOR version."
  ((ver_minor = ver_minor + 1))
  ver_patch=0
  echo "[INFO] Bumping MINOR version. New version: $ver_major.$ver_minor.0"
else
  echo "[INFO] Bumping PATCH version."
  ((ver_patch = ver_patch + 1))
  echo "[INFO] Bumping PATCH version. New version: $ver_major.$ver_minor.$ver_patch"
fi
NEW_VERSION="$ver_major.$ver_minor.$ver_patch"
echo "[INFO] New version: $NEW_VERSION"
echo "::endgroup::"

echo "::notice::[OSSM] Next version: $NEW_VERSION"
echo "$NEW_VERSION"

if [[ -n "$GITHUB_OUTPUT" ]]; then
  echo "NEXT_VERSION=$NEW_VERSION" >> "$GITHUB_OUTPUT"
  echo "BUMP_TYPE=$BUMP_TYPE" >> "$GITHUB_OUTPUT"
fi

echo "::endgroup::"
