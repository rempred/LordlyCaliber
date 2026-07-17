# Project64 Battery Save Manager

This is an optional standalone desktop utility distributed alongside
LordlyCaliber. It is not an editor tab and is never loaded by the browser
application.

The tool indexes Ogre Battle 64 Project64 `.sra` battery saves, groups
byte-identical copies, shows the valid save slots and unlocked world nodes,
and copies a selected battery save into the per-ROM directory Project64
expects. It handles battery saves only; Project64 savestates are excluded.

## Run the GUI

Double-click `pj64_battery_save_manager.py`, or run:

```powershell
py -3 pj64_battery_save_manager.py
```

The source version requires Python 3 with Tkinter. It has no third-party
packages or `pip` dependencies. The standard Windows installer from
python.org includes Tkinter by default.

Long paths, hashes, and explanatory text reflow as the window is resized. In
the Save Library panel, drag the divider between **Library folders** and
**Distinct save contents** to give either list more vertical space. Long
library-folder paths also have a horizontal scrollbar.

## Portable behavior

The script contains no user-specific names or absolute profile paths. Source,
ROM, and Project64 directories are selected at runtime and may be arbitrary
user-accessible locations. Its index and assignment manifests are stored in:

```text
%LOCALAPPDATA%\LordlyCaliber\PJ64SaveManager\
```

On non-Windows systems it uses `$XDG_DATA_HOME`, or `~/.local/share` when that
variable is unset.

## Safety

Assignment copies the selected save; it does not move or modify the source.
A differing destination requires confirmation and is backed up before it is
replaced. The tool also refuses to assign while `Project64.exe` is running so
the emulator cannot overwrite the copied file with stale in-memory data.

Run `py -3 pj64_battery_save_manager.py --help` for the optional command-line
index, prepare, assign, and interactive modes.

## Development verification

From this directory, run the standard-library test suite with:

```powershell
py -3 -m unittest discover -s tests -p "test_*.py" -v
```
