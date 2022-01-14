import subprocess
import shutil


class Neuron(object):
    process = 0

    def __init__(self):
        pass

    def Start_Neuron(self):
        self.process = subprocess.Popen(['./neuron'], cwd='build/')

    def Stop_Neuron(self, remove_persistence_data=True):
        self.process.kill()
        if remove_persistence_data:
            shutil.rmtree("build/persistence", ignore_errors=True)


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
        for node in snodes:
            if node['name'] == name:
                return node['id']
        return 0

    def Node_With_Name_Should_Exist(self, snodes, name):
        assert 0 != self.Get_Node_By_Name(snodes, name)


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


class Read(object):

    def __init__(self):
        pass

    def Compare_Tag_Value_Bool(self, tags, id, value):
        ret = -1
        for tag in tags:
            if tag['id'] == int(id):
                if isinstance(value, str):
                    if (tag['value'] == False and value.lower() == "false" or
                        tag['value'] == True and value.lower() == "true"):
                        ret = 0
                elif tag['value'] == int(value):
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_Int(self, tags, id, value):
        ret = -1
        for tag in tags:
            if tag['id'] == int(id):
                if tag['value'] == int(value):
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_Float(self, tags, id, value):
        ret = -1
        for tag in tags:
            if tag['id'] == int(id):
                if tag['value'] <= float(value) + 0.1 and tag['value'] >= float(value) - 0.1:
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_String(self, tags, id, value):
        ret = -1
        for tag in tags:
            if tag['id'] == int(id):
                if tag['value'] == str(value):
                    ret = 0
                break
        return ret
