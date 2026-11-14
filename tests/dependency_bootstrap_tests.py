"""Offline behavioral tests for the local solver dependency bootstrap."""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "solver_bootstrap", Path(__file__).resolve().parents[1] / "tools/build_solver_dependencies.py"
)
bootstrap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bootstrap)


class BootstrapTests(unittest.TestCase):
    def test_archive_checksum_is_required_before_use(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "source.tar.gz"
            archive.write_bytes(b"source archive bytes")
            expected = hashlib.sha256(archive.read_bytes()).hexdigest()
            bootstrap.verify_archive(archive, expected)
            archive.write_bytes(b"changed archive bytes")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                bootstrap.verify_archive(archive, expected)
            with self.assertRaisesRegex(FileNotFoundError, "--download"):
                bootstrap.verify_archive(Path(directory) / "missing.tar.gz", expected)

    def archive(self, directory, entries):
        path = Path(directory) / "source.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            for name, content, kind in entries:
                info = tarfile.TarInfo(name)
                info.type = kind
                info.linkname = "../../escape"
                info.size = len(content) if kind == tarfile.REGTYPE else 0
                info.mode = 0o755 if name.endswith("configure") else 0o644
                archive.addfile(info, io.BytesIO(content) if info.size else None)
        return path, hashlib.sha256(path.read_bytes()).hexdigest()

    def test_verified_archive_extracts_with_its_top_level_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            archive, digest = self.archive(directory, [
                ("upstream-1/README", b"source text", tarfile.REGTYPE),
                ("upstream-1/bin/configure", b"#!/bin/sh\n", tarfile.REGTYPE),
            ])
            target = Path(directory) / "source"
            root = bootstrap.extract_archive(archive, target, digest)
            self.assertEqual(root, target / "upstream-1")
            self.assertEqual((root / "README").read_bytes(), b"source text")
            self.assertTrue((root / "bin/configure").stat().st_mode & 0o111)

    def test_unsafe_archives_are_rejected_without_partial_extraction(self):
        for name, kind in [
            ("../escape", tarfile.REGTYPE),
            ("upstream/../../escape", tarfile.REGTYPE),
            ("/absolute/escape", tarfile.REGTYPE),
            ("upstream/link", tarfile.SYMTYPE),
            ("upstream/link", tarfile.LNKTYPE),
            ("upstream/device", tarfile.CHRTYPE),
            ("upstream/../escape", tarfile.REGTYPE),
            ("upstream\\escape", tarfile.REGTYPE),
        ]:
            with self.subTest(name=name, kind=kind), tempfile.TemporaryDirectory() as directory:
                archive, digest = self.archive(directory, [
                    ("upstream/safe", b"safe", tarfile.REGTYPE),
                    (name, b"bad", kind),
                ])
                target = Path(directory) / "source"
                with self.assertRaisesRegex(ValueError, "Unsafe archive"):
                    bootstrap.extract_archive(archive, target, digest)
                self.assertFalse(target.exists())
                self.assertFalse((Path(directory) / "escape").exists())

    def test_bad_checksum_never_creates_an_extraction_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            archive, _ = self.archive(directory, [("upstream/safe", b"safe", tarfile.REGTYPE)])
            target = Path(directory) / "source"
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                bootstrap.extract_archive(archive, target, "0" * 64)
            self.assertFalse(target.exists())

    def test_missing_archives_never_trigger_an_implicit_download(self):
        library = {"name": "HiGHS", "version": "1.15.1", "sha256": "0" * 64,
                   "url": "https://example.invalid/source.tar.gz"}
        with tempfile.TemporaryDirectory() as directory, patch("urllib.request.urlopen") as network:
            with self.assertRaisesRegex(FileNotFoundError, "--download"):
                bootstrap.ensure_archive(library, Path(directory), allow_download=False)
            network.assert_not_called()

    def test_explicit_download_is_verified_before_publishing(self):
        content = b"downloaded archive bytes"
        library = {"name": "HiGHS", "version": "1.15.1", "sha256": hashlib.sha256(content).hexdigest(),
                   "url": "https://example.invalid/source.tar.gz"}
        with tempfile.TemporaryDirectory() as directory, patch("urllib.request.urlopen", return_value=io.BytesIO(content)) as network:
            archive = bootstrap.ensure_archive(library, Path(directory), allow_download=True)
            self.assertEqual(archive.read_bytes(), content)
            network.assert_called_once()
        with tempfile.TemporaryDirectory() as directory, patch("urllib.request.urlopen", return_value=io.BytesIO(b"wrong bytes")):
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                bootstrap.ensure_archive(library, Path(directory), allow_download=True)
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_build_commands_stay_local_and_retain_license_sensitive_options(self):
        manifest = bootstrap.load_manifest(Path(__file__).resolve().parents[1] / "third_party/SOLVER_DEPS.json")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for library in manifest["libraries"]:
                commands = bootstrap.build_commands(library, root / "source", root / "build", root / "prefix", 2, Path("/usr/include"))
                configure, build, install = commands
                self.assertEqual(configure[:2], ["cmake", "-S"])
                self.assertIn(f"-DCMAKE_INSTALL_PREFIX={root / 'prefix'}", configure)
                self.assertEqual(build, ["cmake", "--build", str(root / "build"), "--parallel", "2"])
                self.assertEqual(install, ["cmake", "--install", str(root / "build")])
                self.assertFalse(any("sudo" in command for command in commands))
                if library["name"] == "HiGHS":
                    self.assertIn("-DBUILD_SHARED_EXTRAS_LIB=OFF", configure)
                if library["name"] == "NLopt":
                    self.assertIn("-DNLOPT_LUKSAN=OFF", configure)
                    self.assertIn("-DBUILD_SHARED_LIBS=OFF", configure)
                if library["name"] == "pagmo":
                    self.assertIn("-DPAGMO_BUILD_STATIC_LIBRARY=OFF", configure)
                    self.assertIn("-DCMAKE_INSTALL_RPATH=$ORIGIN", configure)
                    self.assertIn(f"-DBOOST_LIBRARYDIR={root / 'prefix/lib'}", configure)

    def test_prepare_preserves_verified_sources_and_license_texts(self):
        repository = Path(__file__).resolve().parents[1]
        manifest = json.loads((repository / "third_party/SOLVER_DEPS.json").read_text())
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / "work"
            downloads = work / "downloads"
            downloads.mkdir(parents=True)
            for library in manifest["libraries"]:
                archive, digest = self.archive(directory, [
                    ("upstream/LICENSE.txt", b"upstream license text", tarfile.REGTYPE),
                    ("upstream/src/probe.cpp", b"int answer(){return 42;}\n", tarfile.REGTYPE),
                ])
                library["sha256"] = digest
                (downloads / bootstrap.archive_name(library)).write_bytes(archive.read_bytes())
            manifest_path = Path(directory) / "manifest.json"
            manifest_path.write_text(json.dumps(manifest))
            prefix = work / "rebuild-prefix"
            projects = bootstrap.prepare_dependencies(manifest_path, work, allow_download=False, install_prefix=prefix)
            self.assertEqual(len(projects), 5)
            output = prefix / "share/tiger-sheets"
            self.assertFalse((work / "prefix").exists())
            preserved = json.loads((output / "sources/SOLVER_DEPS.json").read_text())
            self.assertEqual(preserved, manifest)
            for library in manifest["libraries"]:
                bootstrap.verify_archive(output / "sources" / bootstrap.archive_name(library), library["sha256"])
            for project in projects:
                self.assertEqual((output / "licenses/solvers" / project["slug"] / "LICENSE.txt").read_bytes(), b"upstream license text")
            self.assertTrue((output / "sources/build_solver_dependencies.py").is_file())

    def test_generated_commands_really_build_and_install_a_local_library(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(BootstrapProbe LANGUAGES CXX)\n"
                "add_library(probe SHARED probe.cpp)\ninstall(TARGETS probe LIBRARY DESTINATION lib)\n")
            (source / "probe.cpp").write_text("int answer(){return 42;}\n")
            commands = bootstrap.build_commands({"name": "HiGHS"}, source, root / "build", root / "prefix", 1, Path("/usr/include"))
            for command in commands:
                subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertTrue((root / "prefix/lib/libprobe.so").is_file())

    def test_cli_offline_verification_reports_missing_archive_and_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(["python3", str(SPEC.origin), "--work-dir", directory, "--verify-only"], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("--download", result.stderr)
            self.assertFalse((Path(directory) / "sources").exists())


if __name__ == "__main__":
    unittest.main()
