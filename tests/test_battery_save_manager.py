"""Battery manager restart, loading and copy workflows; temporary files only."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import threading
import time
import tkinter as tk
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "supplemental-tools/project64-battery-save-manager/pj64_battery_save_manager.py"
SPEC = importlib.util.spec_from_file_location("battery_manager", SCRIPT)
manager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(manager)


def save_bytes(marker=1):
    native = bytearray(32768)
    base = 16
    native[base + 4:base + 12] = b"QuestOG3"
    native[base + 12] = marker
    payload = native[base + 12:base + 12 + 0x1844]
    native[base:base + 2] = ((base + sum(payload)) & 65535).to_bytes(2, "big")
    bits = sum(bin(v).count("1") for v in payload)
    native[base + 2:base + 4] = ((base + bits) & 65535).to_bytes(2, "big")
    raw = bytearray(len(native))
    for lane in range(4):
        raw[lane::4] = native[3-lane::4]
    return bytes(raw)


def put_save(root, number, marker=1):
    path = root / f"OgreBattle64-{number:032X}" / "OgreBattle64.sra"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(save_bytes(marker))
    return path


class BatteryManagerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def test_duplicate_decoding_and_incremental_refresh(self):
        root = self.work / "Save"
        first = put_save(root, 1)
        second = put_save(root, 2)
        changed = put_save(root, 3, 2)
        with patch.object(manager, "pj64_to_native_saveram", wraps=manager.pj64_to_native_saveram) as decode:
            cold = manager.build_index([root, root / first.parent.name])
            self.assertEqual(decode.call_count, 2)  # Three locations, two distinct byte sets.
        self.assertEqual(cold["locationCount"], 3)
        self.assertEqual(cold["distinctSaveCount"], 2)
        self.assertTrue(all(g["validOb64BatterySave"] for g in cold["saves"]))
        with patch.object(manager, "inspect_sra", side_effect=AssertionError("unchanged save reread")):
            warm = manager.build_index([root], previous=cold)
        self.assertEqual(warm["saves"], cold["saves"])
        previous_time = changed.stat().st_mtime_ns
        changed.write_bytes(save_bytes(3))
        os.utime(changed, ns=(previous_time + 1_000_000_000, previous_time + 1_000_000_000))
        second.unlink()
        with patch.object(manager, "inspect_sra", wraps=manager.inspect_sra) as inspect:
            updated = manager.build_index([root], previous=warm)
            self.assertEqual(inspect.call_count, 1)
        self.assertEqual(updated["locationCount"], 2)
        self.assertIn(hashlib.sha256(save_bytes(3)).hexdigest().upper(), [g["sha256"] for g in updated["saves"]])

    def make_app(self, roots, settings=None):
        root = tk.Tk()
        root.withdraw()
        def cleanup():
            try:
                root.destroy()
            except tk.TclError:
                pass  # A restart check already closed this window.
        self.addCleanup(cleanup)
        settings_path = self.work / "settings.json"
        if settings is not None:
            manager.write_json(settings_path, settings)
        with patch.object(manager, "discover_save_roots", return_value=roots):
            app = manager.create_gui(root, settings_path=settings_path, index_path=self.work / "cache.json")
        return root, app

    def wait_for(self, root, predicate):
        deadline = time.monotonic() + 5
        while not predicate() and time.monotonic() < deadline:
            root.update()
            time.sleep(0.005)
        self.assertTrue(predicate(), "Background operation did not complete")

    def close_app(self, root, app):
        # Cancel test-window callbacks before releasing its Tcl interpreter.
        for token in root.tk.call("after", "info"):
            root.after_cancel(token)
        app.settings_timer = None
        app.close()

    def test_remove_folders_and_cached_library_survive_restart(self):
        a, b = self.work / "A", self.work / "B"
        put_save(a, 1)
        changed = put_save(b, 2, 2)
        with patch("tkinter.messagebox.showerror") as error:
            root, app = self.make_app([a, b])
            self.wait_for(root, lambda: app.index is not None and not app.scanning)
            app.root_list.selection_set(0)
            app.remove_selected_root()
            self.wait_for(root, lambda: not app.scanning)
            self.assertEqual(app.save_roots, [b])
            self.assertEqual(app.index["locationCount"], 1)
            self.close_app(root, app)
            previous_time = changed.stat().st_mtime_ns
            changed.write_bytes(save_bytes(3))
            os.utime(changed, ns=(previous_time + 1_000_000_000, previous_time + 1_000_000_000))
            root2, app2 = self.make_app([a, b])
            self.assertEqual(app2.save_roots, [b])
            self.assertEqual(len(app2.save_tree.get_children()), 1)  # Visible before refresh runs.
            self.assertIsNone(app2.selected_source)  # Stale cached entry cannot be assigned.
            self.wait_for(root2, lambda: app2.selected_source is not None and not app2.scanning)
            app2.root_list.selection_set(0)
            app2.remove_selected_root()
            self.assertEqual(app2.index["locationCount"], 0)
            self.close_app(root2, app2)
            root3, app3 = self.make_app([a, b])
            self.assertEqual(app3.save_roots, [])
            self.assertEqual(app3.save_tree.get_children(), ())
            self.close_app(root3, app3)
            error.assert_not_called()

    def test_assignment_clicks_and_remembered_choices(self):
        library = self.work / "Save"
        source = put_save(library, 1)
        rom = self.work / "game.n64"
        rom.write_bytes(bytes.fromhex("40123780") + bytes(4092))
        destination = self.work / "Destination"
        settings = {"schemaVersion": 1, "saveRoots": [str(library)], "rom": str(rom), "destinationRoot": str(destination)}
        main_thread = threading.get_ident()
        identity = manager.rom_identity
        def background_identity(path):
            self.assertNotEqual(threading.get_ident(), main_thread)
            return identity(path)
        with patch.object(manager, "rom_identity", side_effect=background_identity), \
             patch.object(manager, "project64_running", return_value=False), \
             patch.object(manager, "DEFAULT_ASSIGNMENT_DIR", self.work / "manifests"), \
             patch("tkinter.messagebox.askyesno", return_value=True) as question, \
             patch("tkinter.messagebox.showinfo") as info, \
             patch("tkinter.messagebox.showerror") as error:
            root, app = self.make_app([], settings)
            self.wait_for(root, lambda: app.index is not None and not app.scanning and app.rom_meta is not None)
            app.assign_button.invoke()
            self.wait_for(root, lambda: not app.busy)
            target = Path(app.target_var.get())
            self.assertEqual(target.read_bytes(), source.read_bytes())
            question.assert_not_called()
            info.assert_not_called()
            app.assign_button.invoke()  # Identical assignment is an inline no-op.
            self.wait_for(root, lambda: not app.busy)
            question.assert_not_called()
            target.write_bytes(save_bytes(2))
            app.assign_button.invoke()
            self.wait_for(root, lambda: not app.busy)
            self.assertEqual(question.call_count, 1)
            self.assertEqual(target.read_bytes(), save_bytes())
            self.assertEqual(source.read_bytes(), save_bytes())
            backups = [p for p in target.parent.iterdir() if p != target and p.is_file()]
            self.assertEqual(len(backups), 1)
            self.assertEqual(backups[0].read_bytes(), save_bytes(2))
            self.close_app(root, app)
            root2, app2 = self.make_app([])
            self.assertEqual(app2.save_root_var.get(), str(destination))
            self.wait_for(root2, lambda: app2.rom_meta is not None and not app2.scanning)
            self.assertEqual(app2.rom_meta["path"], str(rom.resolve()))
            self.close_app(root2, app2)
            error.assert_not_called()

    def test_assignment_revalidates_cached_source_and_confirmed_destination(self):
        source = put_save(self.work / "Source", 1)
        rom = self.work / "game.n64"
        rom.write_bytes(bytes.fromhex("40123780") + bytes(4092))
        request = dict(source=source, rom=rom, save_root=self.work / "Output", apply=True,
                       replace=False, allow_invalid=False, allow_running=False,
                       manifest_path=self.work / "assigned.json")
        with patch.object(manager, "project64_running", return_value=False):
            first = manager.assign_save(**request)
            target = Path(first["destination"]["path"])
            selected_hash = first["source"]["sha256"]
            source.write_bytes(save_bytes(2))
            with self.assertRaises(manager.BatterySaveError):
                manager.assign_save(**request, expected_source_sha256=selected_hash)
            target.write_bytes(save_bytes(3))
            request["replace"] = True
            with self.assertRaises(manager.BatterySaveError):
                manager.assign_save(**request, expected_destination_sha256=selected_hash)
            self.assertEqual(target.read_bytes(), save_bytes(3))
            self.assertEqual(len(list(target.parent.iterdir())), 1)  # Refusal made no backup/write.
            expected_rom = first["rom"]["sha256"]
            rom.write_bytes(bytes.fromhex("40123780") + bytes([1]) * 4092)
            with self.assertRaises(manager.BatterySaveError):
                manager.assign_save(**request, expected_rom_sha256=expected_rom)
            with patch.object(manager, "project64_running", return_value=True):
                with self.assertRaises(manager.BatterySaveError):
                    manager.assign_save(**request)
            self.assertEqual(target.read_bytes(), save_bytes(3))


if __name__ == "__main__":
    unittest.main()
