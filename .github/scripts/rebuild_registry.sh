#!/usr/bin/env bash
# Rebuilds <BUCKET>/registry.json from the bucket's actual contents and uploads
# it atomically. The registry is derived state: this script never reads the old
# registry.json, so a corrupt, empty, or missing one cannot propagate. Safe to
# run at any time; the last run to finish always converges on ground truth.
#
# Bucket layout: branch names containing slashes are stored as real nested
# paths (branch kareem/foo lives under kareem/foo/, NOT under a folder
# literally named kareem%2Ffoo — the CLI and the storage API percent-decode
# object paths). Registry keys, by contrast, are URL-encoded (kareem%2Ffoo)
# because the web flasher builds manifest URLs from them and the encoding
# decodes back to the nested path when fetched. Discovery below therefore
# recurses into folders that hold no manifest.json (pure namespaces) and
# re-encodes the joined path for the registry key.
#
# Usage:
#   rebuild_registry.sh                  rebuild and upload registry.json
#   rebuild_registry.sh --list-branches  print discovered branch keys, one per
#                                        line, and write nothing (used by the
#                                        cleanup workflow to find stale
#                                        branches)
#
# Required env:
#   SUPABASE_ACCESS_TOKEN  - platform token (sbp_...) or project service key
#   SUPABASE_PROJECT_ID    - project ref
set -euo pipefail

BUCKET="ossm-firmware"
# Release channels are published by the version/release workflows and are not
# dev branches; the web flasher's Development dropdown must not list them.
PROTECTED_CHANNELS=("master" "alpha" "production")
BASE="https://${SUPABASE_PROJECT_ID}.supabase.co/storage/v1"
LIST_LIMIT=1000
# Namespace nesting bound: a branch named a/b/c/d is four segments deep.
MAX_DEPTH=4

MODE="rebuild"
if [ "${1:-}" = "--list-branches" ]; then
  MODE="list"
fi

# The storage REST API needs a project JWT. CI stores a platform access token
# (sbp_...), so exchange it for the service_role key the same way the CLI does.
if [[ "${SUPABASE_ACCESS_TOKEN}" == sbp_* ]]; then
  STORAGE_KEY=$(curl -sf "https://api.supabase.com/v1/projects/${SUPABASE_PROJECT_ID}/api-keys?reveal=true" \
      -H "Authorization: Bearer ${SUPABASE_ACCESS_TOKEN}" \
    | jq -re '.[] | select(.name == "service_role") | .api_key')
else
  STORAGE_KEY="${SUPABASE_ACCESS_TOKEN}"
fi

# One-level listing. The prefix travels in the JSON body, so nested paths need
# no URL escaping. Folders come back with "id": null; files with a uuid id.
list() {
  curl -sf -X POST "${BASE}/object/list/${BUCKET}" \
    -H "Authorization: Bearer ${STORAGE_KEY}" \
    -H "apikey: ${STORAGE_KEY}" \
    -H "Content-Type: application/json" \
    -d "$(jq -nc --arg p "$1" --argjson l "$LIST_LIMIT" '{prefix: $p, limit: $l, offset: 0, sortBy: {column: "name", order: "asc"}}')"
}

# A listing that hits the limit means entries were silently dropped; a registry
# built from it would look complete while missing branches. Refuse instead.
assert_not_truncated() {
  local count
  count=$(echo "$1" | jq 'length')
  if [ "$count" -ge "$LIST_LIMIT" ]; then
    echo "ERROR: listing for prefix '$2' hit the ${LIST_LIMIT}-entry cap; refusing to build a truncated registry" >&2
    exit 1
  fi
}

has_manifest() {
  echo "$1" | jq -e 'map(select(.id != null and .name == "manifest.json")) | length > 0' > /dev/null
}

is_protected() {
  local name="$1" p
  for p in "${PROTECTED_CHANNELS[@]}"; do
    [ "$name" = "$p" ] && return 0
  done
  return 1
}

