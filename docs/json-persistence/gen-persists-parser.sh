#!/bin/sh

outdir=${outc_dir:-"."}

json-autotype --clang --no-autounify --debug -t json_node ./json_adapter.json -o json_adapter.c
json-autotype --clang --no-autounify --debug -t json_datatags ./json_datatags.json -o json_datatags.c
json-autotype --clang --no-autounify --debug -t json_group_configs ./json_group_configs.json -o json_group_configs.c
json-autotype --clang --no-autounify --debug -t json_plugin ./json_plugin.json -o json_plugin.c
json-autotype --clang --no-autounify --debug -t json_subscriptions ./json_subs.json -o json_subs.c

mv *.c *.h ${outdir}/
