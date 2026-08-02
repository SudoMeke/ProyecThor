#!/bin/sh
cd "/usr/local/lib/proyecthor" || exit 1
export VLC_PLUGIN_PATH="/usr/local/lib/proyecthor/plugins"
exec ./ProyecThor "$@"
