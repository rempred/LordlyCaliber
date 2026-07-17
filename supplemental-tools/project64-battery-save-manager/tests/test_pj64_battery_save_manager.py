import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


MODULE_PATH = Path(__file__).resolve().parents[1] / "pj64_battery_save_manager.py"
SPEC = importlib.util.spec_from_file_location("pj64_battery_save_manager", MODULE_PATH)
manager = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(manager)


def make_roms(directory: Path) -> tuple[Path, Path, Path]:
    directory.mkdir(parents=True, exist_ok=True)
    z64_data = bytes.fromhex("80371240 01020304 A0B0C0D0 11223344")
    v64_data = bytearray(z64_data)
    for offset in range(0, len(v64_data), 2):
        v64_data[offset], v64_data[offset + 1] = v64_data[offset + 1], v64_data[offset]
    n64_data = bytearray(z64_data)
    for offset in range(0, len(n64_data), 4):
        n64_data[offset : offset + 4] = reversed(n64_data[offset : offset + 4])
    z64 = directory / "test.z64"
    v64 = directory / "test.v64"
    n64 = directory / "test.n64"
    z64.write_bytes(z64_data)
    v64.write_bytes(v64_data)
    n64.write_bytes(n64_data)
    return z64, v64, n64


def write_packed_bits(payload: bytearray, bit_offset: int, bit_count: int, value: int) -> None:
    for index in range(bit_count):
        source_bit = (value >> (bit_count - 1 - index)) & 1
        target_bit = bit_offset + index
        mask = 1 << (7 - (target_bit & 7))
        if source_bit:
            payload[target_bit >> 3] |= mask
        else:
            payload[target_bit >> 3] &= ~mask


def make_sra(
    path: Path,
    slot_name: str = "Magnus",
    selectable_nodes: tuple[int, ...] = (),
) -> Path:
    native = bytearray(manager.SAVERAM_SIZE)
    base = manager.SLOT_BASE
    native[base + manager.MAGIC_OFFSET : base + manager.MAGIC_OFFSET + len(manager.NATIVE_MAGIC)] = (
        manager.NATIVE_MAGIC
    )
    name_bytes = slot_name.encode("ascii")[:16]
    native[base + manager.HEADER_NAME_OFFSET : base + manager.HEADER_NAME_OFFSET + len(name_bytes)] = name_bytes
    native[base + manager.CHECKSUM_REGION_OFFSET] = 1

    payload = bytearray(manager.SAVERAM_PACKED_SIZE)
    node_states = bytearray(10)
    active_nodes = bytearray(5)
    for node_id in selectable_nodes:
        zero_based = node_id - 1
        node_states[zero_based >> 2] |= 2 << ((zero_based & 3) * 2)
        active_nodes[zero_based >> 3] |= 1 << (zero_based & 7)
    for index, value in enumerate(node_states):
        write_packed_bits(
            payload,
            manager.WORLD_NODE_STATE_PACKED_BIT_OFFSET + index * 8,
            8,
            value,
        )
    for index, value in enumerate(active_nodes):
        write_packed_bits(
            payload,
            manager.WORLD_ACTIVE_NODE_PACKED_BIT_OFFSET + index * 8,
            8,
            value,
        )
    packed_start = base + manager.SAVERAM_PACKED_OFFSET
    native[packed_start : packed_start + manager.SAVERAM_PACKED_SIZE] = payload

    total, bits = manager._slot_checksums(native, base)
    native[base : base + 2] = total.to_bytes(2, "big")
    native[base + 2 : base + 4] = bits.to_bytes(2, "big")
    raw = bytearray(manager.PJ64_SRA_SIZE)
    for offset in range(0, len(raw), 4):
        raw[offset : offset + 4] = reversed(native[offset : offset + 4])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)
    return path


class RomHashTests(unittest.TestCase):
    def test_all_rom_byte_orders_produce_same_save_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            z64, v64, n64 = make_roms(Path(temporary))
            hashes = {manager.rom_identity(path)["pj64SaveHash"] for path in (z64, v64, n64)}
            self.assertEqual(1, len(hashes))


