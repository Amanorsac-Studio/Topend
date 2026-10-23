#!/bin/bash
# Uninstall Top End.command - removes everything the Top End installer placed.
# Your own presets, songs and stems in ~/Documents/Amanorsac Studio/Top End are
# NOT touched. Delete that folder yourself if you want them gone too.
echo "This removes Top End from this Mac. Your presets, songs and stems are kept."
read -r -p "Remove Top End? (y/n) " answer
[ "$answer" = "y" ] || { echo "Nothing was removed."; exit 0; }
sudo rm -rf "/Applications/Top End.app" \
            "/Library/Audio/Plug-Ins/VST3/Top End.vst3" \
            "/Library/Audio/Plug-Ins/Components/Top End.component" \
            "/Library/Application Support/Amanorsac Studio/Top End"
for id in app vst3 au model docs; do sudo pkgutil --forget "com.amanorsac.topend.$id" >/dev/null 2>&1; done
rm -rf "$HOME/Library/Application Support/Amanorsac Studio/Top End"
echo "Top End has been removed. Your own files are still in ~/Documents/Amanorsac Studio/Top End."
