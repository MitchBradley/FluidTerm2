# pio_use_clang.py
Import("env")

env.AppendUnique(FRAMEWORKS=Split('IOKit CoreFoundation'))

print("\nReplacing the default toolchain with Clang...\n")

# # Replace the C and C++ compilers with clang and clang++
env.Replace(
    CC="clang",
    CXX="clang++",
    LINK="clang++"
)

print(f"New CC: {env.get('CC')}")
print(f"New CXX: {env.get('CXX')}")
print(f"New Linker: {env.get('LINK')}")
