#!/usr/bin/env python3
"""Index and assign Ogre Battle 64 Project64 battery saves.

This tool intentionally handles only 32 KiB Project64 ``.sra`` cartridge
saves. It never reads, rewrites, or attempts to reseed Project64 savestates.

Project64 stores OB64 battery saves beneath a directory keyed by the MD5 of
the complete ROM normalized to ``.n64`` byte order::

    <Project64 root>/Save/OgreBattle64-<MD5>/OgreBattle64.sra

Run the script without arguments for its native desktop GUI. The recursive
index groups byte-identical saves so the hundreds of per-build copies are
presented as a small set of distinct battery saves, including each valid slot's
currently selectable world nodes. Assignment is a copy, not a move. A
differing destination is never overwritten unless replacement is explicitly
confirmed, and replacement first creates a hash-named backup. ``prepare`` can
create the expected per-ROM directory without copying a save. All original CLI
subcommands remain available.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid
from typing import Any, Iterable, Mapping, Sequence


TOOL_VERSION = "1.2.2"
TOOL_SOURCE = (
    "editor/supplemental-tools/project64-battery-save-manager/"
    "pj64_battery_save_manager.py"
)
PREFIX = "OgreBattle64"
SAVE_FILENAME = f"{PREFIX}.sra"
SAVE_DIR_RE = re.compile(rf"^{PREFIX}-([0-9A-Fa-f]{{32}})$")
ROM_SUFFIXES = {".v64", ".z64", ".n64"}
PJ64_SRA_SIZE = 0x8000
SAVERAM_SIZE = 0x10000
SLOT_BASE = 0x10
SLOT_STRIDE = 0x1850
SLOT_COUNT = 3
MAGIC_OFFSET = 0x04
HEADER_NAME_OFFSET = 0x18
CHECKSUM_REGION_OFFSET = 0x0C
CHECKSUM_REGION_SIZE = 0x1844
SAVERAM_PACKED_OFFSET = 0x2A
SAVERAM_PACKED_SIZE = 0x1824
NATIVE_MAGIC = b"QuestOG3"

# Bit offsets within the packed slot payload. They are derived from the
# byte-verified native save codec descriptor at RDRAM 0x80187444. The world
# group (physical RDRAM base 0x196A58) begins at packed bit 42986; these three
# fields are byte-width commands within that group.
WORLD_EDGE_PACKED_BIT_OFFSET = 43284
WORLD_NODE_STATE_PACKED_BIT_OFFSET = 43324
WORLD_ACTIVE_NODE_PACKED_BIT_OFFSET = 43428
WORLD_NODE_MIN = 2
WORLD_NODE_MAX = 40

WORLD_NODE_NAMES = {
    2: "Pre-game",
    3: "Crenel Canyon",
    4: "Volmus Mine",
    5: "Volmus Mine (revisit)",
    6: "Tenne Plains",
    7: "Alba",
    8: "Dardunnelles, the Crossroads",
    9: "Gunther Piedmont",
    10: "Mylesia",
    11: "Gules Hills",
    12: "Tremos Mountains",
    13: "Fair Heights",
    14: "Temple of Berthe",
    15: "Capitrium, the Land of Advent",
    16: "Celesis, the Eastern Church",
    17: "Tremos Mountains (revisit)",
    18: "Sable Lowlands",
    19: "Audvera Heights",
    20: "The Highland of Soathon",
    21: "Mount Ithaca",
    22: "Azure Plains",
    23: "Wentinus",
    24: "Mount Keryoleth",
    25: "Tybell, the Wicked Land",
    26: "The Tundra of Argent",
    27: "Vert Plateau",
    28: "Aurua Plains",
    29: "Barpheth",
    30: "Latium",
    31: "The Blue Basilica",
    32: "Ptia, the Secluded Land",
    33: "Romulus",
    34: "Wentinus (revisit)",
    35: "Dardunnelles, the Crossroads (revisit)",
    36: "Mount Keryoleth (revisit)",
    37: "Winnea, Capital of Palatinus",
    38: "Castle Talpaea",
    39: "Fort Romulus",
    40: "Alba (revisit)",
}

SCRIPT_DIR = Path(__file__).resolve().parent


def user_data_root(
    environment: Mapping[str, str] | None = None,
    home: Path | None = None,
) -> Path:
    """Return a writable per-user data root for source and frozen builds."""
    values = os.environ if environment is None else environment
    local_app_data = values.get("LOCALAPPDATA")
    if local_app_data:
        base = Path(local_app_data).expanduser()
    elif values.get("XDG_DATA_HOME"):
        base = Path(values["XDG_DATA_HOME"]).expanduser()
    else:
        home_path = (home or Path.home()).expanduser()
        base = home_path / "AppData" / "Local" if os.name == "nt" else home_path / ".local" / "share"
    return base / "LordlyCaliber" / "PJ64SaveManager"


USER_DATA_ROOT = user_data_root()
DEFAULT_INDEX = USER_DATA_ROOT / "pj64-battery-save-index.json"
DEFAULT_ASSIGNMENT_DIR = USER_DATA_ROOT / "assignment-manifests"


class BatterySaveError(RuntimeError):
    """A user-facing catalog or assignment error."""


def utc_now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


def utc_text(value: dt.datetime | None = None) -> str:
    value = value or utc_now()
    return value.astimezone(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def gui_wrap_length(container_width: int, padding: int = 28, minimum: int = 80) -> int:
    """Return a safe Tk label wrap width for a resizable container."""
    return max(minimum, int(container_width) - padding)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def normalize_rom_to_n64(data: bytes) -> bytes:
    """Return complete ROM bytes in Project64's little-endian word order."""
    if len(data) < 4:
        raise BatterySaveError("ROM is too small to identify")
    if len(data) % 4:
        raise BatterySaveError(f"ROM size is not word-aligned: {len(data)} bytes")
    magic = data[:4]
    if magic == b"\x40\x12\x37\x80":  # already .n64
        return data
    out = bytearray(len(data))
    if magic == b"\x37\x80\x40\x12":  # .v64 -> .z64
        # v64 word [b0,b1,b2,b3] -> n64 [b2,b3,b0,b1]. Sliced
        # assignments keep a 40 MiB ROM conversion comfortably sub-second.
        out[0::4] = data[2::4]
        out[1::4] = data[3::4]
        out[2::4] = data[0::4]
        out[3::4] = data[1::4]
        return bytes(out)
    if magic != b"\x80\x37\x12\x40":  # .z64
        raise BatterySaveError(f"unknown ROM byte order: {magic.hex().upper()}")
    out[0::4] = data[3::4]
    out[1::4] = data[2::4]
    out[2::4] = data[1::4]
    out[3::4] = data[0::4]
    return bytes(out)


def rom_identity(path: Path) -> dict[str, Any]:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise BatterySaveError(f"ROM not found: {resolved}")
    raw = resolved.read_bytes()
    normalized = normalize_rom_to_n64(raw)
    return {
        "path": str(resolved),
        "size": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
        "pj64SaveHash": hashlib.md5(normalized).hexdigest().upper(),
        "byteOrderMagic": raw[:4].hex().upper(),
    }


def pj64_to_native_saveram(raw: bytes) -> bytes:
    if len(raw) != PJ64_SRA_SIZE:
        raise BatterySaveError(
            f"expected a {PJ64_SRA_SIZE}-byte Project64 .sra, got {len(raw)} bytes"
        )
    native = bytearray(SAVERAM_SIZE)
    for offset in range(0, len(raw), 4):
        native[offset : offset + 4] = reversed(raw[offset : offset + 4])
    return bytes(native)


def _read_ascii(data: bytes, offset: int, maximum: int) -> str:
    chars: list[str] = []
    for value in data[offset : offset + maximum]:
        if value in (0, 0xFF):
            break
        if 0x20 <= value < 0x7F:
            chars.append(chr(value))
    return "".join(chars)


def _slot_checksums(native: bytes, base: int) -> tuple[int, int]:
    total = base & 0xFFFF
    bit_count = base & 0xFFFF
    start = base + CHECKSUM_REGION_OFFSET
    end = start + CHECKSUM_REGION_SIZE
    for value in native[start:end]:
        total = (total + value) & 0xFFFF
        bit_count = (bit_count + value.bit_count()) & 0xFFFF
    return total, bit_count


