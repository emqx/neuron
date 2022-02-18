#!/bin/sh

outdir=${outc_dir:-"../../src/persist/json"}

json-autotype --clang --no-autounify --debug -t json_node ./json_adapter.json -o persist_json_adapter.c
json-autotype --clang --no-autounify --debug -t json_datatags ./json_datatags.json -o persist_json_datatags.c
json-autotype --clang --no-autounify --debug -t json_group_configs ./json_group_configs.json -o persist_json_group_configs.c
json-autotype --clang --no-autounify --debug -t json_plugin ./json_plugin.json -o persist_json_plugin.c
json-autotype --clang --no-autounify --debug -t json_subscriptions ./json_subs.json -o persist_json_subs.c

find -iname \*.c |xargs sed -i 's/json\/persist/persist/g'
mv *.c *.h ${outdir}/
