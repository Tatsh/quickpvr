#!/usr/bin/env bash
# Remove CodeQL results in the build directory, where FetchContent puts the sources of argparse,
# spdlog, and stb. The only argument is the directory of SARIF files.
set -euo pipefail
shopt -s nullglob
files=("$1"/*.sarif)
if [[ ${#files[@]} -eq 0 ]]; then
  echo "::error::No SARIF files in $1."
  exit 1
fi
for file in "${files[@]}"; do
  before=$(jq '[.runs[].results[]] | length' "$file")
  jq '.runs |= map(.results |= map(select(
        ((.locations // [])[0].physicalLocation.artifactLocation.uri // "")
        | test("^build/") | not)))' "$file" > "$file.filtered"
  mv "$file.filtered" "$file"
  after=$(jq '[.runs[].results[]] | length' "$file")
  echo "${file}: kept ${after} of ${before} results."
done