def _read_packed_bits(payload: bytes, bit_offset: int, bit_count: int) -> int:
    """Read one MSB-first value from the game's packed save payload."""
    if bit_offset < 0 or bit_count < 0 or bit_offset + bit_count > len(payload) * 8:
        raise BatterySaveError("packed save field is outside the slot payload")
    value = 0
    for bit_index in range(bit_offset, bit_offset + bit_count):
        byte_value = payload[bit_index >> 3]
        value = (value << 1) | ((byte_value >> (7 - (bit_index & 7))) & 1)
    return value


def _read_packed_bytes(payload: bytes, bit_offset: int, count: int) -> bytes:
    return bytes(_read_packed_bits(payload, bit_offset + index * 8, 8) for index in range(count))


def _world_node_label(node_id: int) -> dict[str, Any]:
    return {
        "id": node_id,
        "name": WORLD_NODE_NAMES.get(node_id, f"Node {node_id}"),
    }


def decode_world_availability(native: bytes, slot_base: int) -> dict[str, Any]:
    """Decode persisted world-map availability without mutating the save.

    The most useful answer is a set, not a guessed chronological maximum:
    branching progress can leave more than one selectable mission marker.
    """
    packed_start = slot_base + SAVERAM_PACKED_OFFSET
    payload = native[packed_start : packed_start + SAVERAM_PACKED_SIZE]
    if len(payload) != SAVERAM_PACKED_SIZE:
        raise BatterySaveError("battery-save slot does not contain a complete packed payload")

    state_bytes = _read_packed_bytes(payload, WORLD_NODE_STATE_PACKED_BIT_OFFSET, 10)
    active_bytes = _read_packed_bytes(payload, WORLD_ACTIVE_NODE_PACKED_BIT_OFFSET, 5)
    node_states: dict[int, int] = {}
    active_nodes: list[int] = []
    selectable_nodes: list[int] = []
    for node_id in range(WORLD_NODE_MIN, WORLD_NODE_MAX + 1):
        zero_based = node_id - 1
        state = (state_bytes[zero_based >> 2] >> ((zero_based & 3) * 2)) & 0x03
        active = bool(active_bytes[zero_based >> 3] & (1 << (zero_based & 7)))
        if state:
            node_states[node_id] = state
        if active:
            active_nodes.append(node_id)
        if state == 2 and active:
            selectable_nodes.append(node_id)

    selectable = [_world_node_label(node_id) for node_id in selectable_nodes]
    return {
        "currentlySelectableNodes": selectable,
        "summary": ", ".join(f"{node['name']} (#{node['id']})" for node in selectable)
        or "no active node",
        "activeNodeIds": active_nodes,
        "nonzeroNodeStates": {str(node_id): state for node_id, state in node_states.items()},
        "interpretation": "node state 2 intersected with the active-node marker; branch choices are retained",
    }


def inspect_sra(path: Path) -> dict[str, Any]:
    resolved = path.expanduser().resolve()
    stat = resolved.stat()
    raw = resolved.read_bytes()
    result: dict[str, Any] = {
        "path": str(resolved),
        "size": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
        "mtimeUtc": utc_text(dt.datetime.fromtimestamp(stat.st_mtime, dt.timezone.utc)),
        "format": "pj64-sra" if len(raw) == PJ64_SRA_SIZE else "invalid-size",
        "validOb64BatterySave": False,
        "slots": [],
    }
    if len(raw) != PJ64_SRA_SIZE:
        return result
    native = pj64_to_native_saveram(raw)
    slots = []
    for slot_index in range(SLOT_COUNT):
        base = SLOT_BASE + slot_index * SLOT_STRIDE
        has_magic = native[base + MAGIC_OFFSET : base + MAGIC_OFFSET + len(NATIVE_MAGIC)] == NATIVE_MAGIC
        stored_sum = int.from_bytes(native[base : base + 2], "big")
        stored_bits = int.from_bytes(native[base + 2 : base + 4], "big")
        calc_sum, calc_bits = _slot_checksums(native, base)
        checksum_ok = stored_sum == calc_sum and stored_bits == calc_bits
        status0 = native[base + CHECKSUM_REGION_OFFSET]
        empty = (not has_magic) or status0 == 0xFF
        valid = has_magic and not empty and checksum_ok
        slots.append(
            {
                "slot": slot_index + 1,
                "name": _read_ascii(native, base + HEADER_NAME_OFFSET, 16),
                "hasMagic": has_magic,
                "empty": empty,
                "checksumOk": checksum_ok,
                "valid": valid,
                "worldMap": decode_world_availability(native, base) if valid else None,
            }
        )
    result["slots"] = slots
    result["validOb64BatterySave"] = any(slot["valid"] for slot in slots)
    return result


def discover_save_roots(extra_roots: Iterable[Path] = ()) -> list[Path]:
    candidates: list[Path] = []
    program_files_x86 = os.environ.get("ProgramFiles(x86)")
    local_app_data = os.environ.get("LOCALAPPDATA")
    if program_files_x86:
        candidates.append(Path(program_files_x86) / "Project64 Dev 4.0" / "Save")
    if local_app_data:
        candidates.append(Path(local_app_data) / "OgreBattle64" / "Project64Dev4" / "Save")

    # Development builds are optional and discovered relative to any ancestor
    # of the shared script. This keeps the release copy independent of a
    # particular repository layout or user-profile path.
    for ancestor in (SCRIPT_DIR, *SCRIPT_DIR.parents):
        project64_win32 = ancestor / "project64" / "bin" / "Win32"
        if project64_win32.is_dir():
            candidates.extend(
                path / "Save" for path in project64_win32.iterdir() if path.is_dir()
            )

    candidates.extend(Path(path).expanduser() for path in extra_roots)
    unique: dict[str, Path] = {}
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
        except OSError:
            continue
        if resolved.is_dir():
            unique[os.path.normcase(str(resolved))] = resolved
    return sorted(unique.values(), key=lambda path: os.path.normcase(str(path)))


def scan_save_roots(save_roots: Sequence[Path]) -> list[dict[str, Any]]:
    locations: list[dict[str, Any]] = []
    seen_paths: set[str] = set()
    for save_root in save_roots:
        resolved_root = save_root.expanduser().resolve()
        for directory, child_names, file_names in os.walk(resolved_root, onerror=lambda _error: None):
            child_names.sort(key=str.lower)
            folder = Path(directory)
            match = SAVE_DIR_RE.fullmatch(folder.name)
            if not match:
                continue
            matching_names = [name for name in file_names if name.lower() == SAVE_FILENAME.lower()]
            if not matching_names:
                continue
            save_path = folder / sorted(matching_names, key=str.lower)[0]
            normalized_path = os.path.normcase(str(save_path.resolve()))
            if normalized_path in seen_paths:
                continue
            seen_paths.add(normalized_path)
            metadata = inspect_sra(save_path)
            metadata.update(
                {
                    "saveRoot": str(resolved_root),
                    "folderHash": match.group(1).upper(),
                }
            )
            locations.append(metadata)
    return sorted(locations, key=lambda item: os.path.normcase(item["path"]))


def _slot_summary(slots: Sequence[dict[str, Any]]) -> str:
    values = []
    for slot in slots:
        if slot["valid"]:
            world = slot.get("worldMap") or {}
            world_summary = world.get("summary")
            suffix = f" [{world_summary}]" if world_summary else ""
            values.append(f"{slot['slot']}:{slot['name'] or '(unnamed)'}{suffix}")
        elif slot["hasMagic"]:
            values.append(f"{slot['slot']}:invalid")
    return ", ".join(values) or "no valid slots"


