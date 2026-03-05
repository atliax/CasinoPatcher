# CasinoPatcher v1.0 by atliax
### Compatibility patch for Leisure Suit Larry's Casino (1998)

CasinoPatcher is a small Windows utility that patches **LCasino.exe** from Sierra's *Leisure Suit Larry's Casino* (1998) to enable it to run on modern systems.
It also installs a required compatibility shim (`GDI3x.dll`) into the game directory.

The patcher was tested with the following versions of the game:

- **1.0.0.3**
- **1.0.0.5**

Patching other versions is likely to work, but is **not guaranteed**.

---

## What the patcher does

1. Creates a backup of the original executable:

   LCasino.exe -> LCasino.bak

2. Creates a copy of the backup using the original filename.

3. Applies a small binary patch to the copied executable.

4. Copies `GDI3x.dll` into the game directory.

The patcher **never modifies the original executable directly**.  
If something goes wrong, you can restore the original by renaming:

LCasino.bak -> LCasino.exe

---

## Requirements

- Windows (any modern version)
- The original **LCasino.exe**
- `GDI3x.dll` placed in the **same directory as CasinoPatcher.exe**

Your folder should look like this before running the patcher:

```
CasinoPatcher.exe
GDI3x.dll
```

---

## How to use

1. Run **CasinoPatcher.exe**.
2. Click **Select file**
3. Select your `LCasino.exe`.
4. Click **Patch file**.
5. Confirm the prompt.

After patching, the game directory should contain:

```
LCasino.exe      (patched)
LCasino.bak      (original backup)
GDI3x.dll        (compatibility shim)
```

---

## Download

Prebuilt binaries are available in the **Releases** section somewhere on the right side of this page.

---

## Notes

- The patcher verifies the executable using an MD5 checksum for known versions.
- If the checksum does not match a known version, the patcher will warn you but still allow patching.
- Use at your own risk when patching unknown versions.

---

## Troubleshooting

### "GDI3x.dll not found"

Make sure `GDI3x.dll` is in the same directory as **CasinoPatcher.exe**.

### The patcher cannot write to the game directory

Try running the patcher **as Administrator** or install the game somewhere outside `Program Files`.

### The game fails to start after patching

Restore the backup:

LCasino.bak -> LCasino.exe

Then contact me here by creating an issue describing the problem as best you can.

---

## Disclaimer

This patcher modifies the game executable in memory-safe ways and always creates a backup first, but it is provided **as-is with no warranty**. Use at your own risk.

This project is an unofficial patching tool and is not affiliated with or endorsed by the original developers or publishers of the game.

## License

This project is licensed under the MIT License. See the LICENSE file for details.