#!/usr/bin/env python3
"""Check the host tools a build or a test run needs, and name what is missing.

Required checks fail the command. zenity, Bun, emcc and a playback device are
reported and do not: native builds and `make test` can run without them.
`emcc` is compared with the Makefile's `EMSCRIPTEN_VERSION` pin, which this
script reads from the file so it does not keep a second copy of the number.
"""

from __future__ import annotations

import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NODE_MIN = (22, 12)  # README prerequisites; make test runs the Node host checks
PYTHON_MIN = (3, 11)  # docs/README.md

# header, Debian/Ubuntu package, Fedora package, Arch package
LINUX_HEADERS = (
    ("X11/Xlib.h", "libx11-dev", "libX11-devel", "libx11"),
    ("X11/extensions/Xrandr.h", "libxrandr-dev", "libXrandr-devel", "libxrandr"),
    ("X11/extensions/Xinerama.h", "libxinerama-dev", "libXinerama-devel", "libxinerama"),
    ("X11/Xcursor/Xcursor.h", "libxcursor-dev", "libXcursor-devel", "libxcursor"),
    ("X11/extensions/XInput2.h", "libxi-dev", "libXi-devel", "libxi"),
    ("GL/gl.h", "libgl1-mesa-dev", "mesa-libGL-devel", "mesa"),
)

CXX_SOURCE = """#include <new>
int main() {
    int *value = new int(1);
    delete value;
    return 0;
}
"""

C_SOURCE = """int main(void) { return 0; }
"""


@dataclass
class Finding:
    name: str
    status: str  # ok, fail, warn, skip
    detail: str = ""


class Host:
    """The real machine. Tests substitute their own which/run/system/family."""

    def which(self, name: str) -> str | None:
        return shutil.which(name)

    def run(self, argv: list[str], stdin: str | None = None, timeout: int = 20) -> tuple[int, str]:
        try:
            proc = subprocess.run(
                argv, input=stdin, text=True, capture_output=True, timeout=timeout, check=False,
            )
        except FileNotFoundError:
            return 127, f"{argv[0]}: not found"
        except subprocess.TimeoutExpired:
            return 124, f"{argv[0]}: timed out after {timeout}s"
        text = (proc.stdout or "") + (proc.stderr or "")
        return proc.returncode, text.strip()

    def system(self) -> str:
        return platform.system()

    def family(self) -> str:
        """debian, fedora, arch, or unknown. macOS and Windows are unknown."""
        path = Path("/etc/os-release")
        if not path.is_file():
            return "unknown"
        ident = ""
        like = ""
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("ID="):
                ident = line.split("=", 1)[1].strip().strip('"').lower()
            elif line.startswith("ID_LIKE="):
                like = line.split("=", 1)[1].strip().strip('"').lower()
        blob = f"{ident} {like}"
        if "debian" in blob or ident in {"ubuntu", "debian", "linuxmint", "pop"}:
            return "debian"
        if ident in {"fedora", "rhel", "centos", "rocky", "almalinux"} or "fedora" in blob or "rhel" in blob:
            return "fedora"
        if "arch" in blob or ident in {"arch", "manjaro"}:
            return "arch"
        return "unknown"

    def read(self, path: Path) -> str | None:
        if not path.is_file():
            return None
        return path.read_text(encoding="utf-8", errors="replace")


def emscripten_pin(makefile: str) -> str:
    match = re.search(r"^EMSCRIPTEN_VERSION \?= (\S+)", makefile, re.MULTILINE)
    return match.group(1) if match else ""


def version_tuple(text: str) -> tuple[int, int] | None:
    match = re.search(r"(\d+)\.(\d+)", text)
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


def compile_file(host: Host, compiler: str, source: str, suffix: str, extra: list[str]) -> tuple[int, str]:
    with tempfile.TemporaryDirectory(prefix="mango-doctor-") as tmp:
        src = Path(tmp) / f"probe{suffix}"
        src.write_text(source, encoding="utf-8")
        out = Path(tmp) / "probe"
        code, text = host.run([compiler, str(src), "-o", str(out), *extra])
        return code, text


def header_compiles(host: Host, compiler: str, header: str) -> tuple[int, str]:
    source = f"#include <{header}>\nint main(void) {{ return 0; }}\n"
    with tempfile.TemporaryDirectory(prefix="mango-doctor-") as tmp:
        src = Path(tmp) / "probe.c"
        src.write_text(source, encoding="utf-8")
        return host.run([compiler, "-fsyntax-only", str(src)])


def first_line(text: str) -> str:
    line = text.strip().splitlines()
    return line[0] if line else ""


def package_for(family: str, packages: tuple[str, str, str]) -> str:
    debian, fedora, arch = packages
    if family == "fedora":
        return f"Fedora/RHEL: sudo dnf install {fedora}"
    if family == "arch":
        return f"Arch: sudo pacman -S {arch}"
    if family == "debian":
        return f"Debian/Ubuntu: sudo apt install {debian}"
    return f"Debian/Ubuntu: {debian}; Fedora/RHEL: {fedora}; Arch: {arch}"


