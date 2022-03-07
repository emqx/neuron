#!/usr/bin/python3

import os
import stat
import shutil


class FileMap:
    def __init__(self, s, d, mode='r', f=""):
        self.src = s
        self.dst = d
        self.mode = mode
        if f == "":
            self.filename = os.path.basename(s)
        else:
            self.filename = f


fix_dri = 'tmp'
deb_dir = fix_dri + "/DEBIAN"


def create_script(path):
    with open(path, mode='x') as f:
        f.writelines("#!/usr/bin/env bash")
        f.close()

        os.chmod(path, stat.S_IXUSR | stat.S_IWUSR | stat.S_IRUSR |
                 stat.S_IROTH | stat.S_IXOTH | stat.S_IRGRP | stat.S_IXGRP)
    return path


def create_template():
    os.mkdir(fix_dri)
    os.mkdir(deb_dir)
    script = ["preinst", "postinst", "prerm", "postrm"]
    path = map(lambda x: deb_dir + "/" + x, script)
    for p in path:
        print(create_script(p))


def create_control(package, version, arch, description, depends):
    with open(deb_dir + "/" + "control", mode='x') as f:
        f.writelines("Package: " + package)
        f.write("\n")
        f.writelines("Version: " + version)
        f.write("\n")
        f.writelines("Priority: optional")
        f.write("\n")
        f.writelines("Architecture: " + arch)
        f.write("\n")
        f.writelines("Maintainer: support <joey@emqx.io>")
        f.write("\n")
        f.writelines("Description: " + description)
        f.write("\n")
        f.writelines("Section: utils")
        f.write("\n")
        if len(depends) != 0:
            f.writelines("Depends: " + depends)
            f.write("\n")
        f.writelines("Installed-Size: " + str(cal_size(fix_dri)))
        f.write("\n")
        f.close()


def cal_size(path):
    size = 0
    for root, dirs, files in os.walk(path):
        size += sum([os.path.getsize(os.path.join(root, name))
                     for name in files])
    return size


def create_deb_file(rules):
    for r in rules:
        src = r.src
        dstpath = fix_dri + r.dst
        dst = dstpath + r.filename
        if not os.path.isfile(src):
            print("%s not exist!" % src)
            continue

        if not os.path.exists(dstpath):
            os.makedirs(dstpath)

        shutil.copyfile(src, dst)
        if r.mode == "x":
            os.chmod(dst,
                     stat.S_IXUSR | stat.S_IRUSR | stat.S_IWUSR | stat.S_IXGRP | stat.S_IWGRP | stat.S_IRGRP)
        print("copy: %s => %s" % (src, dst))


if __name__ == "mkdeb":
    if os.path.exists(fix_dri):
        shutil.rmtree(fix_dri)
    create_template()
