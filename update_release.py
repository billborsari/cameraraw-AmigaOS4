import os
import shutil
import hashlib
import re
import subprocess

work_dir = os.path.dirname(os.path.abspath(__file__))
release_dir = os.path.join(work_dir, "release/CameraRawLib_54.1")

os.makedirs(os.path.join(release_dir, "libs"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "A1222/libs"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "C"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "A1222/C"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "Tools"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "include/libraries"), exist_ok=True)
os.makedirs(os.path.join(release_dir, "include/xml"), exist_ok=True)

# 1. Copy Shared Libraries
lib_std = os.path.join(work_dir, "cameraraw.library/cameraraw.library")
lib_a1222 = os.path.join(work_dir, "cameraraw.library/cameraraw.library.a1222")

if os.path.exists(lib_std):
    shutil.copy2(lib_std, os.path.join(release_dir, "libs/cameraraw.library"))
    print("Copied standard cameraraw.library -> libs/cameraraw.library")

if os.path.exists(lib_a1222):
    shutil.copy2(lib_a1222, os.path.join(release_dir, "A1222/libs/cameraraw.library"))
    print("Copied A1222 cameraraw.library -> A1222/libs/cameraraw.library")

# 2. Copy CLI Tools (Standard & A1222)
tools = ["dcraw_emu", "raw-identify", "simple_dcraw", "test-library"]

for t in tools:
    src_std = os.path.join(work_dir, "bin", t)
    dst_std = os.path.join(release_dir, "C", t)
    if os.path.exists(src_std):
        shutil.copy2(src_std, dst_std)
        print("Copied %s -> C/%s" % (t, t))

    src_a1222 = os.path.join(work_dir, "bin", t + ".a1222")
    dst_a1222 = os.path.join(release_dir, "A1222/C", t)
    if os.path.exists(src_a1222):
        shutil.copy2(src_a1222, dst_a1222)
        print("Copied %s.a1222 -> A1222/C/%s" % (t, t))

# 3. Copy Hardware Detection Tool
detect_tool = os.path.join(work_dir, "tools/detect_a1222")
if not os.path.exists(detect_tool):
    detect_src = os.path.join(work_dir, "tools/detect_a1222.c")
    if os.path.exists(detect_src):
        try:
            subprocess.call(["ppc-amigaos-gcc", "-O2", "-mcrt=newlib", detect_src, "-o", detect_tool])
        except Exception:
            pass
if not os.path.exists(detect_tool):
    fallback = "/opt/code/AmigaOS4-datatypes/cameraraw.datatype/detect_a1222"
    if os.path.exists(fallback):
        detect_tool = fallback
if os.path.exists(detect_tool):
    shutil.copy2(detect_tool, os.path.join(release_dir, "Tools/detect_a1222"))
    print("Copied detect_a1222 -> Tools/detect_a1222")

# 4. Copy Developer Headers and XML
hdr_src = os.path.join(work_dir, "cameraraw.library/cameraraw.h")
xml_src = os.path.join(work_dir, "cameraraw.library/cameraraw.xml")
if os.path.exists(hdr_src):
    shutil.copy2(hdr_src, os.path.join(release_dir, "include/libraries/cameraraw.h"))
    print("Copied cameraraw.h -> include/libraries/cameraraw.h")
if os.path.exists(xml_src):
    shutil.copy2(xml_src, os.path.join(release_dir, "include/xml/cameraraw.xml"))
    print("Copied cameraraw.xml -> include/xml/cameraraw.xml")

# 5. Copy Icons
icon_templates = [
    ("/opt/code/AmigaOS4-datatypes/cameraraw.datatype/release/Cameraraw_dt_54.2.info",
     os.path.join(work_dir, "release/CameraRawLib_54.1.info")),
    ("/opt/code/AmigaOS4-datatypes/cameraraw.datatype/release/Cameraraw_dt_54.2/install cameraraw datatype.info",
     os.path.join(release_dir, "Install cameraraw.library.info")),
    ("/opt/code/AmigaOS4-datatypes/cameraraw.datatype/release/Cameraraw_dt_54.2/Tools.info",
     os.path.join(release_dir, "Tools.info"))
]

