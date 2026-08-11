#!/bin/sh

#  Orca-Flashforge gettext
#  Created by SoftFever on 27/5/23.
#

list_file="./localization/i18n/list.txt"
pot_file="./localization/i18n/OrcaSlicer.pot"
filtered_list=""
missing_list=""
generated_root_dir=""

report_missing_files()
{
    if [ -n "$missing_list" ] && [ -s "$missing_list" ]; then
        echo
        echo "Skipped missing source files listed in ${list_file}:"
        while IFS= read -r missing || [ -n "$missing" ]; do
            echo "  - $missing"
        done < "$missing_list"
    fi
}

cleanup_temp_files()
{
    [ -n "$filtered_list" ] && rm -f "$filtered_list"
    [ -n "$missing_list" ] && rm -f "$missing_list"
    [ -n "$generated_root_dir" ] && rm -rf "$generated_root_dir"
}

files_equal_ignoring_pot_date()
{
    file_a=$1
    file_b=$2
    norm_a=$(mktemp)
    norm_b=$(mktemp)

    sed '/^"POT-Creation-Date: /d' "$file_a" > "$norm_a"
    sed '/^"POT-Creation-Date: /d' "$file_b" > "$norm_b"

    if cmp -s "$norm_a" "$norm_b"; then
        rm -f "$norm_a" "$norm_b"
        return 0
    fi

    rm -f "$norm_a" "$norm_b"
    return 1
}

trap 'report_missing_files; cleanup_temp_files' EXIT

# Check for --full argument
FULL_MODE=false
for arg in "$@"
do
    if [ "$arg" = "--full" ]; then
        FULL_MODE=true
    fi
done

if $FULL_MODE; then
    xgettext --keyword=L --keyword=_L --keyword=_u8L --keyword=L_CONTEXT:1,2c --keyword=_L_PLURAL:1,2 --add-comments=TRN --from-code=UTF-8 --no-location --debug --boost -f ./localization/i18n/list.txt -o ./localization/i18n/Orca-Flashforge.pot
    python3 scripts/HintsToPot.py ./resources ./localization/i18n
fi


echo "$0: working dir = $PWD"
pot_file="./localization/i18n/Orca-Flashforge.pot"
for dir in ./localization/i18n/*/
do
    dir=${dir%*/}      # remove the trailing "/"
    lang=${dir##*/}    # extract the language identifier

    if [ -f "$dir/Orca-Flashforge_${lang}.po" ]; then
        if $FULL_MODE; then
            msgmerge -N -o "$dir/Orca-Flashforge_${lang}.po" "$dir/Orca-Flashforge_${lang}.po" "$pot_file"
        fi
        mkdir -p "resources/i18n/${lang}"
        if ! msgfmt --check-format -o "resources/i18n/${lang}/OrcaSlicer.mo" "$dir/Orca-Flashforge_${lang}.po"; then
            echo "Error encountered with msgfmt command for language ${lang}."
            exit 1  # Exit the script with an error status
        fi
    fi
done
