import mkdeb
import os
import sys

rules = []

rules.append(mkdeb.FileMap("deb/sdk/conffiles", "/DEBIAN/", "r", "conffiles"))
rules.append(mkdeb.FileMap("deb/sdk/postinst", "/DEBIAN/", "r", "postinst"))
rules.append(mkdeb.FileMap("deb/sdk/preinst", "/DEBIAN/", "r", "preinst"))
rules.append(mkdeb.FileMap("deb/sdk/prerm", "/DEBIAN/", "r", "prerm"))

rules.append(mkdeb.FileMap("../cmake/neuron-config.cmake",
             "/usr/local/lib/cmake/neuron/"))
rules.append(mkdeb.FileMap("../build/libneuron-base.so", "/usr/local/lib/"))

rules.append(mkdeb.FileMap("../include/connection/mqtt_client_intf.h",
             "/usr/local/include/neuron/connection/"))
rules.append(mkdeb.FileMap("../include/connection/neu_connection.h",
             "/usr/local/include/neuron/connection/"))
rules.append(mkdeb.FileMap("../include/connection/neu_tcp.h",
             "/usr/local/include/neuron/connection/"))
rules.append(mkdeb.FileMap("../include/event/event.h",
             "/usr/local/include/neuron/event/"))

rules.append(mkdeb.FileMap("../include/json/json.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_error.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_group_config.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_fn.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_log.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_login.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_logout.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_mqtt.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_node.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_param.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_plugin.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_rw.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_tag.h",
             "/usr/local/include/neuron/json/"))
rules.append(mkdeb.FileMap("../include/json/neu_json_tty.h",
             "/usr/local/include/neuron/json/"))

rules.append(mkdeb.FileMap("../include/hash_table.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/idhash.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_adapter.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_atomic_data.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_base64.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_data_expr.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_datatag_table.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_errcodes.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_file.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_license.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_list.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_log.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_panic.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_plugin_info.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_plugin.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_subscribe.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_tag_group_config.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_tag.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_types.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_uuid.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neu_vector.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/neuron.h",
             "/usr/local/include/neuron/"))
rules.append(mkdeb.FileMap("../include/vector.h",
             "/usr/local/include/neuron/"))

mkdeb.create_deb_file(rules)
mkdeb.create_control("neuron-sdk", sys.argv[1], sys.argv[2], "neuron-sdk", "")

cmd = 'dpkg-deb -b tmp/ ' + 'neuron-sdk' + '-' + \
    sys.argv[1] + '-'+sys.argv[2] + ".deb"
os.system(cmd)
