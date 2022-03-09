import mkdeb
import os
import sys

rules = []

rules.append(mkdeb.FileMap("deb/neuron/conffiles",
             "/DEBIAN/", "r", "conffiles"))
rules.append(mkdeb.FileMap("deb/neuron/postinst", "/DEBIAN/", "r", "postinst"))
rules.append(mkdeb.FileMap("deb/neuron/preinst", "/DEBIAN/", "r", "preinst"))
rules.append(mkdeb.FileMap("deb/neuron/prerm", "/DEBIAN/", "r", "prerm"))

rules.append(mkdeb.FileMap("../build/neuron", "/opt/neuron/bin/", "x"))
rules.append(mkdeb.FileMap("../private_test.key", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap("../public_test.pem", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap("../build/libneuron-base.so", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap("../neuron.yaml", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap("../default_plugin.json",
             "/opt/neuron/bin/persistence/", "r", "plugin.json"))

rules.append(mkdeb.FileMap("../build/libplugin-mqtt.so", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap(
    "../plugins/mqtt/mqtt-plugin.json", "/opt/neuron/bin/schema/"))
rules.append(mkdeb.FileMap(
    "../build/libplugin-modbus-tcp.so", "/opt/neuron/bin/"))
rules.append(mkdeb.FileMap(
    "../plugins/modbus/modbus-tcp-plugin.json", "/opt/neuron/bin/"))


mkdeb.create_deb_file(rules)
mkdeb.create_control("neuron", sys.argv[1], sys.argv[2], "neuron", "")

cmd = 'dpkg-deb -b tmp/ ' + 'neuron' + '-' + \
    sys.argv[1] + '-'+sys.argv[2] + ".deb"
os.system(cmd)