# Mirror of the workflows' URL encoder (encode_branch step), which encodes
# slash, plus, and space. Slashes are re-added by the joiner in discover(), so
# per segment only plus and space need escaping.
encode_segment() {
  echo "$1" | sed 's/+/%2B/g' | sed 's/ /%20/g'
}

REGISTRY='{}'
BRANCH_KEYS=""

# Record the branch root found at bucket path $1 (e.g. kareem/foo) under
# registry key $2 (e.g. kareem%2Ffoo). Its commits are subfolders that look
# like short git hashes and hold their own manifest.json, sorted by that
# manifest's created_at so the array keeps the old append order.
register_branch() {
  local path="$1" key="$2" entries="$3"
  local pairs sub subentries created commits
  pairs='[]'
  while IFS= read -r sub; do
    [ -n "$sub" ] || continue
    echo "$sub" | grep -qE '^[0-9a-f]{7,12}$' || continue
    subentries=$(list "${path}/${sub}/")
    if has_manifest "$subentries"; then
      created=$(echo "$subentries" | jq -r '.[] | select(.id != null and .name == "manifest.json") | .created_at')
      pairs=$(echo "$pairs" | jq --arg c "$sub" --arg t "$created" '. + [{c:$c, t:$t}]')
    fi
  done < <(echo "$entries" | jq -r '.[] | select(.id == null) | .name')

  commits=$(echo "$pairs" | jq 'sort_by(.t) | map(.c)')
  REGISTRY=$(echo "$REGISTRY" | jq --arg b "$key" --argjson c "$commits" '. + {($b): $c}')
  BRANCH_KEYS="${BRANCH_KEYS}${key}"$'\n'
}

# Depth-first discovery. A folder holding a manifest.json is a branch root; a
# folder without one is a namespace segment of a slash-named branch and is
# recursed into.
discover() {
  local path="$1" key="$2" depth="$3"
  local entries sub
  entries=$(list "${path}/")
  assert_not_truncated "$entries" "${path}/"

  if has_manifest "$entries"; then
    register_branch "$path" "$key" "$entries"
    return 0
  fi

  if [ "$depth" -ge "$MAX_DEPTH" ]; then
    echo "WARN: no manifest within depth ${MAX_DEPTH} under '${path}/'; skipping" >&2
    return 0
  fi

  while IFS= read -r sub; do
    [ -n "$sub" ] || continue
    discover "${path}/${sub}" "${key}%2F$(encode_segment "$sub")" $((depth + 1))
  done < <(echo "$entries" | jq -r '.[] | select(.id == null) | .name')
}

TOP=$(list "")
assert_not_truncated "$TOP" ""

while IFS= read -r FOLDER; do
  [ -n "$FOLDER" ] || continue
  is_protected "$FOLDER" && continue
  discover "$FOLDER" "$(encode_segment "$FOLDER")" 1
done < <(echo "$TOP" | jq -r '.[] | select(.id == null) | .name')

if [ "$MODE" = "list" ]; then
  printf '%s' "$BRANCH_KEYS"
  exit 0
fi

# Never upload anything that is not a JSON object. This inverts the failure
# mode that caused the 2026-07 outage: worst case is now a red CI job with the
# previous registry left intact, not a silently poisoned bucket.
echo "$REGISTRY" | jq -e 'type == "object"' > /dev/null
echo "$REGISTRY" | jq . > ./registry.json
[ -s ./registry.json ]

# x-upsert replaces the object in one call. No delete-then-upload window, so a
# failure anywhere in this script leaves the previous registry untouched and
# readers never see a 404.
curl -sf -X POST "${BASE}/object/${BUCKET}/registry.json" \
  -H "Authorization: Bearer ${STORAGE_KEY}" \
  -H "apikey: ${STORAGE_KEY}" \
  -H "Content-Type: application/json" \
  -H "x-upsert: true" \
  -H "cache-control: no-cache" \
  --data-binary @./registry.json > /dev/null

echo "registry.json rebuilt for ${BUCKET}: $(jq -c 'keys' ./registry.json)"