class GuiModelTests(unittest.TestCase):
    def test_gui_wrap_length_tracks_container_width_with_a_small_width_floor(self):
        self.assertEqual(372, manager.gui_wrap_length(400))
        self.assertEqual(80, manager.gui_wrap_length(50))

    def test_no_command_selects_gui_without_constructing_a_window(self):
        args = manager.build_parser().parse_args([])

        self.assertIs(manager.command_gui, args.handler)
        self.assertEqual([], args.save_root)

    def test_gui_command_accepts_initial_source_trees(self):
        args = manager.build_parser().parse_args(
            ["gui", "--save-root", r"C:\One", "--save-root", r"D:\Two"]
        )

        self.assertIs(manager.command_gui, args.handler)
        self.assertEqual([r"C:\One", r"D:\Two"], args.save_root)

    def test_direct_file_uses_the_same_group_model_as_the_index(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = make_sra(
                Path(temporary) / "picked.sra",
                "Magnus",
                selectable_nodes=(6,),
            )

            group = manager.standalone_save_group(source)

            self.assertTrue(group["validOb64BatterySave"])
            self.assertEqual(1, group["copyCount"])
            self.assertEqual(str(source.resolve()), group["locations"][0]["path"])
            self.assertIn("Tenne Plains (#6)", group["slotSummary"])


class PortablePathTests(unittest.TestCase):
    def test_windows_local_app_data_is_the_portable_default_root(self):
        local_app_data = Path(r"C:\Users\Player\AppData\Local")

        result = manager.user_data_root(
            {"LOCALAPPDATA": str(local_app_data)},
            Path(r"C:\Users\Player"),
        )

        self.assertEqual(
            local_app_data / "LordlyCaliber" / "PJ64SaveManager",
            result,
        )

    def test_xdg_data_home_is_used_when_local_app_data_is_absent(self):
        result = manager.user_data_root(
            {"XDG_DATA_HOME": "/home/player/custom-data"},
            Path("/home/player"),
        )

        self.assertEqual(
            Path("/home/player/custom-data/LordlyCaliber/PJ64SaveManager"),
            result,
        )

    def test_default_index_and_manifests_share_the_user_data_root(self):
        self.assertEqual(manager.USER_DATA_ROOT, manager.DEFAULT_INDEX.parent)
        self.assertEqual(
            manager.USER_DATA_ROOT / "assignment-manifests",
            manager.DEFAULT_ASSIGNMENT_DIR,
        )


class IndexTests(unittest.TestCase):
    def test_index_groups_duplicate_sra_and_excludes_savestates(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            save_root_a = root / "A" / "Save"
            save_root_b = root / "B" / "Save"
            folder_a = save_root_a / f"{manager.PREFIX}-{'1' * 32}"
            folder_b = save_root_b / f"{manager.PREFIX}-{'2' * 32}"
            source = make_sra(folder_a / manager.SAVE_FILENAME, "Palatinus")
            folder_b.mkdir(parents=True)
            (folder_b / manager.SAVE_FILENAME).write_bytes(source.read_bytes())
            (folder_b / "freeze.pj.zip").write_bytes(b"not a battery save")

            index = manager.build_index([save_root_a, save_root_b])

            self.assertEqual(manager.TOOL_SOURCE, index["tool"])
            self.assertEqual(2, index["locationCount"])
            self.assertEqual(1, index["distinctSaveCount"])
            self.assertEqual(2, index["saves"][0]["copyCount"])
            self.assertTrue(index["saves"][0]["validOb64BatterySave"])
            self.assertEqual("1:Palatinus [no active node]", index["saves"][0]["slotSummary"])
            self.assertNotIn("freeze.pj.zip", str(index))

    def test_index_recurses_from_an_arbitrary_parent_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            nested_save = (
                root
                / "Portable Project64"
                / "Save"
                / f"{manager.PREFIX}-{'A' * 32}"
                / manager.SAVE_FILENAME
            )
            make_sra(nested_save)

            index = manager.build_index([root])

            self.assertEqual(1, index["locationCount"])
            self.assertEqual(str(nested_save.resolve()), index["saves"][0]["locations"][0]["path"])

    def test_world_node_decode_retains_branch_choices(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = make_sra(
                Path(temporary) / "branch.sra",
                "Magnus",
                selectable_nodes=(6, 11),
            )

            metadata = manager.inspect_sra(source)
            world = metadata["slots"][0]["worldMap"]

            self.assertEqual([6, 11], [node["id"] for node in world["currentlySelectableNodes"]])
            self.assertEqual("Tenne Plains (#6), Gules Hills (#11)", world["summary"])

    def test_optional_rom_scan_joins_known_rom_to_save_group(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, rom = make_roms(root / "roms")
            save_hash = manager.rom_identity(rom)["pj64SaveHash"]
            save_root = root / "Project64" / "Save"
            make_sra(save_root / f"{manager.PREFIX}-{save_hash}" / manager.SAVE_FILENAME)

            index = manager.build_index([save_root], [root / "roms"])

            known_paths = {entry["path"] for entry in index["saves"][0]["knownRoms"]}
            self.assertEqual(3, len(known_paths))
            self.assertIn(str(rom.resolve()), known_paths)


class AssignmentTests(unittest.TestCase):
    def test_prepare_previews_then_creates_directory_under_arbitrary_save_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, rom = make_roms(root / "roms")
            save_root = root / "Any User Folder" / "Custom Saves"

            preview = manager.prepare_save_directory(rom=rom, save_root=save_root, apply=False)
            directory = Path(preview["directory"])
            self.assertFalse(directory.exists())
            self.assertTrue(preview["dryRun"])

            result = manager.prepare_save_directory(rom=rom, save_root=save_root, apply=True)
            self.assertTrue(directory.is_dir())
            self.assertFalse(result["dryRun"])
            self.assertEqual("create", result["action"])

    def test_assignment_is_preview_then_copy_with_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = make_sra(root / "source.sra", "Magnus")
            _, _, rom = make_roms(root)
            save_root = root / "Project64" / "Save"
            manifest_dir = root / "LocalAppData" / "assignment-manifests"

            preview = manager.assign_save(
                source=source,
                rom=rom,
                save_root=save_root,
                apply=False,
                replace=False,
                allow_invalid=False,
                allow_running=False,
            )
            destination = Path(preview["destination"]["path"])
            self.assertFalse(destination.exists())
            self.assertTrue(preview["dryRun"])

            with (
                patch.object(manager, "project64_running", return_value=False),
                patch.object(manager, "DEFAULT_ASSIGNMENT_DIR", manifest_dir),
            ):
                result = manager.assign_save(
                    source=source,
                    rom=rom,
                    save_root=save_root,
                    apply=True,
                    replace=False,
                    allow_invalid=False,
                    allow_running=False,
                )
            manifest_path = Path(result["manifestPath"])
            self.assertEqual(source.read_bytes(), destination.read_bytes())
            self.assertTrue(manifest_path.is_file())
            self.assertEqual(manifest_dir.resolve(), manifest_path.parent)
            self.assertEqual(manager.file_sha256(source), result["destination"]["afterSha256"])
            self.assertEqual("copy", result["action"])

    def test_different_destination_requires_replace_and_is_backed_up(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = make_sra(root / "source.sra", "Source")
            _, _, rom = make_roms(root)
            save_root = root / "Project64" / "Save"
            rom_hash = manager.rom_identity(rom)["pj64SaveHash"]
            destination = save_root / f"{manager.PREFIX}-{rom_hash}" / manager.SAVE_FILENAME
            make_sra(destination, "Destination")
            old_hash = manager.file_sha256(destination)
            fixed_time = manager.dt.datetime(2026, 7, 17, 12, 34, 56, tzinfo=manager.dt.timezone.utc)
            occupied_backup = manager.replacement_backup_path(destination, fixed_time, old_hash)
            occupied_backup.write_bytes(b"prior backup evidence")

            with self.assertRaises(manager.BatterySaveError):
                manager.assign_save(
                    source=source,
                    rom=rom,
                    save_root=save_root,
                    apply=False,
                    replace=False,
                    allow_invalid=False,
                    allow_running=False,
                )

            with (
                patch.object(manager, "project64_running", return_value=False),
                patch.object(manager, "utc_now", return_value=fixed_time),
            ):
                result = manager.assign_save(
                    source=source,
                    rom=rom,
                    save_root=save_root,
                    apply=True,
                    replace=True,
                    allow_invalid=False,
                    allow_running=False,
                    manifest_path=root / "replace.json",
                )
            backup = Path(result["backup"]["path"])
            self.assertTrue(backup.is_file())
            self.assertNotEqual(occupied_backup, backup)
            self.assertEqual(b"prior backup evidence", occupied_backup.read_bytes())
            self.assertEqual(old_hash, manager.file_sha256(backup))
            self.assertEqual(manager.file_sha256(source), manager.file_sha256(destination))
            self.assertEqual("replace", result["action"])


if __name__ == "__main__":
    unittest.main()
