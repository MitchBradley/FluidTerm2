# pio_use_clang.py
Import("env")

print("\nReplacing the default toolchain with Clang...\n")

# Replace the C and C++ compilers with clang and clang++
env.Replace(
    CC="clang",
    CXX="clang++",
    LINK="clang++"
)

# Add framework flags for macOS
env.Append(LINKFLAGS=["-framework", "CoreFoundation", "-framework", "IOKit"])

print(f"New CC: {env.get('CC')}")
print(f"New CXX: {env.get('CXX')}")
print(f"New Linker: {env.get('LINK')}")
print(f"LINKFLAGS: {env.get('LINKFLAGS')}")