def cxx_install_hint(family: str) -> str:
    if family == "fedora":
        return "Fedora/RHEL: sudo dnf install gcc-c++"
    if family == "arch":
        return "Arch: sudo pacman -S gcc"
    if family == "debian":
        return "Debian/Ubuntu: sudo apt install g++"
    return "Install a C++ compiler that can link the C++ standard library (Debian/Ubuntu: g++)."


def check_c_compiler(host: Host) -> Finding:
    name = os.environ.get("CC") or ("clang" if host.system() != "Windows" else "clang")
    path = host.which(name)
    if path is None and name != "cc":
        path = host.which("cc")
        name = "cc" if path else name
    if path is None:
        return Finding(
            "C compiler", "fail",
            f"{name} is not on PATH. The Makefile defaults to clang "
            "(Debian/Ubuntu: sudo apt install clang build-essential).",
        )
    code, text = compile_file(host, path, C_SOURCE, ".c", [])
    if code != 0:
        return Finding("C compiler", "fail", f"{path} could not compile a small C program.\n{first_line(text)}")
    return Finding("C compiler", "ok", path)


def check_cxx(host: Host) -> Finding:
    name = os.environ.get("CXX") or "c++"
    path = host.which(name)
    if path is None:
        return Finding(
            "C++ standard library", "fail",
            f"{name} is not on PATH. raylib's CMake build compiles a small C++ program "
            f"with this compiler and links the C++ standard library.\n{cxx_install_hint(host.family())}",
        )
    code, text = compile_file(host, path, CXX_SOURCE, ".cpp", [])
    if code != 0:
        return Finding(
            "C++ standard library", "fail",
            "raylib's CMake build compiles a small C++ program and links the C++ "
            f"standard library. {path} could not do that.\n{first_line(text)}\n"
            f"{cxx_install_hint(host.family())}",
        )
    return Finding("C++ standard library", "ok", path)


def check_tool(host: Host, program: str, label: str, hint: str) -> Finding:
    path = host.which(program)
    if path is None:
        return Finding(label, "fail", f"{program} is not on PATH. {hint}")
    return Finding(label, "ok", path)


def check_python(host: Host) -> Finding:
    version = sys.version_info
    if (version.major, version.minor) < PYTHON_MIN:
        return Finding(
            "Python", "fail",
            f"Python {version.major}.{version.minor} is older than "
            f"{PYTHON_MIN[0]}.{PYTHON_MIN[1]}, which the docs tools need.",
        )
    return Finding("Python", "ok", f"{version.major}.{version.minor}.{version.micro}")


def check_node(host: Host) -> Finding:
    path = host.which("node")
    if path is None:
        return Finding(
            "Node.js", "fail",
            "node is not on PATH. make test runs the JavaScript host-contract tests "
            f"and needs Node.js {NODE_MIN[0]}.{NODE_MIN[1]} or newer.",
        )
    code, text = host.run([path, "--version"])
    found = version_tuple(text)
    if code != 0 or found is None:
        return Finding("Node.js", "fail", f"{path} --version failed.\n{first_line(text)}")
    if found < NODE_MIN:
        return Finding(
            "Node.js", "fail",
            f"Node.js {found[0]}.{found[1]} is older than {NODE_MIN[0]}.{NODE_MIN[1]}. "
            "make test runs the JavaScript host-contract tests.",
        )
    return Finding("Node.js", "ok", text.strip().splitlines()[0])


def check_headers(host: Host, compiler: str | None) -> list[Finding]:
    if host.system() != "Linux":
        return [Finding("X11/GL headers", "skip", f"not used on {host.system() or 'this host'}")]
    if not compiler:
        return [Finding("X11/GL headers", "fail", "skipped because no C compiler is available")]
    findings = []
    for header, *packages in LINUX_HEADERS:
        code, text = header_compiles(host, compiler, header)
        if code != 0:
            findings.append(Finding(
                header, "fail",
                f"The compiler cannot see <{header}>, which the native raylib link needs.\n"
                f"{first_line(text)}\n{package_for(host.family(), (packages[0], packages[1], packages[2]))}",
            ))
        else:
            findings.append(Finding(header, "ok"))
    return findings


def check_zenity(host: Host) -> Finding:
    if host.system() != "Linux":
        return Finding("zenity", "skip", "Linux file dialogs only")
    path = host.which("zenity")
    if path is None:
        return Finding(
            "zenity", "warn",
            "zenity is not on PATH. The editor's native file dialogs on Linux call it. "
            "The game and the tests do not (Debian/Ubuntu: sudo apt install zenity).",
        )
    return Finding("zenity", "ok", path)


