import requests
import neuron.config as config


def login(user='admin', password='0000'):
    return requests.post(url=config.BASE_URL + '/api/v2/login', json={"name": user, "pass": password})


def change_password(new_password, user='admin', old_password='0000', jwt=config.default_jwt):
    return requests.post(url=config.BASE_URL + '/api/v2/password', headers={"Authorization": jwt}, json={"name": user, "old_pass": old_password, "new_pass": new_password})


def get_license(jwt=config.default_jwt):
    return requests.get(url=config.BASE_URL + '/api/v2/license', headers={"Authorization": jwt})


def upload_license(license, jwt=config.default_jwt):
    return requests.post(url=config.BASE_URL + '/api/v2/license', headers={"Authorization": jwt}, json={"license": license})


def change_log_level(node, level, jwt=config.default_jwt):
    return requests.put(url=config.BASE_URL + '/api/v2/log/level', headers={"Authorization": jwt}, json={"node": node, "level": level})


def add_node(node, plugin, jwt=config.default_jwt):
    return requests.post(url=config.BASE_URL + '/api/v2/node', headers={"Authorization": jwt}, json={"name": node, "plugin": plugin})


def del_node(node, jwt=config.default_jwt):
    return requests.delete(url=config.BASE_URL + '/api/v2/node', headers={"Authorization": jwt}, json={"name": node})


def get_nodes(type, jwt=config.default_jwt):
    return requests.get(url=config.BASE_URL + '/api/v2/node', headers={"Authorization": jwt}, params={"type": type})


def node_ctl(node, ctl, jwt=config.default_jwt):
    return requests.post(url=config.BASE_URL + '/api/v2/node/ctl', headers={"Authorization": jwt}, json={"node": node, "cmd": ctl})


def get_nodes_state(node, jwt=config.default_jwt):
    return requests.get(url=config.BASE_URL + '/api/v2/node/state', headers={"Authorization": jwt}, params={"node": node})


def node_setting(node, json, jwt=config.default_jwt):
    return requests.post(url=config.BASE_URL + '/api/v2/node/setting', headers={"Authorization": jwt}, json={"node": node, "params": json})


def get_node_setting(node, jwt=config.default_jwt):
    return requests.get(url=config.BASE_URL + '/api/v2/node/setting', headers={"Authorization": jwt}, params={"node": node})


# plugin setting


def modbus_tcp_node_setting(node, port, connection_mode=0, transport_mode=0, interval=0, host='127.0.0.1', timeout=3000):
    return node_setting(node, json={"connection_mode": connection_mode, "transport_mode": transport_mode, "interval": interval,
                                    "host": host, "port": port, "timeout": timeout})


def modbus_rtu_node_setting(node, port, connection_mode=0, transport_mode=0, interval=10, host='127.0.0.1', timeout=3000, link=1, device="", stop=0, parity=0, baud=4, data=3):
    return node_setting(node, json={"connection_mode": connection_mode, "transport_mode": transport_mode,
                                    "interval": interval, "host": host, "port": port, "timeout": timeout,
                                    "link": link, "device": device, "stop": stop, "parity": parity,
                                    "baud": baud, "data": data})
