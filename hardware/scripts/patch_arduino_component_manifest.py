import json
import hashlib
import shutil
import tarfile
import tempfile
import urllib.request
from pathlib import Path

Import("env")


ARDUINO_STATE_FILE = ".hitomi-source.json"
LOCK_FILE_NAME = "arduino-component.lock.json"


def _load_lock(project_dir: Path) -> dict:
    lock_path = project_dir / LOCK_FILE_NAME
    if not lock_path.exists():
        raise RuntimeError(f"missing Arduino lock file: {lock_path}")

    data = json.loads(lock_path.read_text())
    required = ("component_name", "version", "url", "sha256", "dependencies")
    missing = [key for key in required if key not in data]
    if missing:
        raise RuntimeError(f"Arduino lock file missing keys: {', '.join(missing)}")

    if "joltwallet/littlefs" not in data["dependencies"]:
        raise RuntimeError("Arduino lock file must define joltwallet/littlefs")

    return data


def _curated_manifest(lock: dict) -> str:
    version = lock["version"]
    littlefs_version = lock["dependencies"]["joltwallet/littlefs"]
    return f"""dependencies:
  idf: '>=5.3,<5.6'
  joltwallet/littlefs:
    version: {littlefs_version}
description: Arduino core for ESP32, ESP32-C, ESP32-H, ESP32-P, ESP32-S series of SoCs
files:
  exclude:
  - .*
  - .gitlab/
  - .gitlab/**/*
  - docs/
  - docs/**/*
  - idf_component_examples/
  - idf_component_examples/**/*
  - package/
  - package/**/*
  - tests/
  - tests/**/*
  - tools/
  - tools/**/*
  - variants/**/*
  - boards.txt
  - CODE_OF_CONDUCT.md
  - LICENSE.md
  - package.json
  - platform.txt
  - programmers.txt
  include:
  - variants/esp32/**/*
  - variants/esp32c2/**/*
  - variants/esp32c3/**/*
  - variants/esp32c5/**/*
  - variants/esp32c6/**/*
  - variants/esp32c61/**/*
  - variants/esp32h2/**/*
  - variants/esp32p4/**/*
  - variants/esp32s2/**/*
  - variants/esp32s3/**/*
license: LGPL-2.1
repository: git://github.com/espressif/arduino-esp32.git
tags:
- arduino
targets:
- esp32
- esp32c2
- esp32c3
- esp32c5
- esp32c6
- esp32c61
- esp32h2
- esp32p4
- esp32s2
- esp32s3
url: https://github.com/espressif/arduino-esp32
version: {version}
"""


def _compute_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def _ensure_tarball(archive_path: Path) -> None:
    lock = _load_lock(Path(env.subst("$PROJECT_DIR")))
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    if archive_path.exists() and _compute_sha256(archive_path) == lock["sha256"]:
        return

    tmp_path = archive_path.with_suffix(".tmp")
    if tmp_path.exists():
        tmp_path.unlink()

    digest = hashlib.sha256()
    with urllib.request.urlopen(lock["url"], timeout=60) as response, tmp_path.open("wb") as output:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            output.write(chunk)
            digest.update(chunk)

    actual_hash = digest.hexdigest()
    if actual_hash != lock["sha256"]:
        tmp_path.unlink(missing_ok=True)
        raise RuntimeError(
            f"unexpected Arduino tarball SHA256 {actual_hash!r}, "
            f"expected {lock['sha256']!r}"
        )

    tmp_path.replace(archive_path)


def _extract_tarball(archive_path: Path, local_dir: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="hitomi-arduino-") as temp_dir_raw:
        temp_dir = Path(temp_dir_raw)
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(temp_dir)

        extracted_dirs = [entry for entry in temp_dir.iterdir() if entry.is_dir()]
        if len(extracted_dirs) != 1:
            raise RuntimeError(f"unexpected Arduino archive layout in {archive_path}")

        shutil.rmtree(local_dir, ignore_errors=True)
        local_dir.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(extracted_dirs[0], local_dir)


def _load_state(state_path: Path) -> dict:
    if not state_path.exists():
        return {}
    return json.loads(state_path.read_text())


def _write_state(state_path: Path) -> None:
    lock = _load_lock(Path(env.subst("$PROJECT_DIR")))
    state = {
        "version": lock["version"],
        "url": lock["url"],
        "sha256": lock["sha256"],
    }
    state_path.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n")


def _patch_manifest():
    project_dir = Path(env.subst("$PROJECT_DIR"))
    lock = _load_lock(project_dir)
    expected_arduino_version = lock["version"]
    expected_tarball_url = lock["url"]
    expected_tarball_sha256 = lock["sha256"]
    expected_component_name = lock["component_name"]

    archive_path = project_dir / ".pio" / "downloads" / f"arduino-esp32-{expected_arduino_version}.tar.gz"
    local_dir = project_dir / ".pio" / "installed" / expected_component_name
    local_package_json = local_dir / "package.json"
    state_path = local_dir / ARDUINO_STATE_FILE

    _ensure_tarball(archive_path)
    state = _load_state(state_path)
    local_version = ""
    if local_package_json.exists():
        local_version = json.loads(local_package_json.read_text()).get("version", "")

    if (
        not local_package_json.exists()
        or not local_version.startswith(expected_arduino_version)
        or state.get("sha256") != expected_tarball_sha256
        or state.get("url") != expected_tarball_url
    ):
        _extract_tarball(archive_path, local_dir)
        _write_state(state_path)
        print(f"[patch-arduino-manifest] prepared local Arduino package at {local_dir}")

    manifest_path = local_dir / "idf_component.yml"
    if not manifest_path.exists():
        raise RuntimeError(f"missing local Arduino manifest: {manifest_path}")

    curated_manifest = _curated_manifest(lock)
    current = manifest_path.read_text()
    if current == curated_manifest:
        print("[patch-arduino-manifest] already patched")
    else:
        manifest_path.write_text(curated_manifest)
        print(f"[patch-arduino-manifest] wrote curated manifest: {manifest_path}")

    env["ENV"]["HITOMI_ARDUINO_COMPONENT_DIR"] = str(local_dir)
    print(f"[patch-arduino-manifest] using Arduino component dir: {local_dir}")


_patch_manifest()
