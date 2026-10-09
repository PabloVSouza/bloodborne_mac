# Game files

[← Documentation](../README.md)

**No game files are included with this project.** You need a decrypted dump of your own copy of
Bloodborne for PS4, with the **1.09** update.

## Preparing the game folder

A dump usually comes as two folders: the base game (version 1.00) and the update (1.09). The port
needs a single folder with the update merged in:

1. Copy the base game folder (for example `CUSA03173`).
2. Copy **everything** from the update folder (for example `CUSA03173-patch`) into it, replacing
   files. This includes `eboot.bin` and `sce_sys`.

The result is a folder with `eboot.bin`, `sce_sys/` and `dvdroot_ps4/`. Choose that folder in the
launcher.

## Supported editions

The port runs the game's 1.09 executable: its hooks and patches use that executable's addresses.
It is tested with **CUSA03173** (Europe, Game of the Year edition).

Other editions and regions (CUSA00207, CUSA00900, CUSA03023, ...) work when their 1.09 executable
is the same one. The launcher compares the executable itself, not the title ID, so a matching
edition is accepted automatically.

## The Old Hunters

The launcher's **Home** tab shows whether *The Old Hunters* is included:

- **Included:** the Game of the Year editions (CUSA03173, CUSA03023) contain it.
- **Not included:** other editions sell it as an add-on. The port does not load add-on packages
  yet.

## When the launcher reports a problem

| Message | What it means | What to do |
|---|---|---|
| Needs the 1.09 update | The folder is the base game or an older update | Merge the 1.09 update into it (see above) |
| `eboot.bin` is not 1.09 | `param.sfo` says 1.09 but the executable is older | Copy `eboot.bin` from the 1.09 update |
| Another build of 1.09 | A Bloodborne edition whose executable differs from the supported one | Not supported yet |
| Not Bloodborne | The folder is another game | Choose your Bloodborne folder |
| Cannot be read | `eboot.bin` is not a decrypted PS4 executable | Dump the game and update again |
