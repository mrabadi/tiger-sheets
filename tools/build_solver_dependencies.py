#!/usr/bin/env python3
"""Build TigerSheets' pinned native solvers without system installation."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request


ARCHIVE_PATTERNS = {
    "HiGHS": "highs-v{version}.tar.gz",
    "NLopt": "nlopt-v{version}.tar.gz",
    "pagmo": "pagmo-v{version}.tar.gz",
    "oneTBB": "onetbb-v{version}.tar.gz",
    "Boost.Serialization": "boost-serialization-{version}.tar.gz",
}


def load_manifest(path):
    manifest = json.loads(Path(path).read_text(encoding="utf-8"))
    libraries = manifest.get("libraries", [])
    if len(libraries) != len(ARCHIVE_PATTERNS) or {item.get("name") for item in libraries} != set(ARCHIVE_PATTERNS):
        raise ValueError("Solver manifest must pin each of the five native dependencies exactly once")
    for library in libraries:
        if (not re.fullmatch(r"[0-9]+(?:\.[0-9]+){1,3}", library.get("version", ""))
                or not re.fullmatch(r"[a-f0-9]{64}", library.get("sha256", ""))
                or not library.get("url", "").startswith("https://")):
            raise ValueError(f"Invalid pinned solver dependency: {library.get('name')}")
    return manifest


def archive_name(library):
    return ARCHIVE_PATTERNS[library["name"]].format(version=library["version"])


def verify_archive(path, expected_sha256):
    """Reject missing or damaged archives before extraction or compilation."""
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Missing source archive {path}; use --download explicitly to fetch it")
    digest = hashlib.sha256()
    with path.open("rb") as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(chunk)
    if digest.hexdigest() != expected_sha256:
        raise ValueError(f"SHA-256 mismatch for {path}")


def ensure_archive(library, directory, allow_download=False):
    directory = Path(directory)
    path = directory / archive_name(library)
    if path.exists() or not allow_download:
        verify_archive(path, library["sha256"])
        return path
    directory.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".download-", dir=directory, delete=False) as output:
            temporary = Path(output.name)
            with urllib.request.urlopen(library["url"], timeout=60) as response:
                shutil.copyfileobj(response, output)
        verify_archive(temporary, library["sha256"])
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return path


def extract_archive(path, destination, expected_sha256):
    """Verify and extract one upstream root; never follow archive links."""
    verify_archive(path, expected_sha256)
    destination = Path(destination)
    if destination.exists():
        raise FileExistsError(f"Extraction destination already exists: {destination}")
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()
        seen = set()
        roots = set()
        for member in members:
            name = PurePosixPath(member.name)
            if (name.is_absolute() or ".." in name.parts or "\\" in member.name
                    or not name.parts or not (member.isfile() or member.isdir())
                    or name in seen):
                raise ValueError(f"Unsafe archive entry: {member.name}")
            seen.add(name)
            roots.add(name.parts[0])
        if len(roots) != 1:
            raise ValueError("Unsafe archive: expected exactly one upstream root directory")
        root = next(iter(roots))
        if any(member.isfile() and len(PurePosixPath(member.name).parts) < 2 for member in members):
            raise ValueError("Unsafe archive: top-level source must be a directory")
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".extract-", dir=destination.parent) as temporary:
            stage = Path(temporary) / "source"
            stage.mkdir()
            for member in members:
                target = stage / member.name
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as source, target.open("xb") as output:
                        shutil.copyfileobj(source, output)
                    target.chmod(0o755 if member.mode & 0o111 else 0o644)
            os.replace(stage, destination)
    return destination / root


def build_commands(library, source, build, prefix, jobs, boost_headers):
    source, build, prefix = Path(source), Path(build), Path(prefix)
    options = {
        "CMAKE_BUILD_TYPE": "Release", "CMAKE_INSTALL_PREFIX": str(prefix),
        "CMAKE_INSTALL_LIBDIR": "lib", "CMAKE_POSITION_INDEPENDENT_CODE": "ON",
        "CMAKE_INSTALL_RPATH": "$ORIGIN",
    }
    name = library["name"]
    if name == "HiGHS":
        options.update(BUILD_SHARED_LIBS="OFF", BUILD_SHARED_EXTRAS_LIB="OFF", BUILD_TESTING="OFF", BUILD_CXX_EXE="OFF",
                       HIGHS_NO_DEFAULT_THREADS="ON", ZLIB="OFF", FORTRAN="OFF")
    elif name == "NLopt":
        options.update(BUILD_SHARED_LIBS="OFF", NLOPT_LUKSAN="OFF", NLOPT_TESTS="OFF",
                       NLOPT_PYTHON="OFF", NLOPT_GUILE="OFF", NLOPT_MATLAB="OFF",
                       NLOPT_OCTAVE="OFF", NLOPT_SWIG="OFF", NLOPT_JAVA="OFF", NLOPT_FORTRAN="OFF")
    elif name == "oneTBB":
        options.update(TBB_TEST="OFF", TBBMALLOC_BUILD="OFF", TBB_STRICT="OFF")
    elif name == "Boost.Serialization":
        options.update(BOOST_SERIALIZATION_ROOT=str(source), BOOST_HEADERS=str(boost_headers),
                       BOOST_SERIALIZATION_VERSION=library["version"])
        source = source / "_tigersheets_build"
    elif name == "pagmo":
        options.update(PAGMO_BUILD_STATIC_LIBRARY="OFF", PAGMO_WITH_NLOPT="OFF", PAGMO_WITH_EIGEN3="OFF",
                       PAGMO_WITH_IPOPT="OFF", PAGMO_BUILD_TESTS="OFF", PAGMO_BUILD_TUTORIALS="OFF",
                       PAGMO_BUILD_BENCHMARKS="OFF", PAGMO_ENABLE_IPO="OFF", Boost_NO_BOOST_CMAKE="ON",
                       BOOST_LIBRARYDIR=str(prefix / "lib"), BOOST_INCLUDEDIR=str(boost_headers),
                       CMAKE_PREFIX_PATH=str(prefix), PAGMO_INSTALL_LIBDIR="lib", CMAKE_INSTALL_RPATH_USE_LINK_PATH="OFF")
    else:
        raise ValueError(f"Unknown solver dependency: {name}")
    for key, value in library.get("requiredBuildOptions", {}).items():
        if key in options and options[key] != value:
            raise ValueError(f"Manifest conflicts with required {name} option {key}")
        options[key] = value
    configure = ["cmake", "-S", str(source), "-B", str(build)]
    configure.extend(f"-D{key}={value}" for key, value in sorted(options.items()))
    return [configure, ["cmake", "--build", str(build), "--parallel", str(jobs)],
            ["cmake", "--install", str(build)]]


def default_manifest():
    script = Path(__file__).resolve()
    adjacent = script.parent / "SOLVER_DEPS.json"
    return adjacent if adjacent.is_file() else script.parents[1] / "third_party/SOLVER_DEPS.json"


def prepare_dependencies(manifest_path, work_dir, allow_download=False, install_prefix=None):
    """Prepare a fresh verified source tree and retain redistributable inputs."""
    manifest = load_manifest(manifest_path)
    work_dir = Path(work_dir).resolve()
    archives = [(library, ensure_archive(library, work_dir / "downloads", allow_download))
                for library in manifest["libraries"]]
    projects = []
    for library, archive in archives:
        slug = archive.name.removesuffix(".tar.gz")
        destination = work_dir / "sources" / slug
        if destination.exists():
            if destination.is_symlink():
                raise ValueError(f"Refusing source-directory symlink: {destination}")
            shutil.rmtree(destination)
        source = extract_archive(archive, destination, library["sha256"])
        projects.append({"library": library, "archive": archive, "source": source, "slug": slug})
    prefix = Path(install_prefix).resolve() if install_prefix is not None else work_dir / "prefix"
    output = prefix / "share/tiger-sheets"
    source_output = output / "sources"
    source_output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(manifest_path, source_output / "SOLVER_DEPS.json")
    shutil.copyfile(__file__, source_output / "build_solver_dependencies.py")
    for project in projects:
        shutil.copyfile(project["archive"], source_output / project["archive"].name)
        for source in project["source"].rglob("*"):
            if source.is_file() and re.search(r"^(?:license|copying|copyright)(?:[._-]|$)|notices?|license_and_copyright", source.name, re.IGNORECASE):
                target = output / "licenses/solvers" / project["slug"] / source.relative_to(project["source"])
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
    script = Path(__file__).resolve()
    bundled = script.parents[1] / "third_party/licenses/solvers"
    if not bundled.is_dir():
        bundled = script.parent.parent / "licenses/solvers"
    if bundled.is_dir():
        for source in bundled.rglob("*"):
            if source.is_file():
                target = output / "licenses/solvers" / source.relative_to(bundled)
                if not target.exists():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(source, target)
    return projects


BOOST_CMAKE = """cmake_minimum_required(VERSION 3.20)
project(TigerSheetsBoostSerialization LANGUAGES CXX)
file(GLOB BOOST_SERIALIZATION_SOURCES CONFIGURE_DEPENDS "${BOOST_SERIALIZATION_ROOT}/src/*.cpp")
add_library(boost_serialization SHARED ${BOOST_SERIALIZATION_SOURCES})
target_include_directories(boost_serialization PUBLIC "${BOOST_SERIALIZATION_ROOT}/include" "${BOOST_HEADERS}")
target_compile_definitions(boost_serialization PRIVATE BOOST_SERIALIZATION_DYN_LINK)
set_target_properties(boost_serialization PROPERTIES VERSION "${BOOST_SERIALIZATION_VERSION}" SOVERSION "${BOOST_SERIALIZATION_VERSION}")
install(TARGETS boost_serialization LIBRARY DESTINATION lib)
"""


def validate_boost_headers(include_dir, version):
    header = Path(include_dir) / "boost/version.hpp"
    major, minor, patch = (int(part) for part in version.split("."))
    expected = major * 100000 + minor * 100 + patch
    match = re.search(r"^\s*#\s*define\s+BOOST_VERSION\s+(\d+)", header.read_text(), re.MULTILINE)
    if not match or int(match.group(1)) != expected:
        raise ValueError(f"Boost headers must match the pinned Serialization {version}: {header}")


def build_dependencies(projects, work_dir, jobs=2, boost_headers=Path("/usr/include"), install_prefix=None):
    if jobs < 1:
        raise ValueError("--jobs must be positive")
    work_dir = Path(work_dir).resolve()
    prefix = Path(install_prefix).resolve() if install_prefix is not None else work_dir / "prefix"
    boost_headers = Path(boost_headers).resolve()
    serialization = next(project["library"] for project in projects if project["library"]["name"] == "Boost.Serialization")
    validate_boost_headers(boost_headers, serialization["version"])
    order = {name: index for index, name in enumerate(("HiGHS", "NLopt", "oneTBB", "Boost.Serialization", "pagmo"))}
    for project in sorted(projects, key=lambda item: order[item["library"]["name"]]):
        if project["library"]["name"] == "Boost.Serialization":
            wrapper = project["source"] / "_tigersheets_build"
            wrapper.mkdir(exist_ok=True)
            (wrapper / "CMakeLists.txt").write_text(BOOST_CMAKE, encoding="utf-8")
        commands = build_commands(project["library"], project["source"], work_dir / "build" / project["slug"],
                                  prefix, jobs, boost_headers)
        for command in commands:
            print("+ " + " ".join(command), flush=True)
            subprocess.run(command, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=default_manifest())
    parser.add_argument("--work-dir", type=Path, default=Path.cwd() / ".deps/solvers")
    parser.add_argument("--prefix", type=Path, help="Local install destination (default: WORK_DIR/prefix)")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--boost-include", type=Path, default=Path("/usr/include"))
    parser.add_argument("--download", action="store_true", help="Explicitly permit fetching missing pinned source archives")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--verify-only", action="store_true", help="Only verify all pinned archive checksums")
    mode.add_argument("--prepare-only", action="store_true", help="Verify, extract, and preserve sources/licenses without compiling")
    args = parser.parse_args(argv)
    try:
        if args.jobs < 1:
            raise ValueError("--jobs must be positive")
        if args.verify_only:
            manifest = load_manifest(args.manifest)
            for library in manifest["libraries"]:
                ensure_archive(library, args.work_dir / "downloads", args.download)
            print("All five pinned solver source archives verified")
        else:
            projects = prepare_dependencies(args.manifest, args.work_dir, args.download, args.prefix)
            if not args.prepare_only:
                build_dependencies(projects, args.work_dir, args.jobs, args.boost_include, args.prefix)
            prefix = args.prefix.resolve() if args.prefix is not None else args.work_dir.resolve() / "prefix"
            print(f"Solver dependency inputs preserved in {prefix / 'share/tiger-sheets'}")
        return 0
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.CalledProcessError) as error:
        print(f"Solver dependency bootstrap failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
