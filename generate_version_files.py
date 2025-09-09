# generate_version_header.py
# import sys
import json

# Get the project name from command-line
# project_name = sys.argv[1] if len(sys.argv) > 1 else "UnknownProject"

with open("version_latest.json", "r") as f:
    version = json.load(f)

major = version["major"]
minor = version["minor"]
patch = version["patch"]
build = version["build"]
version["build"] += 1

with open("version_latest.json", "w") as f:
    version = json.dump(version, f, indent=2)

with open("version.h", "w") as f:
    f.write(f"#define VER_MAJOR {major}\n")
    f.write(f"#define VER_MINOR {minor}\n")
    f.write(f"#define VER_PATCH {patch}\n")
    f.write(f"#define VER_BUILD {build}\n")
    f.write(f"#define VER_FILEVERSION {major},{minor},{patch},{build}\n")
    f.write(f"#define VER_FILEVERSION_STR \"{major}.{minor}.{patch}\\0\"\n")

with open("version_minimum.json", "r") as f:
    version = json.load(f)

mnm_major = version["major"]
mnm_minor = version["minor"]
mnm_patch = version["patch"]
mnm_build = version["build"]


with open("version.txt", "w") as f:
    f.write(f"{major},{minor},{patch},{build}\n")
    f.write(f"{mnm_major},{mnm_minor},{mnm_patch},{mnm_build}\n")