def group_locations(locations: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    by_hash: dict[str, list[dict[str, Any]]] = {}
    for location in locations:
        by_hash.setdefault(location["sha256"], []).append(location)
    groups = []
    for sha256, copies in by_hash.items():
        copies = sorted(copies, key=lambda item: item["mtimeUtc"], reverse=True)
        groups.append(
            {
                "id": sha256[:12],
                "sha256": sha256,
                "size": copies[0]["size"],
                "validOb64BatterySave": copies[0]["validOb64BatterySave"],
                "slots": copies[0]["slots"],
                "slotSummary": _slot_summary(copies[0]["slots"]),
                "latestMtimeUtc": copies[0]["mtimeUtc"],
                "copyCount": len(copies),
                "folderHashes": sorted({copy["folderHash"] for copy in copies}),
                "locations": [
                    {
                        key: copy[key]
                        for key in ("path", "saveRoot", "folderHash", "mtimeUtc")
                    }
                    for copy in copies
                ],
            }
        )
    return sorted(groups, key=lambda item: item["latestMtimeUtc"], reverse=True)


def find_roms(rom_dirs: Sequence[Path]) -> list[Path]:
    roms: dict[str, Path] = {}
    for root in rom_dirs:
        resolved = root.expanduser().resolve()
        paths = [resolved] if resolved.is_file() else resolved.rglob("*") if resolved.is_dir() else []
        for path in paths:
            if path.is_file() and path.suffix.lower() in ROM_SUFFIXES:
                roms[os.path.normcase(str(path.resolve()))] = path.resolve()
    return sorted(roms.values(), key=lambda path: os.path.normcase(str(path)))


def build_index(save_roots: Sequence[Path], rom_dirs: Sequence[Path] = ()) -> dict[str, Any]:
    locations = scan_save_roots(save_roots)
    roms = []
    for path in find_roms(rom_dirs):
        try:
            roms.append(rom_identity(path))
        except (BatterySaveError, OSError) as error:
            roms.append({"path": str(path), "error": str(error)})
    groups = group_locations(locations)
    roms_by_save_hash: dict[str, list[dict[str, Any]]] = {}
    for rom in roms:
        if "pj64SaveHash" in rom:
            roms_by_save_hash.setdefault(rom["pj64SaveHash"], []).append(rom)
    for group in groups:
        group["knownRoms"] = [
            rom
            for folder_hash in group["folderHashes"]
            for rom in roms_by_save_hash.get(folder_hash, [])
        ]
    return {
        "schemaVersion": 1,
        "tool": TOOL_SOURCE,
        "toolVersion": TOOL_VERSION,
        "generatedAtUtc": utc_text(),
        "scope": "Project64 .sra battery saves only; savestates excluded",
        "saveRoots": [str(path) for path in save_roots],
        "locationCount": len(locations),
        "distinctSaveCount": len({location["sha256"] for location in locations}),
        "saves": groups,
        "roms": roms,
    }


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp-{uuid.uuid4().hex}")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def load_index(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise BatterySaveError(f"battery-save index not found: {path}; run the index command first")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1 or not isinstance(data.get("saves"), list):
        raise BatterySaveError(f"unsupported battery-save index: {path}")
    return data


def select_indexed_save(index: dict[str, Any], sha_prefix: str) -> dict[str, Any]:
    normalized = sha_prefix.strip().upper()
    matches = [entry for entry in index["saves"] if entry["sha256"].startswith(normalized)]
    if not matches:
        raise BatterySaveError(f"no indexed save matches SHA-256 prefix {normalized!r}")
    if len(matches) != 1:
        raise BatterySaveError(f"SHA-256 prefix {normalized!r} matches {len(matches)} saves; use more characters")
    return matches[0]


def source_from_group(group: dict[str, Any]) -> Path:
    for location in group["locations"]:
        path = Path(location["path"])
        if path.is_file() and file_sha256(path) == group["sha256"]:
            return path
    raise BatterySaveError(f"no indexed copy of save {group['sha256']} still exists with the indexed bytes")


def project64_running() -> bool:
    if os.name != "nt":
        return False
    try:
        result = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq Project64.exe", "/FO", "CSV", "/NH"],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return False
    return "Project64.exe" in result.stdout


def assignment_manifest_path(rom_hash: str, timestamp: dt.datetime) -> Path:
    stamp = timestamp.strftime("%Y%m%dT%H%M%SZ")
    base_name = f"{stamp}-{rom_hash[:12]}"
    candidate = DEFAULT_ASSIGNMENT_DIR / f"{base_name}.json"
    suffix = 2
    while candidate.exists():
        candidate = DEFAULT_ASSIGNMENT_DIR / f"{base_name}-{suffix}.json"
        suffix += 1
    return candidate


def replacement_backup_path(
    destination: Path,
    timestamp: dt.datetime,
    existing_sha256: str,
) -> Path:
    """Return a sibling backup name that never overwrites prior evidence."""
    stamp = timestamp.strftime("%Y%m%dT%H%M%SZ")
    base_name = f"{SAVE_FILENAME}.backup-{stamp}-{existing_sha256[:12]}"
    candidate = destination.with_name(base_name)
    suffix = 2
    while candidate.exists():
        candidate = destination.with_name(f"{base_name}-{suffix}")
        suffix += 1
    return candidate


def rom_save_directory(rom: Path, save_root: Path) -> tuple[dict[str, Any], Path]:
    rom_meta = rom_identity(rom)
    resolved_root = save_root.expanduser().resolve()
    directory = resolved_root / f"{PREFIX}-{rom_meta['pj64SaveHash']}"
    return rom_meta, directory


def prepare_save_directory(*, rom: Path, save_root: Path, apply: bool) -> dict[str, Any]:
    """Preview or create the per-ROM Project64 battery-save directory."""
    rom_meta, directory = rom_save_directory(rom, save_root)
    existed_before = directory.is_dir()
    if directory.exists() and not existed_before:
        raise BatterySaveError(f"expected save directory path is occupied by a file: {directory}")
    result = {
        "schemaVersion": 1,
        "tool": TOOL_SOURCE,
        "toolVersion": TOOL_VERSION,
        "generatedAtUtc": utc_text(),
        "scope": "Project64 per-ROM battery-save directory preparation; no save, savestate, ROM, or RAM mutation",
        "dryRun": not apply,
        "action": "already-exists" if existed_before else "create",
        "rom": rom_meta,
        "saveRoot": str(save_root.expanduser().resolve()),
        "directory": str(directory),
        "destination": str(directory / SAVE_FILENAME),
        "existedBefore": existed_before,
    }
    if apply and not existed_before:
        directory.mkdir(parents=True, exist_ok=True)
    if apply and not directory.is_dir():
        raise BatterySaveError(f"failed to create Project64 save directory: {directory}")
    result["existsAfter"] = directory.is_dir()
    return result


def assign_save(
    *,
    source: Path,
    rom: Path,
    save_root: Path,
    apply: bool,
    replace: bool,
    allow_invalid: bool,
    allow_running: bool,
    manifest_path: Path | None = None,
    command_line: Sequence[str] | None = None,
) -> dict[str, Any]:
    source_meta = inspect_sra(source)
    if not source_meta["validOb64BatterySave"] and not allow_invalid:
        raise BatterySaveError(
            f"source is not a checksum-valid OB64 Project64 battery save: {source}; "
            "use --allow-invalid only for a deliberate forensic copy"
        )
    rom_meta, destination_directory = rom_save_directory(rom, save_root)
    resolved_root = save_root.expanduser().resolve()
    destination = destination_directory / SAVE_FILENAME
    destination_before = inspect_sra(destination) if destination.is_file() else None
    action = "copy"
    if destination_before:
        if destination_before["sha256"] == source_meta["sha256"]:
            action = "already-identical"
        elif replace:
            action = "replace"
        else:
            raise BatterySaveError(
                "destination already contains a different battery save; rerun with --replace to back it up "
                f"and replace it: {destination}"
            )

    timestamp = utc_now()
    backup_path: Path | None = None
    manifest = {
        "schemaVersion": 1,
        "tool": TOOL_SOURCE,
        "toolVersion": TOOL_VERSION,
        "generatedAtUtc": utc_text(timestamp),
        "commandLine": list(command_line or []),
        "scope": "Project64 .sra battery save assignment; no savestate or RAM mutation",
        "dryRun": not apply,
        "action": action,
        "source": source_meta,
        "rom": rom_meta,
        "destination": {
            "saveRoot": str(resolved_root),
            "path": str(destination),
            "before": destination_before,
            "afterSha256": source_meta["sha256"] if action != "already-identical" or destination_before else None,
        },
        "backup": None,
    }
    if not apply:
        return manifest
    if project64_running() and not allow_running:
        raise BatterySaveError(
            "Project64.exe is running. Close it before assigning a battery save so shutdown cannot flush stale "
            "SRAM over the assigned file, or use --allow-running if the risk is deliberate."
        )

    destination.parent.mkdir(parents=True, exist_ok=True)
    if action == "replace" and destination_before:
        backup_path = replacement_backup_path(
            destination,
            timestamp,
            destination_before["sha256"],
        )
        shutil.copy2(destination, backup_path)
        backup_sha = file_sha256(backup_path)
        if backup_sha != destination_before["sha256"]:
            raise BatterySaveError(f"destination backup verification failed: {backup_path}")
        manifest["backup"] = {"path": str(backup_path), "sha256": backup_sha}

    if action != "already-identical":
        temporary = destination.with_name(f".{SAVE_FILENAME}.tmp-{uuid.uuid4().hex}")
        try:
            shutil.copy2(source, temporary)
            if file_sha256(temporary) != source_meta["sha256"]:
                raise BatterySaveError(f"temporary copy verification failed: {temporary}")
            os.replace(temporary, destination)
        finally:
            if temporary.exists():
                temporary.unlink()
    destination_after = inspect_sra(destination)
    if destination_after["sha256"] != source_meta["sha256"]:
        raise BatterySaveError(f"assigned save verification failed: {destination}")
    manifest["destination"]["after"] = destination_after
    manifest["destination"]["afterSha256"] = destination_after["sha256"]

    output = manifest_path or assignment_manifest_path(rom_meta["pj64SaveHash"], timestamp)
    write_json(output.expanduser().resolve(), manifest)
    manifest["manifestPath"] = str(output.expanduser().resolve())
    return manifest


def print_index(
    index: dict[str, Any],
    include_invalid: bool = False,
    show_locations: bool = False,
) -> None:
    print(
        f"Indexed {index['locationCount']} Project64 battery-save locations as "
        f"{index['distinctSaveCount']} distinct byte sets."
    )
    shown = 0
    for entry in index["saves"]:
        if not entry["validOb64BatterySave"] and not include_invalid:
            continue
        shown += 1
        valid = "valid" if entry["validOb64BatterySave"] else "INVALID"
        print(
            f"{shown:>3}. {entry['id']}  {entry['latestMtimeUtc']}  {valid:<7}  "
            f"copies={entry['copyCount']:<3}  {entry['slotSummary']}"
        )
        if show_locations:
            for location in entry["locations"]:
                print(f"     SAVE {location['path']}")
        for rom in entry.get("knownRoms", []):
            print(f"     ROM {rom['path']}")


def command_index(args: argparse.Namespace) -> int:
    roots = discover_save_roots(Path(path) for path in args.save_root)
    if not roots:
        raise BatterySaveError("no Project64 Save roots were found; pass --save-root")
    index = build_index(roots, [Path(path) for path in args.rom_dir])
    output = Path(args.output).expanduser().resolve()
    write_json(output, index)
    print_index(
        index,
        include_invalid=args.include_invalid,
        show_locations=args.show_locations,
    )
    print(f"Index written: {output}")
    return 0


def resolve_source(args: argparse.Namespace) -> Path:
    if args.save:
        source = Path(args.save).expanduser().resolve()
        if not source.is_file():
            raise BatterySaveError(f"battery save not found: {source}")
        return source
    index = load_index(Path(args.index).expanduser().resolve())
    return source_from_group(select_indexed_save(index, args.sha))


def resolve_destination_root(args: argparse.Namespace) -> Path:
    if args.save_root:
        return Path(args.save_root).expanduser().resolve()
    return Path(args.pj64_root).expanduser().resolve() / "Save"


def command_prepare(args: argparse.Namespace) -> int:
    result = prepare_save_directory(
        rom=Path(args.rom),
        save_root=resolve_destination_root(args),
        apply=args.apply,
    )
    suffix = " (preview)" if result["dryRun"] else ""
    print(f"Action:      {result['action']}{suffix}")
    print(f"ROM hash:    {result['rom']['pj64SaveHash']}")
    print(f"Directory:   {result['directory']}")
    print(f"Save target: {result['destination']}")
    if not args.apply:
        print("No files changed. Add --apply to create this directory.")
    return 0


def command_assign(args: argparse.Namespace) -> int:
    source = resolve_source(args)
    save_root = resolve_destination_root(args)
    manifest = assign_save(
        source=source,
        rom=Path(args.rom),
        save_root=save_root,
        apply=args.apply,
        replace=args.replace,
        allow_invalid=args.allow_invalid,
        allow_running=args.allow_running,
        manifest_path=Path(args.manifest) if args.manifest else None,
        command_line=sys.argv,
    )
    print(f"Action:      {manifest['action']}" + (" (preview)" if manifest["dryRun"] else ""))
    print(f"Source:      {manifest['source']['path']}")
    print(f"Source SHA:  {manifest['source']['sha256']}")
    print(f"ROM hash:    {manifest['rom']['pj64SaveHash']}")
    print(f"Destination: {manifest['destination']['path']}")
    if manifest.get("backup"):
        print(f"Backup:      {manifest['backup']['path']}")
    if manifest.get("manifestPath"):
        print(f"Manifest:    {manifest['manifestPath']}")
    if not args.apply:
        print("No files changed. Add --apply to perform this assignment.")
    return 0


def command_interactive(args: argparse.Namespace) -> int:
    roots = discover_save_roots(Path(path) for path in args.extra_save_root)
    if args.save:
        source = Path(args.save).expanduser().resolve()
        if not source.is_file():
            raise BatterySaveError(f"battery save not found: {source}")
        source_meta = inspect_sra(source)
        if not source_meta["validOb64BatterySave"]:
            raise BatterySaveError(f"source is not a checksum-valid OB64 battery save: {source}")
        print(f"Selected save: {source}")
        print(f"Slots:         {_slot_summary(source_meta['slots'])}")
    else:
        if not roots:
            raise BatterySaveError(
                "no Project64 Save roots were found; pass --extra-save-root or select a file with --save"
            )
        index_path = Path(args.index).expanduser().resolve()
        index = build_index(roots)
        write_json(index_path, index)
        valid_groups = [entry for entry in index["saves"] if entry["validOb64BatterySave"]]
        if not valid_groups:
            raise BatterySaveError("no checksum-valid OB64 Project64 battery saves were found")
        print_index(index)
        choice = input("Choose battery save number: ").strip()
        if not choice.isdigit() or not 1 <= int(choice) <= len(valid_groups):
            raise BatterySaveError("invalid battery-save selection")
        group = valid_groups[int(choice) - 1]
        source = source_from_group(group)

    rom_text = args.rom or input("Patched ROM path: ").strip().strip('"')
    if args.save_root:
        selected_root = Path(args.save_root)
    elif args.pj64_root:
        selected_root = Path(args.pj64_root) / "Save"
    else:
        if roots:
            print("Project64 Save roots:")
            for number, root in enumerate(roots, 1):
                print(f"  {number:>2}. {root}")
            root_choice = input(
                "Choose a destination number, or enter any Project64 Save path: "
            ).strip().strip('"')
            if root_choice.isdigit():
                if not 1 <= int(root_choice) <= len(roots):
                    raise BatterySaveError("invalid destination-root selection")
                selected_root = roots[int(root_choice) - 1]
            elif root_choice:
                selected_root = Path(root_choice)
            else:
                raise BatterySaveError("destination Save path is required")
        else:
            root_text = input("Destination Project64 Save path: ").strip().strip('"')
            if not root_text:
                raise BatterySaveError("destination Save path is required")
            selected_root = Path(root_text)

    preview = assign_save(
        source=source,
        rom=Path(rom_text),
        save_root=selected_root,
        apply=False,
        replace=args.replace,
        allow_invalid=False,
        allow_running=args.allow_running,
        command_line=sys.argv,
    )
    print(f"\nSource:      {preview['source']['path']}")
    print(f"Slots:       {_slot_summary(preview['source']['slots'])}")
    print(f"Destination: {preview['destination']['path']}")
    confirmation = input("Type ASSIGN to copy this battery save: ").strip()
    if confirmation != "ASSIGN":
        print("Cancelled; no files changed.")
        return 0
    result = assign_save(
        source=source,
        rom=Path(rom_text),
        save_root=selected_root,
        apply=True,
        replace=args.replace,
        allow_invalid=False,
        allow_running=args.allow_running,
        command_line=sys.argv,
    )
    print(f"Assigned: {result['destination']['path']}")
    if result.get("backup"):
        print(f"Backup:   {result['backup']['path']}")
    print(f"Manifest: {result['manifestPath']}")
    return 0


def standalone_save_group(path: Path) -> dict[str, Any]:
    """Wrap one explicitly selected save in the same model used by an index."""
    metadata = inspect_sra(path)
    parent_match = SAVE_DIR_RE.fullmatch(path.expanduser().resolve().parent.name)
    save_root = path.expanduser().resolve().parent.parent if parent_match else path.parent
    return {
        "id": metadata["sha256"][:12],
        "sha256": metadata["sha256"],
        "size": metadata["size"],
        "validOb64BatterySave": metadata["validOb64BatterySave"],
        "slots": metadata["slots"],
        "slotSummary": _slot_summary(metadata["slots"]),
        "latestMtimeUtc": metadata["mtimeUtc"],
        "copyCount": 1,
        "folderHashes": [parent_match.group(1).upper()] if parent_match else [],
        "locations": [
            {
                "path": metadata["path"],
                "saveRoot": str(save_root.expanduser().resolve()),
                "folderHash": parent_match.group(1).upper() if parent_match else "",
                "mtimeUtc": metadata["mtimeUtc"],
            }
        ],
        "knownRoms": [],
    }


def launch_gui(initial_roots: Sequence[Path] = ()) -> int:
    """Launch the standard-library Tk desktop interface."""
    try:
        import threading
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
    except ImportError as error:
        raise BatterySaveError(
            "the desktop GUI requires Python's standard tkinter package"
        ) from error

    try:
        window = tk.Tk()
    except tk.TclError as error:
        raise BatterySaveError(f"could not open the desktop GUI: {error}") from error

    class BatterySaveManagerGui:
        def __init__(self, root: Any) -> None:
            self.root = root
            self.root.title("Ogre Battle 64 - Project64 Battery Save Manager")
            self.root.geometry("1220x820")
            self.root.minsize(960, 680)

            self.save_roots: list[Path] = []
            self.index: dict[str, Any] | None = None
            self.groups_by_item: dict[str, dict[str, Any]] = {}
            self.selected_group: dict[str, Any] | None = None
            self.selected_source: Path | None = None
            self.rom_meta: dict[str, Any] | None = None
            self.busy = False

            self.status_var = tk.StringVar(value="Choose a save library folder or an individual .sra file.")
            self.source_var = tk.StringVar(value="No battery save selected")
            self.source_details_var = tk.StringVar(
                value="Only checksum-valid Project64 .sra battery saves can be assigned."
            )
            self.rom_var = tk.StringVar(value="")
            self.rom_hash_var = tk.StringVar(value="No ROM selected")
            self.save_root_var = tk.StringVar(value="")
            self.target_var = tk.StringVar(value="Select a ROM and destination Save root.")

            self._build_styles()
            self._build_layout()
            self.save_root_var.trace_add("write", lambda *_args: self._update_target())

            discovered = discover_save_roots(initial_roots)
            for path in discovered:
                self._append_root(path)
            if self.save_roots:
                self.root.after(150, self.scan_library)

        def _build_styles(self) -> None:
            style = ttk.Style(self.root)
            available = style.theme_names()
            if "vista" in available:
                style.theme_use("vista")
            style.configure("Title.TLabel", font=("Segoe UI Semibold", 18))
            style.configure("Subtle.TLabel", foreground="#555555")
            style.configure("Path.TLabel", font=("Consolas", 9))
            style.configure("Treeview", rowheight=25)
            style.configure("Primary.TButton", font=("Segoe UI Semibold", 10))

        def _wrapped_label(
            self,
            parent: Any,
            *,
            wrap_padding: int = 28,
            **options: Any,
        ) -> Any:
            """Create a label whose wrapping follows its containing panel."""
            options.setdefault("anchor", "w")
            options.setdefault("justify", "left")
            label = ttk.Label(parent, wraplength=430, **options)

            def reflow(event: Any) -> None:
                wrap_length = gui_wrap_length(event.width, padding=wrap_padding)
                if int(label.cget("wraplength")) != wrap_length:
                    label.configure(wraplength=wrap_length)

            parent.bind("<Configure>", reflow, add="+")
            return label

        def _build_layout(self) -> None:
            self.root.columnconfigure(0, weight=1)
            self.root.rowconfigure(1, weight=1)

            heading = ttk.Frame(self.root, padding=(16, 14, 16, 8))
            heading.grid(row=0, column=0, sticky="ew")
            heading.columnconfigure(0, weight=1)
            ttk.Label(
                heading,
                text="Project64 Battery Save Manager",
                style="Title.TLabel",
            ).grid(row=0, column=0, sticky="w")
            ttk.Label(
                heading,
                text=(
                    "Battery saves only. Sources are copied, never moved; "
                    "Project64 savestates are excluded."
                ),
                style="Subtle.TLabel",
            ).grid(row=1, column=0, sticky="w", pady=(3, 0))

            content = ttk.Frame(self.root, padding=(16, 0, 16, 10))
            content.grid(row=1, column=0, sticky="nsew")
            content.columnconfigure(0, weight=3)
            content.columnconfigure(1, weight=2)
            content.rowconfigure(0, weight=1)

            self._build_library_panel(content)
            self._build_assignment_panel(content)

            status = ttk.Frame(self.root, padding=(16, 7, 16, 10))
            status.grid(row=2, column=0, sticky="ew")
            status.columnconfigure(0, weight=1)
            ttk.Separator(status).grid(row=0, column=0, sticky="ew", pady=(0, 7))
            ttk.Label(status, textvariable=self.status_var).grid(row=1, column=0, sticky="w")

        def _build_library_panel(self, parent: Any) -> None:
            panel = ttk.LabelFrame(parent, text="1. Save Library", padding=10)
            panel.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
            panel.columnconfigure(0, weight=1)
            panel.rowconfigure(1, weight=1)

            buttons = ttk.Frame(panel)
            buttons.grid(row=0, column=0, sticky="ew", pady=(0, 7))
            self.add_folder_button = ttk.Button(
                buttons,
                text="Add Folder...",
                command=self.choose_library_folder,
            )
            self.add_folder_button.pack(side="left")
            ttk.Button(
                buttons,
                text="Choose .sra File...",
                command=self.choose_source_file,
            ).pack(side="left", padx=(6, 0))
            self.scan_button = ttk.Button(
                buttons,
                text="Scan Library",
                command=self.scan_library,
            )
            self.scan_button.pack(side="left", padx=(6, 0))
            ttk.Button(
                buttons,
                text="Remove Folder",
                command=self.remove_selected_root,
            ).pack(side="left", padx=(6, 0))
            self.export_button = ttk.Button(
                buttons,
                text="Export Index...",
                command=self.export_index,
                state="disabled",
            )
            self.export_button.pack(side="right")

            self.library_panes = tk.PanedWindow(
                panel,
                orient=tk.VERTICAL,
                borderwidth=0,
                relief="flat",
                sashwidth=8,
                sashrelief="groove",
                showhandle=True,
                handlesize=10,
                handlepad=12,
            )
            self.library_panes.grid(row=1, column=0, sticky="nsew")

            roots_pane = ttk.Frame(self.library_panes)
            roots_pane.columnconfigure(0, weight=1)
            roots_pane.rowconfigure(1, weight=1)
            ttk.Label(
                roots_pane,
                text="Library folders (drag the divider below to resize)",
                style="Subtle.TLabel",
            ).grid(row=0, column=0, sticky="w", pady=(0, 4))

            root_frame = ttk.Frame(roots_pane)
            root_frame.grid(row=1, column=0, sticky="nsew")
            root_frame.columnconfigure(0, weight=1)
            root_frame.rowconfigure(0, weight=1)
            self.root_list = tk.Listbox(
                root_frame,
                height=3,
                exportselection=False,
                font=("Consolas", 9),
            )
            self.root_list.grid(row=0, column=0, sticky="nsew")
            root_scroll = ttk.Scrollbar(
                root_frame,
                orient="vertical",
                command=self.root_list.yview,
            )
            root_scroll.grid(row=0, column=1, sticky="ns")
            root_x = ttk.Scrollbar(
                root_frame,
                orient="horizontal",
                command=self.root_list.xview,
            )
            root_x.grid(row=1, column=0, sticky="ew")
            self.root_list.configure(
                yscrollcommand=root_scroll.set,
                xscrollcommand=root_x.set,
            )

            results_pane = ttk.Frame(self.library_panes)
            results_pane.columnconfigure(0, weight=1)
            results_pane.rowconfigure(1, weight=1)
            ttk.Label(
                results_pane,
                text="Distinct save contents (select a row to assign it)",
                style="Subtle.TLabel",
            ).grid(row=0, column=0, sticky="w", pady=(4, 4))

            tree_frame = ttk.Frame(results_pane)
            tree_frame.grid(row=1, column=0, sticky="nsew")
            tree_frame.columnconfigure(0, weight=1)
            tree_frame.rowconfigure(0, weight=1)
            columns = ("id", "status", "copies", "modified", "slots")
            self.save_tree = ttk.Treeview(
                tree_frame,
                columns=columns,
                show="headings",
                selectmode="browse",
            )
            self.save_tree.heading("id", text="Save ID")
            self.save_tree.heading("status", text="Status")
            self.save_tree.heading("copies", text="Copies")
            self.save_tree.heading("modified", text="Newest")
            self.save_tree.heading("slots", text="Slots / selectable world nodes")
            self.save_tree.column("id", width=105, minwidth=95, stretch=False)
            self.save_tree.column("status", width=72, minwidth=65, stretch=False)
            self.save_tree.column("copies", width=58, minwidth=52, stretch=False, anchor="center")
            self.save_tree.column("modified", width=138, minwidth=125, stretch=False)
            self.save_tree.column("slots", width=390, minwidth=230, stretch=True)
            self.save_tree.grid(row=0, column=0, sticky="nsew")
            self.save_tree.tag_configure("invalid", foreground="#9b2c2c")
            self.save_tree.bind("<<TreeviewSelect>>", self.on_save_selected)
            tree_y = ttk.Scrollbar(
                tree_frame,
                orient="vertical",
                command=self.save_tree.yview,
            )
            tree_y.grid(row=0, column=1, sticky="ns")
            tree_x = ttk.Scrollbar(
                tree_frame,
                orient="horizontal",
                command=self.save_tree.xview,
            )
            tree_x.grid(row=1, column=0, sticky="ew")
            self.save_tree.configure(yscrollcommand=tree_y.set, xscrollcommand=tree_x.set)

            self.library_panes.add(
                roots_pane,
                minsize=78,
                height=112,
                stretch="never",
            )
            self.library_panes.add(results_pane, minsize=180, stretch="always")

        def _build_assignment_panel(self, parent: Any) -> None:
            panel = ttk.Frame(parent)
            panel.grid(row=0, column=1, sticky="nsew", padx=(8, 0))
            panel.columnconfigure(0, weight=1)

            source = ttk.LabelFrame(panel, text="2. Selected Battery Save", padding=10)
            source.grid(row=0, column=0, sticky="ew")
            source.columnconfigure(0, weight=1)
            self._wrapped_label(
                source,
                textvariable=self.source_var,
                style="Path.TLabel",
            ).grid(row=0, column=0, sticky="ew")
            self._wrapped_label(
                source,
                textvariable=self.source_details_var,
            ).grid(row=1, column=0, sticky="ew", pady=(7, 0))

            rom = ttk.LabelFrame(panel, text="3. Patched ROM", padding=10)
            rom.grid(row=1, column=0, sticky="ew", pady=(10, 0))
            rom.columnconfigure(0, weight=1)
            rom_row = ttk.Frame(rom)
            rom_row.grid(row=0, column=0, sticky="ew")
            rom_row.columnconfigure(0, weight=1)
            ttk.Entry(
                rom_row,
                textvariable=self.rom_var,
                state="readonly",
            ).grid(row=0, column=0, sticky="ew")
            ttk.Button(
                rom_row,
                text="Choose ROM...",
                command=self.choose_rom,
            ).grid(row=0, column=1, padx=(6, 0))
            self._wrapped_label(
                rom,
                textvariable=self.rom_hash_var,
                style="Path.TLabel",
            ).grid(row=1, column=0, sticky="ew", pady=(6, 0))

            destination = ttk.LabelFrame(
                panel,
                text="4. Project64 Destination",
                padding=10,
            )
            destination.grid(row=2, column=0, sticky="ew", pady=(10, 0))
            destination.columnconfigure(0, weight=1)
            self._wrapped_label(
                destination,
                text=(
                    "Save root (editable; it may be a new path). Choose a Project64 "
                    "folder to append Save automatically."
                ),
                style="Subtle.TLabel",
            ).grid(row=0, column=0, sticky="ew", columnspan=2)
            ttk.Entry(
                destination,
                textvariable=self.save_root_var,
            ).grid(row=1, column=0, sticky="ew", pady=(7, 0), columnspan=2)
            destination_buttons = ttk.Frame(destination)
            destination_buttons.grid(row=2, column=0, sticky="ew", pady=(7, 0), columnspan=2)
            ttk.Button(
                destination_buttons,
                text="Choose Save Folder...",
                command=self.choose_save_root,
            ).pack(side="left")
            ttk.Button(
                destination_buttons,
                text="Choose Project64 Folder...",
                command=self.choose_project64_root,
            ).pack(side="left", padx=(6, 0))
            ttk.Label(
                destination,
                text="Expected target:",
                style="Subtle.TLabel",
            ).grid(row=3, column=0, sticky="w", pady=(9, 2), columnspan=2)
            self._wrapped_label(
                destination,
                textvariable=self.target_var,
                style="Path.TLabel",
            ).grid(row=4, column=0, sticky="ew", columnspan=2)

            actions = ttk.LabelFrame(panel, text="5. Create or Assign", padding=10)
            actions.grid(row=3, column=0, sticky="ew", pady=(10, 0))
            actions.columnconfigure(0, weight=1)
            actions.columnconfigure(1, weight=1)
            self.prepare_button = ttk.Button(
                actions,
                text="Create ROM Save Directory",
                command=self.create_directory,
                state="disabled",
            )
            self.prepare_button.grid(row=0, column=0, sticky="ew", padx=(0, 4))
            self.assign_button = ttk.Button(
                actions,
                text="Assign Selected Save",
                command=self.assign_selected_save,
                state="disabled",
                style="Primary.TButton",
            )
            self.assign_button.grid(row=0, column=1, sticky="ew", padx=(4, 0))
            self._wrapped_label(
                actions,
                text=(
                    "Assignment creates missing folders automatically. If a different "
                    "save exists, replacement requires confirmation and creates a "
                    "verified sibling backup."
                ),
                style="Subtle.TLabel",
            ).grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 0))

        def _append_root(self, path: Path) -> bool:
            resolved = path.expanduser().resolve()
            if not resolved.is_dir():
                return False
            key = os.path.normcase(str(resolved))
            if any(os.path.normcase(str(existing)) == key for existing in self.save_roots):
                return False
            self.save_roots.append(resolved)
            self.root_list.insert("end", str(resolved))
            return True

        def choose_library_folder(self) -> None:
            selected = filedialog.askdirectory(
                parent=self.root,
                title="Choose a Project64 Save folder or any parent directory",
                mustexist=True,
            )
            if not selected:
                return
            if self._append_root(Path(selected)):
                self.status_var.set(f"Added library tree: {Path(selected).resolve()}")
                self.scan_library()
            else:
                self.status_var.set("That library folder is already listed.")

        def remove_selected_root(self) -> None:
            selected = list(self.root_list.curselection())
            if not selected:
                return
            for index in reversed(selected):
                del self.save_roots[index]
                self.root_list.delete(index)
            self.status_var.set("Removed the selected library folder. Scan to refresh results.")

        def choose_source_file(self) -> None:
            selected = filedialog.askopenfilename(
                parent=self.root,
                title="Choose an Ogre Battle 64 Project64 battery save",
                filetypes=[
                    ("Project64 battery save", "*.sra"),
                    ("All files", "*.*"),
                ],
            )
            if not selected:
                return
            try:
                group = standalone_save_group(Path(selected))
                if not group["validOb64BatterySave"]:
                    raise BatterySaveError(
                        "the selected file has no checksum-valid Ogre Battle 64 save slot"
                    )
            except (BatterySaveError, OSError) as error:
                self._show_error("Cannot use battery save", error)
                return
            item = f"direct-{uuid.uuid4().hex}"
            self.groups_by_item[item] = group
            self._insert_group(item, group, at_start=True)
            self.save_tree.selection_set(item)
            self.save_tree.focus(item)
            self.save_tree.see(item)
            self.on_save_selected()
            self.status_var.set(f"Selected battery save: {Path(selected).resolve()}")

        def scan_library(self) -> None:
            if self.busy:
                return
            roots = list(self.save_roots)
            if not roots:
                messagebox.showinfo(
                    "No library folders",
                    "Add a folder to scan, or choose an individual .sra file.",
                    parent=self.root,
                )
                return
            self._set_busy(True, f"Scanning {len(roots)} library tree(s)...")

            def worker() -> None:
                try:
                    index = build_index(roots)
                except Exception as error:  # forwarded to the UI thread
                    self._post_to_ui(self._scan_failed, error)
                    return
                self._post_to_ui(self._scan_finished, index)

            threading.Thread(target=worker, daemon=True).start()

        def _post_to_ui(self, callback: Any, *values: Any) -> None:
            try:
                self.root.after(0, callback, *values)
            except (RuntimeError, tk.TclError):
                pass

        def _scan_failed(self, error: Exception) -> None:
            self._set_busy(False)
            self._show_error("Save library scan failed", error)

        def _scan_finished(self, index: dict[str, Any]) -> None:
            self.index = index
            self._populate_index(index)
            self._set_busy(False)
            self.export_button.configure(state="normal")
            self.status_var.set(
                f"Indexed {index['locationCount']} locations as "
                f"{index['distinctSaveCount']} distinct byte sets."
            )

        def _populate_index(self, index: dict[str, Any]) -> None:
            existing_items = self.save_tree.get_children()
            if existing_items:
                self.save_tree.delete(*existing_items)
            self.groups_by_item.clear()
            self.selected_group = None
            self.selected_source = None
            self.source_var.set("No battery save selected")
            self.source_details_var.set(
                "Select a checksum-valid row, or choose an individual .sra file."
            )
            first_valid: str | None = None
            for number, group in enumerate(index["saves"]):
                item = f"index-{number}"
                self.groups_by_item[item] = group
                self._insert_group(item, group)
                if first_valid is None and group["validOb64BatterySave"]:
                    first_valid = item
            if first_valid:
                self.save_tree.selection_set(first_valid)
                self.save_tree.focus(first_valid)
                self.save_tree.see(first_valid)
                self.on_save_selected()
            self._update_actions()

        def _insert_group(
            self,
            item: str,
            group: dict[str, Any],
            *,
            at_start: bool = False,
        ) -> None:
            valid = group["validOb64BatterySave"]
            modified = group["latestMtimeUtc"].replace("T", " ")[:19]
            self.save_tree.insert(
                "",
                0 if at_start else "end",
                iid=item,
                values=(
                    group["id"],
                    "Valid" if valid else "Invalid",
                    group["copyCount"],
                    modified,
                    group["slotSummary"],
                ),
                tags=() if valid else ("invalid",),
            )

        def on_save_selected(self, _event: Any = None) -> None:
            selected = self.save_tree.selection()
            if not selected:
                return
            group = self.groups_by_item.get(selected[0])
            if not group:
                return
            self.selected_group = group
            if not group["validOb64BatterySave"]:
                self.selected_source = None
                self.source_var.set("Invalid battery save selected")
                self.source_details_var.set(group["slotSummary"])
                self._update_actions()
                return
            try:
                source = source_from_group(group)
            except (BatterySaveError, OSError) as error:
                self.selected_source = None
                self._show_error("Selected source is unavailable", error)
                self._update_actions()
                return
            self.selected_source = source
            self.source_var.set(str(source))
            self.source_details_var.set(
                f"{group['slotSummary']}\n"
                f"SHA-256: {group['sha256']}\n"
                f"Physical copies indexed: {group['copyCount']}"
            )
            self._update_actions()

        def export_index(self) -> None:
            if not self.index:
                return
            selected = filedialog.asksaveasfilename(
                parent=self.root,
                title="Export battery-save index",
                defaultextension=".json",
                initialfile="pj64-battery-save-index.json",
                filetypes=[("JSON", "*.json"), ("All files", "*.*")],
            )
            if not selected:
                return
            try:
                write_json(Path(selected).expanduser().resolve(), self.index)
            except OSError as error:
                self._show_error("Could not export index", error)
                return
            self.status_var.set(f"Index exported: {Path(selected).resolve()}")

        def choose_rom(self) -> None:
            selected = filedialog.askopenfilename(
                parent=self.root,
                title="Choose the patched Ogre Battle 64 ROM",
                filetypes=[
                    ("Nintendo 64 ROM", "*.v64 *.z64 *.n64"),
                    ("All files", "*.*"),
                ],
            )
            if not selected:
                return
            try:
                metadata = rom_identity(Path(selected))
            except (BatterySaveError, OSError) as error:
                self._show_error("Cannot use ROM", error)
                return
            self.rom_meta = metadata
            self.rom_var.set(metadata["path"])
            self.rom_hash_var.set(f"Project64 save hash: {metadata['pj64SaveHash']}")
            self.status_var.set(f"Selected ROM: {metadata['path']}")
            self._update_target()

        def choose_save_root(self) -> None:
            selected = filedialog.askdirectory(
                parent=self.root,
                title="Choose the Project64 Save directory",
                mustexist=True,
            )
            if selected:
                self.save_root_var.set(str(Path(selected).resolve()))

        def choose_project64_root(self) -> None:
            selected = filedialog.askdirectory(
                parent=self.root,
                title="Choose the Project64 installation or build directory",
                mustexist=True,
            )
            if selected:
                self.save_root_var.set(str(Path(selected).resolve() / "Save"))

        def _current_save_root(self) -> Path:
            value = self.save_root_var.get().strip().strip('"')
            if not value:
                raise BatterySaveError("choose or enter a destination Save root")
            return Path(value).expanduser().resolve()

        def _update_target(self) -> None:
            if not self.rom_meta:
                self.target_var.set("Select a ROM and destination Save root.")
                self._update_actions()
                return
            value = self.save_root_var.get().strip().strip('"')
            if not value:
                self.target_var.set("Choose or enter a destination Save root.")
                self._update_actions()
                return
            root = Path(value).expanduser().resolve()
            destination = (
                root
                / f"{PREFIX}-{self.rom_meta['pj64SaveHash']}"
                / SAVE_FILENAME
            )
            self.target_var.set(str(destination))
            self._update_actions()

        def _update_actions(self) -> None:
            ready_target = bool(self.rom_meta and self.save_root_var.get().strip())
            prepare_state = "normal" if ready_target and not self.busy else "disabled"
            assign_ready = ready_target and self.selected_source is not None and not self.busy
            self.prepare_button.configure(state=prepare_state)
            self.assign_button.configure(state="normal" if assign_ready else "disabled")

        def create_directory(self) -> None:
            if not self.rom_meta:
                return
            try:
                save_root = self._current_save_root()
                preview = prepare_save_directory(
                    rom=Path(self.rom_meta["path"]),
                    save_root=save_root,
                    apply=False,
                )
            except (BatterySaveError, OSError) as error:
                self._show_error("Cannot prepare save directory", error)
                return
            if preview["action"] == "already-exists":
                messagebox.showinfo(
                    "Save directory already exists",
                    preview["directory"],
                    parent=self.root,
                )
                self.status_var.set(f"Directory already exists: {preview['directory']}")
                return
            if not messagebox.askyesno(
                "Create ROM save directory",
                f"Create this directory?\n\n{preview['directory']}",
                parent=self.root,
            ):
                return
            try:
                result = prepare_save_directory(
                    rom=Path(self.rom_meta["path"]),
                    save_root=save_root,
                    apply=True,
                )
            except (BatterySaveError, OSError) as error:
                self._show_error("Could not create save directory", error)
                return
            messagebox.showinfo(
                "Save directory ready",
                result["directory"],
                parent=self.root,
            )
            self.status_var.set(f"Save directory ready: {result['directory']}")

        def assign_selected_save(self) -> None:
            if not self.selected_source or not self.rom_meta:
                return
            try:
                save_root = self._current_save_root()
                preview = assign_save(
                    source=self.selected_source,
                    rom=Path(self.rom_meta["path"]),
                    save_root=save_root,
                    apply=False,
                    replace=True,
                    allow_invalid=False,
                    allow_running=False,
                    command_line=[str(Path(__file__).resolve()), "gui"],
                )
            except (BatterySaveError, OSError) as error:
                self._show_error("Cannot preview assignment", error)
                return

            action = preview["action"]
            if action == "replace":
                prompt = (
                    "A different battery save already exists at the destination.\n\n"
                    f"{preview['destination']['path']}\n\n"
                    "Replace it? The existing file will first be copied to a "
                    "verified, uniquely named sibling backup."
                )
                if not messagebox.askyesno(
                    "Replace existing battery save",
                    prompt,
                    icon="warning",
                    parent=self.root,
                ):
                    return
                replace = True
            elif action == "already-identical":
                if not messagebox.askyesno(
                    "Battery save already assigned",
                    (
                        "The destination already contains identical bytes. "
                        "Verify it and write an assignment manifest anyway?"
                    ),
                    parent=self.root,
                ):
                    return
                replace = False
            else:
                prompt = (
                    f"Copy this battery save?\n\nSource:\n{preview['source']['path']}\n\n"
                    f"Destination:\n{preview['destination']['path']}\n\n"
                    f"Slots:\n{_slot_summary(preview['source']['slots'])}"
                )
                if not messagebox.askyesno(
                    "Assign battery save",
                    prompt,
                    parent=self.root,
                ):
                    return
                replace = False

            self._set_busy(True, "Assigning and verifying battery save...")
            try:
                result = assign_save(
                    source=self.selected_source,
                    rom=Path(self.rom_meta["path"]),
                    save_root=save_root,
                    apply=True,
                    replace=replace,
                    allow_invalid=False,
                    allow_running=False,
                    command_line=[str(Path(__file__).resolve()), "gui"],
                )
            except (BatterySaveError, OSError) as error:
                self._set_busy(False)
                self._show_error("Battery-save assignment failed", error)
                return
            self._set_busy(False)
            lines = [
                "Battery save assigned and verified.",
                "",
                result["destination"]["path"],
            ]
            if result.get("backup"):
                lines.extend(["", f"Backup: {result['backup']['path']}"])
            if result.get("manifestPath"):
                lines.extend(["", f"Manifest: {result['manifestPath']}"])
            messagebox.showinfo(
                "Assignment complete",
                "\n".join(lines),
                parent=self.root,
            )
            self.status_var.set(f"Assigned and verified: {result['destination']['path']}")

        def _set_busy(self, busy: bool, message: str | None = None) -> None:
            self.busy = busy
            self.root.configure(cursor="wait" if busy else "")
            self.scan_button.configure(state="disabled" if busy else "normal")
            self.add_folder_button.configure(state="disabled" if busy else "normal")
            if message:
                self.status_var.set(message)
            self._update_actions()
            self.root.update_idletasks()

        def _show_error(self, title: str, error: Exception) -> None:
            message = str(error)
            if isinstance(error, PermissionError):
                message += (
                    "\n\nWindows denied access. Choose a user-owned destination "
                    "or run the tool from an elevated terminal."
                )
            self.status_var.set(message)
            messagebox.showerror(title, message, parent=self.root)

    BatterySaveManagerGui(window)
    window.mainloop()
    return 0


