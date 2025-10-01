#!/usr/bin/env bash
set -euo pipefail

# This script finds markdown files not referenced in SUMMARY.md and inserts them
# into the appropriate section based on their directory structure.
# - Only scans non-dot directories
# - Preserves existing SUMMARY.md, writing changes in-place
# - Uses two spaces per depth level for bullets

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SUMMARY_FILE="$ROOT_DIR/SUMMARY.md"

if [[ ! -f "$SUMMARY_FILE" ]]; then
  echo "ERROR: SUMMARY.md not found at $SUMMARY_FILE" >&2
  exit 1
fi

# Read all existing paths from SUMMARY.md (anything inside <...>)
# Store in an associative array for O(1) lookups
declare -A LISTED
while IFS= read -r line; do
  # Extract content between < and >
  if [[ "$line" =~ \<(.*)\> ]]; then
    path="${BASH_REMATCH[1]}"
    # Normalize possible leading ./
    path="${path#./}"
    LISTED["$path"]=1
  fi
done < "$SUMMARY_FILE"

# Helper: generate a human-friendly title from a file path
# - Uses basename without extension
# - Replaces - and _ with spaces
# - Title-cases words
make_title() {
  local file_path="$1"
  local base file title
  base="$(basename "$file_path")"
  file="${base%.md}"
  # Special-case README.md -> use directory name if available
  if [[ "$base" == "README.md" ]]; then
    local parent
    parent="$(basename "$(dirname "$file_path")")"
    file="$parent"
  fi
  # Replace separators with spaces
  title="${file//-/ }"
  title="${title//_/ }"
  # Collapse multiple spaces
  title="$(echo "$title" | tr -s ' ')"
  # Title case words
  local out=()
  for w in $title; do
    if [[ ${#w} -le 3 ]]; then
      # Keep small words as-is but capitalize if all lowercase
      out+=("${w^}")
    else
      out+=("${w^}")
    fi
  done
  printf '%s' "${out[*]}"
}

# Helper: compute bullet indent based on depth (two spaces per level)
indent_for_path() {
  local rel="$1"
  local depth=0
  if [[ "$rel" == */* ]]; then
    # Count slashes as depth
    depth="$(awk -F'/' '{print NF-1}' <<< "$rel")"
  fi
  # Root files (no slash) -> depth 0
  printf '%*s' $((depth * 2)) ''
}

# Helper: find best insertion line number for a new path.
# Strategy:
#  1) Try the deepest existing ancestor directory prefix present in SUMMARY (by matching any line that links to a path starting with that prefix)
#  2) If none found, try top-level segment
#  3) Fallback to end of file
find_insertion_line() {
  local rel="$1"
  local search_prefixes=()

  # Build candidate prefixes from directory hierarchy, deepest first
  local dir="$(dirname "$rel")"
  if [[ "$dir" != "." ]]; then
    # Add progressively shorter prefixes
    while true; do
      search_prefixes+=("$dir/")
      [[ "$dir" == */* ]] || break
      dir="${dir%/*}"
    done
  fi

  # Always include top-level segment (if any)
  if [[ "$rel" == */* ]]; then
    search_prefixes+=("${rel%%/*}/")
  fi

  local last_match_line=""
  local prefix
  for prefix in "${search_prefixes[@]}"; do
    # Use grep -n to get the last line that has a link path beginning with prefix
    # The pattern looks for '(<prefix' literal inside angle brackets
    if last_match_line=$(grep -nE "\(<${prefix//\//\/}" "$SUMMARY_FILE" | tail -n1 | cut -d: -f1); then
      if [[ -n "$last_match_line" ]]; then
        printf '%s' "$last_match_line"
        return 0
      fi
    fi
  done
  # Fallback: append at end of file
  wc -l < "$SUMMARY_FILE"
}

# Collect missing markdown files
# - Skip dot directories using prunes
# - Skip SUMMARY.md itself
# - Use -print0 to safely handle spaces
missing_paths=()
while IFS= read -r -d '' f; do
  rel="${f#$ROOT_DIR/}"
  # Normalize leading ./ if any
  rel="${rel#./}"
  # Skip SUMMARY.md
  if [[ "$rel" == "SUMMARY.md" ]]; then
    continue
  fi
  # Exclude dot directories anywhere in path
  if [[ "$rel" == *"/."* ]]; then
    continue
  fi
  if [[ -z "${LISTED[$rel]:-}" ]]; then
    missing_paths+=("$rel")
  fi
done < <(find "$ROOT_DIR" \
  -type d -name ".*" -prune -o \
  -type f -name "*.md" -print0)

if (( ${#missing_paths[@]} == 0 )); then
  echo "No missing markdown files. SUMMARY.md is up to date."
  exit 0
fi

echo "Found ${#missing_paths[@]} missing markdown file(s). Updating SUMMARY.md..."

# Determine insertion operations: line number and entry text
# We'll compute insertion line numbers before modification and apply from highest to lowest
mapfile -t sorted_missing < <(printf '%s\n' "${missing_paths[@]}" | LC_ALL=C sort)

inserts_lines=()
inserts_texts=()

for rel in "${sorted_missing[@]}"; do
  title="$(make_title "$rel")"
  indent="$(indent_for_path "$rel")"
  entry_line="${indent}* [${title}](<${rel}>)"
  line_no="$(find_insertion_line "$rel")"
  # Insert after the found line number (append in that section)
  if [[ -z "$line_no" ]]; then
    line_no="$(wc -l < "$SUMMARY_FILE")"
  fi
  inserts_lines+=("$line_no")
  inserts_texts+=("$entry_line")
  echo "  + $rel -> line $((line_no+1))"
done

# Apply insertions in descending order of line number to avoid shifting subsequent targets
# Build indices sorted by line number desc
idxs=()
for i in "${!inserts_lines[@]}"; do
  idxs+=("$i")
done

# Sort indices by corresponding line number desc
mapfile -t idxs < <(for i in "${idxs[@]}"; do echo "$i ${inserts_lines[$i]}"; done | sort -k2,2nr | awk '{print $1}')

# Create a temp file to write updates incrementally
TMP_FILE="$(mktemp)"
cp "$SUMMARY_FILE" "$TMP_FILE"

for i in "${idxs[@]}"; do
  ln_no="${inserts_lines[$i]}"
  txt="${inserts_texts[$i]}"
  # sed: append text after ln_no
  printf '%s\n' "$txt" | sed -i '' -e "$((ln_no))r /dev/stdin" "$TMP_FILE"
done

# Move temp over original
mv "$TMP_FILE" "$SUMMARY_FILE"

echo "SUMMARY.md updated."
