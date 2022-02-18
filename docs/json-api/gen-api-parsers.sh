#!/bin/sh

source_dir=${outc_dir:-"../../src/parser"}
header_dir=${outh_dir:-"../../include/json"}

json-autotype --clang --no-autounify --debug -t json_error ./neu_json_error.json -o neu_json_error.c
json-autotype --clang --no-autounify --debug -t json_group_config ./neu_json_group_config.json -o neu_json_group_config.c
json-autotype --clang --no-autounify --debug -t json_node ./neu_json_node.json -o neu_json_node.c
json-autotype --clang --no-autounify --debug -t json_plugin ./neu_json_plugin.json -o neu_json_plugin.c
json-autotype --clang --no-autounify --debug -t json_rw ./neu_json_rw.json -o neu_json_rw.c
json-autotype --clang --no-autounify --debug -t json_tag ./neu_json_tag.json -o neu_json_tag.c
json-autotype --clang --no-autounify --debug -t json_tty ./neu_json_tty.json -o neu_json_tty.c

mv *.c ${source_dir}/
mv *.h ${header_dir}/
