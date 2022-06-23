import subprocess

Import("env")

gitFail = False
try:
    subprocess.check_call(["git", "status"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
except:
    gitFail = True

if gitFail:
    tag = "v3.0.x"
    rev = " (noGit)"
else:
    try:
        tag = (
            subprocess.check_output(["git", "describe", "--tags", "--abbrev=0"], stderr=subprocess.DEVNULL)
            .strip()
            .decode("utf-8")
        )
    except:
        tag = "NoTag"

    # Check to see if the head commit exactly matches a tag.
    # If so, the revision is "release", otherwise it is BRANCH-COMMIT
    try:
        subprocess.check_call(["git", "describe", "--tags", "--exact"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rev = ''
    except:
        branchname = (
            subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"])
            .strip()
            .decode("utf-8")
        )
        revision = (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])
            .strip()
            .decode("utf-8")
        )
        modified = (
            subprocess.check_output(["git", "status", "-uno", "-s"])
            .strip()
            .decode("utf-8")
        )
        if modified:
            dirty = "-dirty"
        else:
            dirty = ""

        rev = " (%s%s)" % (revision, dirty)

git_info = '%s%s' % (tag, rev)
build_flag = "-DVERSION=\\\"\"" + git_info + "\\\"\""
print("Version ", build_flag)

def git_version():
    ret = subprocess.run(["git", "describe"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    #ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags
    build_version = ret.stdout.strip()
    build_flag = "-D VERSION=\\\"" + build_version + "\\\""
    print ("Version: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[build_flag]
)
