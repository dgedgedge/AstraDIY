#!/bin/bash
FILESOURCE=$(dirname $0)
TARGETDIR=~/.config/autostart/
echo "Begin Installing Auto-Start Astra Hmi"
mkdir -p $TARGETDIR
# Supprimer tous les anciens fichiers d'autostart Astra
rm -f $TARGETDIR/Astra*.desktop
# Installer uniquement AstraDIYHmi.desktop (qui contient tous les onglets)
cp ${FILESOURCE}/autostart/AstraDIYHmi.desktop $TARGETDIR

echo "End Installing Auto-Start Astra Hmi"