def command_gui(args: argparse.Namespace) -> int:
    return launch_gui([Path(path) for path in args.save_root])


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "GUI and CLI for indexing and safely assigning OB64 Project64 "
            ".sra battery saves (never savestates)."
        )
    )
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    parser.set_defaults(handler=command_gui, save_root=[])
    subparsers = parser.add_subparsers(dest="command")

    gui_parser = subparsers.add_parser(
        "gui",
        help="open the native desktop interface (also the default with no command)",
    )
    gui_parser.add_argument(
        "--save-root",
        action="append",
        default=[],
        help="source directory tree to add when the GUI opens (repeatable)",
    )
    gui_parser.set_defaults(handler=command_gui)

    index_parser = subparsers.add_parser(
        "index",
        help="recursively scan directory trees and write a grouped JSON index",
    )
    index_parser.add_argument(
        "--save-root",
        action="append",
        default=[],
        help="additional directory tree containing Project64 saves (repeatable)",
    )
    index_parser.add_argument(
        "--rom-dir",
        action="append",
        default=[],
        help="optional ROM file/directory to map by save hash",
    )
    index_parser.add_argument("--output", default=str(DEFAULT_INDEX), help="index JSON output")
    index_parser.add_argument(
        "--include-invalid",
        action="store_true",
        help="show invalid .sra entries in the console list",
    )
    index_parser.add_argument(
        "--show-locations",
        action="store_true",
        help="print every indexed source path beneath its grouped save",
    )
    index_parser.set_defaults(handler=command_index)

    prepare_parser = subparsers.add_parser(
        "prepare",
        help="preview or create the per-ROM Project64 save directory without copying a save",
    )
    prepare_destination = prepare_parser.add_mutually_exclusive_group(required=True)
    prepare_destination.add_argument(
        "--pj64-root", help="arbitrary Project64 directory; Save/ is appended"
    )
    prepare_destination.add_argument(
        "--save-root", help="arbitrary Project64 Save directory, existing or new"
    )
    prepare_parser.add_argument(
        "--rom", required=True, help="ROM whose Project64 save directory is required"
    )
    prepare_parser.add_argument(
        "--apply", action="store_true", help="create the directory; default is preview only"
    )
    prepare_parser.set_defaults(handler=command_prepare)

    assign_parser = subparsers.add_parser(
        "assign", help="preview or perform one battery-save assignment"
    )
    source_group = assign_parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--sha", help="unique SHA-256 prefix from the index")
    source_group.add_argument("--save", help="explicit source .sra path")
    destination_group = assign_parser.add_mutually_exclusive_group(required=True)
    destination_group.add_argument(
        "--pj64-root", help="arbitrary Project64 directory; Save/ is appended"
    )
    destination_group.add_argument(
        "--save-root", help="arbitrary Project64 Save directory, existing or new"
    )
    assign_parser.add_argument("--rom", required=True, help="patched ROM receiving the battery save")
    assign_parser.add_argument("--index", default=str(DEFAULT_INDEX), help="index used with --sha")
    assign_parser.add_argument("--manifest", help="custom assignment manifest path")
    assign_parser.add_argument("--apply", action="store_true", help="perform the copy; default is preview only")
    assign_parser.add_argument(
        "--replace",
        action="store_true",
        help="back up and replace a different destination save",
    )
    assign_parser.add_argument(
        "--allow-invalid",
        action="store_true",
        help="allow a non-valid source for forensic use",
    )
    assign_parser.add_argument(
        "--allow-running",
        action="store_true",
        help="allow assignment while Project64.exe is running",
    )
    assign_parser.set_defaults(handler=command_assign)

    interactive = subparsers.add_parser(
        "interactive", help="scan, choose a battery save, and assign it interactively"
    )
    interactive.add_argument("--save", help="select an explicit source .sra instead of scanning an index")
    interactive.add_argument("--rom", help="patched ROM path; prompted when omitted")
    interactive_destination = interactive.add_mutually_exclusive_group()
    interactive_destination.add_argument(
        "--pj64-root", help="arbitrary Project64 directory; Save/ is appended"
    )
    interactive_destination.add_argument(
        "--save-root", help="arbitrary Project64 Save directory, existing or new"
    )
    interactive.add_argument(
        "--extra-save-root",
        action="append",
        default=[],
        help="additional source directory tree (repeatable)",
    )
    interactive.add_argument("--index", default=str(DEFAULT_INDEX), help="index JSON output")
    interactive.add_argument("--replace", action="store_true", help="back up and replace a different destination save")
    interactive.add_argument(
        "--allow-running",
        action="store_true",
        help="allow assignment while Project64.exe is running",
    )
    interactive.set_defaults(handler=command_interactive)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.handler(args)
    except (BatterySaveError, OSError, json.JSONDecodeError) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
