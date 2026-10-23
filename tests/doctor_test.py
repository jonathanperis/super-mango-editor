#!/usr/bin/env python3
"""Host-prerequisite decisions in tools/doctor.py, with the machine faked out."""

from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("doctor", ROOT / "tools" / "doctor.py")
assert spec and spec.loader
doctor = importlib.util.module_from_spec(spec)
sys.modules["doctor"] = doctor
spec.loader.exec_module(doctor)


class FakeHost:
    def __init__(self) -> None:
        self.bins = {"clang": "/usr/bin/clang", "c++": "/usr/bin/c++", "cmake": "/usr/bin/cmake",
                     "make": "/usr/bin/make", "node": "/usr/bin/node", "zenity": "/usr/bin/zenity",
                     "bun": "/usr/bin/bun", "emcc": "/opt/emsdk/emcc", "pactl": "/usr/bin/pactl"}
        self.system_name = "Linux"
        self.family_name = "debian"
        self.node_version = "v22.23.4"
        self.emcc_version = "emcc (Emscripten) 9.9.9"
        self.makefile = "EMSCRIPTEN_VERSION ?= 9.9.9\n"
        self.cxx_ok = True
        self.c_ok = True
        self.headers_ok = True
        self.sinks = "1\tmango\tmodule-null-sink.c\tSUSPENDED\n"
        self.cards = None
        self.files = {str(ROOT / "Makefile"): self.makefile}

    def which(self, name: str):
        return self.bins.get(name)

    def system(self) -> str:
        return self.system_name

    def family(self) -> str:
        return self.family_name

    def read(self, path: Path):
        # Fixtures use POSIX paths. On Windows, str(Path("/proc/..."))
        # is a different string, so also try the POSIX form.
        return self.files.get(str(path), self.files.get(path.as_posix()))

    def run(self, argv, stdin=None, timeout=20):
        program = Path(argv[0]).name
        if program in {"clang", "cc", "c++", "g++"}:
            source = ""
            for arg in argv[1:]:
                if arg.endswith((".c", ".cpp")) and Path(arg).is_file():
                    source = Path(arg).read_text(encoding="utf-8")
            if source.startswith("#include <new>"):
                if not self.cxx_ok:
                    return 1, "/usr/bin/ld: cannot find -lstdc++"
                return 0, ""
            if "#include <" in source and not self.headers_ok:
                header = source.split("<", 1)[1].split(">", 1)[0]
                return 1, f"fatal error: {header}: No such file or directory"
            if not self.c_ok and source.startswith("int main"):
                return 1, "clang: error: linker command failed"
            return 0, ""
        if program == "node":
            return 0, self.node_version
        if program == "bun":
            return 0, "1.4.4"
        if program == "emcc":
            return 0, self.emcc_version
        if program == "pactl":
            return 0, self.sinks
        return 127, f"unexpected {' '.join(argv)}"


def by_name(findings):
    return {finding.name: finding for finding in findings}


def statuses(findings):
    return {finding.name: finding.status for finding in findings}


def main() -> None:
    os.environ.pop("CC", None)
    os.environ.pop("CXX", None)
    pin = doctor.emscripten_pin("note\nEMSCRIPTEN_VERSION ?= 1.2.3\nEMSCRIPTEN_VERSION = 9\n")
    assert pin == "1.2.3", pin

    ready = by_name(doctor.collect(FakeHost()))
    assert ready["C compiler"].status == "ok", ready["C compiler"]
    assert ready["C++ standard library"].status == "ok"
    assert ready["X11/Xlib.h"].status == "ok"
    assert ready["emcc"].status == "ok", ready["emcc"]
    assert ready["audio"].status == "ok"
    assert not any(item.status == "fail" for item in ready.values())

    broken = FakeHost()
    broken.cxx_ok = False
    broken.bins.pop("c++")
    # which() misses c++ entirely: the message must say CMake looks for it.
    missing_cxx = by_name(doctor.collect(broken))["C++ standard library"]
    assert missing_cxx.status == "fail", missing_cxx
    assert "c++" in missing_cxx.detail and "g++" in missing_cxx.detail, missing_cxx.detail

    link = FakeHost()
    link.cxx_ok = False
    link_fail = by_name(doctor.collect(link))["C++ standard library"]
    assert link_fail.status == "fail" and "lstdc++" in link_fail.detail, link_fail.detail
    assert "apt install g++" in link_fail.detail

    headers = FakeHost()
    headers.headers_ok = False
    header = by_name(doctor.collect(headers))["X11/extensions/XInput2.h"]
    assert header.status == "fail" and "libxi-dev" in header.detail, header.detail

    fedora = FakeHost()
    fedora.family_name = "fedora"
    fedora.headers_ok = False
    assert "libXi-devel" in by_name(doctor.collect(fedora))["X11/extensions/XInput2.h"].detail

    mac = FakeHost()
    mac.system_name = "Darwin"
    mac.bins.pop("pactl")
    mac_findings = by_name(doctor.collect(mac))
    assert mac_findings["X11/GL headers"].status == "skip"
    assert mac_findings["zenity"].status == "skip"

    old_node = FakeHost()
    old_node.node_version = "v18.19.0"
    node = by_name(doctor.collect(old_node))["Node.js"]
    assert node.status == "fail" and "22.12" in node.detail, node.detail

    optional = FakeHost()
    optional.bins.pop("zenity")
    optional.bins.pop("bun")
    optional.bins.pop("emcc")
    optional.sinks = ""
    optional.files["/proc/asound/cards"] = "--- no soundcards ---\n"
    notes = by_name(doctor.collect(optional))
    assert notes["zenity"].status == "warn" and "file dialogs" in notes["zenity"].detail
    assert notes["Bun"].status == "warn"
    assert notes["emcc"].status == "warn" and "9.9.9" in notes["emcc"].detail, notes["emcc"].detail
    assert notes["audio"].status == "warn" and "RAYLIB_PLATFORM=memory" in notes["audio"].detail
    assert not any(item.status == "fail" for item in notes.values())

    mismatch = FakeHost()
    mismatch.emcc_version = "emcc (Emscripten) 6.0.0"
    emcc = by_name(doctor.collect(mismatch))["emcc"]
    assert emcc.status == "warn" and "9.9.9" in emcc.detail and "6.0.0" in emcc.detail, emcc.detail

    alsa = FakeHost()
    alsa.bins.pop("pactl")
    alsa.files["/proc/asound/cards"] = " 0 [PCH]: HDA Intel\n"
    assert by_name(doctor.collect(alsa))["audio"].status == "ok"

    rendered = doctor.render(doctor.collect(optional))
    assert "optional note" in rendered and "FAIL" not in rendered, rendered
    failed = doctor.render(doctor.collect(link))
    assert "required check" in failed and "FAIL" in failed

    print("doctor_test: ok")


if __name__ == "__main__":
    main()
