import mkdeb
import os
import sys

rules = []

rules.append(mkdeb.FileMap("deb/neuron/conffiles",
             "/DEBIAN/", "r", "conffiles"))
rules.append(mkdeb.FileMap("deb/neuron/postinst", "/DEBIAN/", "r", "postinst"))
rules.append(mkdeb.FileMap("deb/neuron/preinst", "/DEBIAN/", "r", "preinst"))
rules.append(mkdeb.FileMap("deb/neuron/prerm", "/DEBIAN/", "r", "prerm"))

rules.append(mkdeb.FileMap("neuron.service", "/lib/systemd/system/"))

rules.append(mkdeb.FileMap("neuron.sh", "/opt/neuron/", "x"))
rules.append(mkdeb.FileMap("../build/neuron", "/opt/neuron/", "x"))

rules.append(mkdeb.FileMap("../zlog.conf", "/opt/neuron/config/"))
rules.append(mkdeb.FileMap("../zlog/src/libzlog.so.1.2", "/usr/local/lib/"))

rules.append(mkdeb.FileMap("../.gitkeep", "/opt/neuron/logs/"))
rules.append(mkdeb.FileMap("../.gitkeep", "/opt/neuron/core/"))

rules.append(mkdeb.FileMap("../neuron.key", "/opt/neuron/config/"))
rules.append(mkdeb.FileMap("../neuron.pem", "/opt/neuron/config/"))
rules.append(mkdeb.FileMap("../neuron.yaml", "/opt/neuron/config/"))

rules.append(mkdeb.FileMap("../default_plugins.json",
             "/opt/neuron/persistence/", "r", "plugins.json"))

rules.append(mkdeb.FileMap("../build/libneuron-base.so", "/usr/local/lib/"))

rules.append(mkdeb.FileMap(
    "../build/plugins/libplugin-ekuiper.so", "/opt/neuron/plugins/"))
rules.append(mkdeb.FileMap(
    "../build/plugins/libplugin-mqtt.so", "/opt/neuron/plugins/"))
rules.append(mkdeb.FileMap(
    "../build/plugins/libplugin-modbus-tcp.so", "/opt/neuron/plugins/"))

rules.append(mkdeb.FileMap(
    "../plugins/ekuiper/ekuiper.json", "/opt/neuron/plugins/schema/"))
rules.append(mkdeb.FileMap(
    "../plugins/mqtt/mqtt.json", "/opt/neuron/plugins/schema/"))
rules.append(mkdeb.FileMap(
    "../plugins/modbus/modbus-tcp.json", "/opt/neuron/plugins/schema/"))


mkdeb.create_deb_file(rules)
mkdeb.create_control("neuron", sys.argv[1], sys.argv[2], "neuron", "")

cmd = 'dpkg-deb -b tmp/ ' + 'neuron' + '-' + \
    sys.argv[1] + '-'+sys.argv[2] + ".deb"
os.system(cmd)
