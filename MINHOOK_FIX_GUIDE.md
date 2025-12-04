# MinHook Integration Fix Guide

## Problem
```
[MinHookStub] WARNING: Using stub implementation - hooks will not work!
```

This means your DLL project is linking to a placeholder stub instead of the actual MinHook library.

## Solution

### Step 1: Download MinHook Source
1. Get MinHook from: https://github.com/TsudaKageyu/minhook
2. Extract to: `C:\Users\tai2l\source\repos\Mira Cleaner\minhook\`

### Step 2: Add MinHook Source Files to Your DLL Project

#### Required Files (x64):
```
minhook/src/buffer.c
minhook/src/hook.c
minhook/src/trampoline.c
minhook/src/hde/hde64.c        ? Use hde64.c for x64 builds
```

#### DO NOT INCLUDE (for x64):
```
minhook/src/hde/hde32.c        ? Only for x86 builds
```

### Step 3: Update Visual Studio Project

#### In Solution Explorer:
1. Right-click your DLL project (e.g., "Mira DLL")
2. **Add** ? **Existing Item...**
3. Navigate to `minhook\src\`
4. Add these files:
   - `buffer.c`
   - `hook.c`
   - `trampoline.c`
5. Navigate to `minhook\src\hde\`
6. Add `hde64.c` (for x64 builds)

#### Configure Properties:
1. Right-click your DLL project ? **Properties**
2. **C/C++** ? **General** ? **Additional Include Directories**
3. Add: `$(ProjectDir)minhook\include;%(AdditionalIncludeDirectories)`

### Step 4: Remove/Disable MinHookStub

If you have a `MinHookStub.cpp` file:
1. Right-click it in Solution Explorer
2. **Exclude From Project** (or delete it)

### Step 5: Verify Include Path

In your DLL source files, use:
```cpp
#include "MinHook.h"  // Not <MinHook.h>
```

### Step 6: Rebuild

1. **Build** ? **Clean Solution**
2. **Build** ? **Rebuild Solution**
3. The warning should be gone!

## Verify It Works

Check build output for these files being compiled:
```
Compiling...
  buffer.c
  hook.c
  trampoline.c
  hde64.c
```

If you don't see these, the files aren't being compiled!

## Project Structure Should Look Like:

```
Mira Cleaner/
??? Mira DLL/
?   ??? dllmain.cpp
?   ??? HWIDSpoof.cpp
?   ??? (other DLL files)
??? minhook/
    ??? include/
    ?   ??? MinHook.h
    ??? src/
        ??? buffer.c
        ??? hook.c
        ??? trampoline.c
        ??? hde/
            ??? hde64.c  ? Use for x64
            ??? hde32.c  ? Use for x86
```

## Common Mistakes

? **WRONG:** Adding only MinHook.h header  
? **CORRECT:** Adding all source .c files

? **WRONG:** Files excluded from build  
? **CORRECT:** Files set to compile (check Properties)

? **WRONG:** Using hde32.c for x64 build  
? **CORRECT:** Using hde64.c for x64 build

## Alternative: Use Pre-built Library

If you don't want to compile from source:

1. Get pre-built `MinHook.x64.lib` from releases
2. Add to project:
   - **Linker** ? **General** ? **Additional Library Directories**: `$(ProjectDir)minhook\lib\`
   - **Linker** ? **Input** ? **Additional Dependencies**: `MinHook.x64.lib;%(AdditionalDependencies)`
3. Still need `MinHook.h` in include path
