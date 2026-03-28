#!/usr/bin/env bash
set -euo pipefail

CURSO_DIR="$(cd "$(dirname "$0")" && pwd)/CURSO"

# Strip accents/diacritics from a string
strip_accents() {
    echo "$1" | sed \
        -e 'y/áéíóúÁÉÍÓÚ/aeiouAEIOU/' \
        -e 'y/àèìòùÀÈÌÒÙ/aeiouAEIOU/' \
        -e 'y/äëïöüÄËÏÖÜ/aeiouAEIOU/' \
        -e 'y/âêîôûÂÊÎÔÛ/aeiouAEIOU/' \
        -e 'y/ñÑ/nN/'
}

# Strip leading zeros from a number: 01 -> 1, 10 -> 10
strip_zero() {
    echo "$1" | sed 's/^0*//' | sed 's/^$/0/'
}

for block_dir in "$CURSO_DIR"/B[0-9]*; do
    [ -d "$block_dir" ] || continue

    for chapter_dir in "$block_dir"/C[0-9]*; do
        [ -d "$chapter_dir" ] || continue

        chapter_num=$(basename "$chapter_dir" | grep -oP '^C\K[0-9]+')
        readmes_dir="$chapter_dir/READMES"

        # Track if this chapter has any topics at all
        has_topics=false

        for section_dir in "$chapter_dir"/S[0-9]*; do
            [ -d "$section_dir" ] || continue

            section_num=$(basename "$section_dir" | grep -oP '^S\K[0-9]+')

            for topic_dir in "$section_dir"/T[0-9]*; do
                [ -d "$topic_dir" ] || continue
                has_topics=true

                topic_basename=$(basename "$topic_dir")
                topic_num=$(echo "$topic_basename" | grep -oP '^T\K[0-9]+')
                # Topic name is everything after T##_
                topic_name=$(echo "$topic_basename" | sed 's/^T[0-9]*_//')

                cn=$(strip_zero "$chapter_num")
                sn=$(strip_zero "$section_num")
                tn=$(strip_zero "$topic_num")
                clean_name=$(strip_accents "$topic_name")

                dest_file="C${cn}_S${sn}_T${tn}_${clean_name}.md"
                dest_path="$readmes_dir/$dest_file"

                # Skip if already exists in READMES
                if [ -f "$dest_path" ]; then
                    continue
                fi

                # Pick source: prefer .claude.md, fallback to README.md
                if [ -f "$topic_dir/README.claude.md" ]; then
                    src="$topic_dir/README.claude.md"
                elif [ -f "$topic_dir/README.md" ]; then
                    src="$topic_dir/README.md"
                else
                    echo "WARN: no README found in $topic_dir" >&2
                    continue
                fi

                # Create READMES dir on first actual copy for this chapter
                mkdir -p "$readmes_dir"
                cp "$src" "$dest_path"
                echo "Copied: $dest_file"
            done
        done

        if [ "$has_topics" = false ]; then
            continue
        fi
    done
done

echo "Done."
