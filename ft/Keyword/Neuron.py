import subprocess


class Neuron(object):
    process = 0

    def __init__(self):
        pass

    def Start_Neuron(self):
        self.process = subprocess.Popen(['./neuron'], cwd='build/')

    def Stop_Neuron(self):
        self.process.kill()


class Tool(object):

    def __init__(self):
        pass

    def Array_Len(self, array):
        return len(array)


class GrpConfig(object):

    def __init__(self):
        pass

    def Get_Interval_In_Group_Config(self, gconfigs, gname):
        for gconfig in gconfigs[0]:
            if gconfig['name'] == gname:
                return gconfig['interval']
        return 0

    def Group_Config_Size(self, gconfigs):
        size = 0
        for gconfig in gconfigs[0]:
            size = size + 1
        return size


class Node(object):

    def __init__(self):
        pass

    def Get_Node_By_Name(self, snodes, name):
        for node in snodes[0]:
            if node['name'] == name:
                return node['id']
        return 0


class Tag(object):

    def __init__(self):
        pass

    def Tag_Check(self, tags, name, type, group_config_name, attribute, address):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if tag['type'] == int(type) and tag['group_config_name'] == group_config_name and tag['attribute'] == int(attribute) and tag['address'] == address:
                    ret = 0
                break
        return ret

    def Tag_Find_By_Name(self, tags, name):
        for tag in tags:
            if tag['name'] == name:
                return tag['id']

        return -1


class Subscribe(object):

    def __init__(self):
        pass

    def Subscribe_Check(self, groups, node_id, group_config_name):
        ret = -1
        for group in groups:
            if group['node_id'] == int(node_id) and group['group_config_name'] == group_config_name:
                ret = 0
                break
        return ret
