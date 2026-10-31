"""Check the vendor SDK before packaging; never accesses account credentials."""
import argparse
import ctypes
from pathlib import Path
import re
import tempfile


class LogSettings(ctypes.Structure):
    _fields_ = [("fileDir", ctypes.c_char_p), ("expireHours", ctypes.c_int),
                ("level", ctypes.c_int)]


def validate(application_directory):
    root = application_directory.resolve(strict=True)
    library = root / "FlashNetwork.dll"
    settings = root / "resources/data/FLASHNETWORK9.DAT"
    if not library.is_file() or not settings.is_file():
        raise RuntimeError("Supply a complete official application directory with its SDK and settings.")
    source = (Path(__file__).resolve().parents[1] /
              "src/slic3r/GUI/FlashForge/FlashNetworkIntfc.cpp").read_text(encoding="utf-8")
    expected = re.search(r'expectedVersion\[\] = "([^"]+)"', source).group(1)
    dll = ctypes.CDLL(str(library))
    dll.fnet_getVersion.restype = ctypes.c_char_p
    version = dll.fnet_getVersion().decode("ascii")
    if version != expected:
        raise RuntimeError(f"FlashNetwork {version} is incompatible with this source (requires {expected}). "
                           "Use the SDK and settings from official Flash Studio Desktop 1.7.15.")
    required = re.findall(r"INIT_FUNC_PTR\(\w+, (fnet_\w+)\)", source)
    missing = [name for name in required if not hasattr(dll, name)]
    if missing:
        raise RuntimeError("Missing SDK exports: " + ", ".join(missing))
    dll.fnet_initlize.argtypes = [ctypes.c_char_p, ctypes.POINTER(LogSettings)]
    dll.fnet_initlize.restype = ctypes.c_int
    dll.fnet_uninitlize.argtypes = []
    with tempfile.TemporaryDirectory(prefix="flashstudio-sdk-check-") as logs:
        # SDK logging is disabled, including during this packaging check.
        log_settings = LogSettings(str(logs).encode("utf-8"), 1, 0)
        result = dll.fnet_initlize(str(settings).encode("utf-8"), ctypes.byref(log_settings))
        if result != 0:
            raise RuntimeError(f"FlashNetwork/settings initialization failed ({result}).")
        dll.fnet_uninitlize()
    print(f"FlashNetwork {version}: {len(required)} exports and settings initialization verified.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("application_directory", type=Path)
    args = parser.parse_args()
    try:
        validate(args.application_directory)
    except (OSError, RuntimeError, AttributeError, ValueError) as error:
        parser.exit(1, f"SDK validation failed: {error}\n")