def check_bun(host: Host) -> Finding:
    path = host.which("bun")
    if path is None:
        return Finding(
            "Bun", "warn",
            "bun is not on PATH. The docs site in docs/ installs and builds with Bun. The game does not.",
        )
    code, text = host.run([path, "--version"])
    if code != 0:
        return Finding("Bun", "warn", f"{path} --version failed.\n{first_line(text)}")
    return Finding("Bun", "ok", first_line(text))


def check_emcc(host: Host) -> Finding:
    makefile = host.read(ROOT / "Makefile") or ""
    pin = emscripten_pin(makefile)
    path = host.which("emcc")
    if path is None:
        pin_text = f" The Makefile pin is {pin}." if pin else ""
        return Finding(
            "emcc", "warn",
            "emcc is not on PATH. make web needs the Emscripten SDK."
            f"{pin_text} Native builds do not need it.",
        )
    code, text = host.run([path, "--version"])
    reported = first_line(text)
    if code != 0:
        return Finding("emcc", "warn", f"{path} --version failed.\n{reported}")
    if pin and not re.search(rf"(?<![\d.]){re.escape(pin)}(?![\d.])", reported):
        return Finding(
            "emcc", "warn",
            f"emcc reports {reported!r}; the Makefile pin (EMSCRIPTEN_VERSION) is {pin}. "
            "make web stops until they match. Activate that SDK, or pass "
            "EMSCRIPTEN_VERSION=<version> (empty skips the check).",
        )
    detail = reported if not pin else f"{reported} (Makefile pin {pin})"
    return Finding("emcc", "ok", detail)


def has_alsa_card(text: str) -> bool:
    return any(re.match(r"\s*\d+\s", line) for line in text.splitlines())


def check_audio(host: Host) -> Finding:
    system = host.system()
    if system == "Linux":
        pactl = host.which("pactl")
        if pactl is not None:
            code, text = host.run([pactl, "list", "short", "sinks"])
            if code == 0 and text.strip():
                return Finding("audio", "ok", "PulseAudio/PipeWire sink present")
        cards = host.read(Path("/proc/asound/cards"))
        if cards and has_alsa_card(cards):
            return Finding("audio", "ok", "ALSA card present")
        return Finding(
            "audio", "warn",
            "No playback device found (no PulseAudio/PipeWire sink and no ALSA card). "
            "The game refuses to start without one. Tests can use "
            "RAYLIB_PLATFORM=memory or RAYLIB_AUDIO=null. A PulseAudio null sink is enough.",
        )
    if system == "Darwin":
        code, text = host.run(["system_profiler", "SPAudioDataType"], timeout=15)
        if code == 0 and "Output" in text:
            return Finding("audio", "ok", "audio device listed")
        return Finding(
            "audio", "warn",
            "Could not see a playback device. The game refuses to start without one. "
            "Tests can use RAYLIB_PLATFORM=memory or RAYLIB_AUDIO=null.",
        )
    return Finding("audio", "skip", "not probed on this host; the game still needs a playback device")


def compiler_path(host: Host) -> str | None:
    name = os.environ.get("CC") or "clang"
    return host.which(name) or host.which("cc")


def collect(host: Host) -> list[Finding]:
    compiler = compiler_path(host)
    findings = [
        check_c_compiler(host),
        check_cxx(host),
        check_tool(host, "cmake", "CMake", "raylib is built with CMake (Debian/Ubuntu: sudo apt install cmake)."),
        check_tool(host, "make", "Make", "Make is the application entry point (Debian/Ubuntu: sudo apt install make)."),
        check_python(host),
        check_node(host),
        *check_headers(host, compiler),
        check_zenity(host),
        check_bun(host),
        check_emcc(host),
        check_audio(host),
    ]
    return findings


def render(findings: list[Finding]) -> str:
    lines: list[str] = []
    for finding in findings:
        label = {"ok": "ok  ", "fail": "FAIL", "warn": "warn", "skip": "skip"}[finding.status]
        lines.append(f"{label}  {finding.name}" + (f"  ({finding.detail})" if finding.status == "ok" and finding.detail else ""))
        if finding.status in {"fail", "warn"} and finding.detail:
            for detail in finding.detail.splitlines():
                lines.append(f"      {detail}")
        elif finding.status == "skip" and finding.detail:
            lines.append(f"      {finding.detail}")
    failed = [f for f in findings if f.status == "fail"]
    warned = [f for f in findings if f.status == "warn"]
    if failed:
        lines.append(f"doctor: {len(failed)} required check(s) failed; {len(warned)} optional note(s)")
    elif warned:
        lines.append(f"doctor: host can build; {len(warned)} optional note(s)")
    else:
        lines.append("doctor: host looks ready")
    return "\n".join(lines)


def main() -> int:
    findings = collect(Host())
    print(render(findings))
    return 1 if any(f.status == "fail" for f in findings) else 0


if __name__ == "__main__":
    sys.exit(main())