for src_ic, dst_ic in icon_templates:
    if os.path.exists(src_ic) and not os.path.exists(dst_ic):
        shutil.copy2(src_ic, dst_ic)
        print("Copied icon: %s -> %s" % (os.path.basename(src_ic), os.path.basename(dst_ic)))

# 6. Calculate MD5 Checksums
def get_md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(4096)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()

md5_lib_std = get_md5(os.path.join(release_dir, "libs/cameraraw.library"))
md5_lib_a1222 = get_md5(os.path.join(release_dir, "A1222/libs/cameraraw.library"))

md5_tools_std = {t: get_md5(os.path.join(release_dir, "C", t)) for t in tools}
md5_tools_a1222 = {t: get_md5(os.path.join(release_dir, "A1222/C", t)) for t in tools}

print("Calculated MD5 Hashes:")
print("  Standard Library: %s" % md5_lib_std)
print("  A1222 Library   : %s" % md5_lib_a1222)
for t in tools:
    print("  Standard %-14s: %s" % (t, md5_tools_std[t]))
    print("  A1222    %-14s: %s" % (t, md5_tools_a1222[t]))

# 7. Update Installer Script with Checksums
installer_path = os.path.join(release_dir, "Install cameraraw.library")
with open(installer_path, "r") as f:
    installer_code = f.read()

dict_std = """    expected_md5_std = {
        my_join(final_lib_dir, "cameraraw.library"): "%s",
        my_join(final_cmd_dir, "dcraw_emu"): "%s",
        my_join(final_cmd_dir, "raw-identify"): "%s",
        my_join(final_cmd_dir, "simple_dcraw"): "%s",
        my_join(final_cmd_dir, "test-library"): "%s",
    }""" % (md5_lib_std, md5_tools_std["dcraw_emu"], md5_tools_std["raw-identify"],
            md5_tools_std["simple_dcraw"], md5_tools_std["test-library"])

dict_a1222 = """    expected_md5_a1222 = {
        my_join(final_lib_dir, "cameraraw.library"): "%s",
        my_join(final_cmd_dir, "dcraw_emu"): "%s",
        my_join(final_cmd_dir, "raw-identify"): "%s",
        my_join(final_cmd_dir, "simple_dcraw"): "%s",
        my_join(final_cmd_dir, "test-library"): "%s",
    }""" % (md5_lib_a1222, md5_tools_a1222["dcraw_emu"], md5_tools_a1222["raw-identify"],
            md5_tools_a1222["simple_dcraw"], md5_tools_a1222["test-library"])

installer_code = re.sub(r'    expected_md5_std = \{[^}]*\}', dict_std, installer_code)
installer_code = re.sub(r'    expected_md5_a1222 = \{[^}]*\}', dict_a1222, installer_code)

with open(installer_path, "w") as f:
    f.write(installer_code)
print("Updated 'Install cameraraw.library' with verified MD5 checksums.")

# 8. Create LHA Archive
print("Creating release LHA archive...")
pycache_dir = os.path.join(release_dir, "__pycache__")
if os.path.exists(pycache_dir):
    shutil.rmtree(pycache_dir)

lha_file = os.path.join(work_dir, "release/CameraRawLib_54.1.lha")
if os.path.exists(lha_file):
    os.remove(lha_file)

cmd = ["lha", "a", "-x0", "CameraRawLib_54.1.lha", "CameraRawLib_54.1", "CameraRawLib_54.1.info"]
res = subprocess.call(cmd, cwd=os.path.join(work_dir, "release"))
if res == 0:
    print("SUCCESS: CameraRawLib_54.1.lha created successfully!")
else:
    print("ERROR: lha exited with code %d" % res)
