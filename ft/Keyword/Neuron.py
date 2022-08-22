from os import remove
import os
import random
import subprocess
import shutil
import time


def prepare_persistence_dir():
    os.system("mkdir -p build/persistence")


class Neuron(object):
    process = 0
    profiler_process = None

    def __init__(self):
        pass

    def Start_Neuron(self):
        prepare_persistence_dir()
        self.process = subprocess.Popen(['./neuron', '--log'], cwd='build/')
        return self.process

    def Stop_Neuron(self, remove_persistence_data=True):
        if self.profiler_process is not None:
            # wait for plotting
            time.sleep(5)
        self.process.kill()
        if remove_persistence_data:
            os.system("rm build/persistence/sqlite.db")
            prepare_persistence_dir()

    def End_Neuron(self, process):
        process.kill()

    def Remove_Persistence(self):
        os.system("rm build/persistence/sqlite.db")
        prepare_persistence_dir()

    def Profile_Neuron(self, interval, result_dir):
        cmd = [
            'psrecord', str(self.process.pid),
            '--interval', str(interval),
            '--plot', result_dir + '/cpu_mem.svg'
        ]

        self.profiler_process = subprocess.Popen(cmd)


class Tool(object):

    def __init__(self):
        pass

    def Array_Len(self, array):
        return len(array)


class Plugin(object):

    def __init__(self):
        pass

    def Get_Plugin_By_Name(self, splugins, name):
        for plugin in splugins:
            if plugin['name'] == name:
                return plugin['id']
        return 0

    def Plugin_With_Name_Sholud_Exist(self, splugins, name):
        assert 0 != self.Get_Plugin_By_Name(splugins, name)

    def Plugin_With_Name_Should_Not_Exist(self, splugins, name):
        assert 0 == self.Get_Plugin_By_Name(splugins, name)


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

    def Get_Random_Group_Config(self, gconfigs):
        return random.choice(gconfigs)['name']

    def Group_Config_Check(self, gconfigs, name, interval):
        ret = -1
        for gconfig in gconfigs:
            if gconfig['name'] == name:
                if gconfig['interval'] == int(interval):
                    ret = 0
                break
        return ret

    def Get_Group_Config_By_Name(self, sgconfigs, name):
        for gconfig in sgconfigs:
            if gconfig['name'] == name:
                return gconfig['name']
        return 0


class Node(object):

    def __init__(self):
        pass

    def Get_Node_By_Name(self, snodes, name):
        for node in snodes:
            if node['name'] == name:
                return True
        return False

    def Node_Should_Exist(self, snodes, name):
        assert True == self.Get_Node_By_Name(snodes, name)

    def Node_Should_Not_Exist(self, snodes, name):
        assert False == self.Get_Node_By_Name(snodes, name)

    def Node_With_Name_Should_Not_Exist(self, snodes, name):
        assert 0 == self.Get_Node_By_Name(snodes, name)

    def Get_Random_Node(self, snodes):
        return random.choice(snodes)['id']


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
                return tag

        return None


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

    def Compare_Tag_Value_Bool(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if isinstance(value, str):
                    if (tag['value'] == False and value.lower() == "false" or
                            tag['value'] == True and value.lower() == "true"):
                        ret = 0
                elif tag['value'] == int(value):
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_Int(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if int(tag['value']) == int(value):
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_Float(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if float(tag['value']) <= float(value) + 0.1 and float(tag['value']) >= float(value) - 0.1:
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_String(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if tag['value'] == str(value):
                    ret = 0
                break
        return ret

    def Compare_Tag_Value_Strings(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if "\"" + tag['value'] + "\"" == str(value):
                    ret = 0
                break
        return ret

    def Check_Tag_Error(self, tags, name, value):
        ret = -1
        for tag in tags:
            if tag['name'] == name:
                if int(tag['error']) == int(value):
                    ret = 0
                break
        return ret
