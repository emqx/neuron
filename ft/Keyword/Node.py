class Node(object):

    def __init__(self):
        pass

    def Get_Node_By_Name(self, snodes, name):
        for node in snodes[0]:
            if node['name'] == name:
                return node['id']
        return 0
