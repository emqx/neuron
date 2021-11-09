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
