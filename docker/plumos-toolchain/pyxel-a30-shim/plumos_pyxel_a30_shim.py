"""plumOS A30 Pyxel launcher shim.

This module keeps Pyxel itself unmodified.  It wraps the public pyxel.init()
function, publishes the app's logical window size to the SDL a30mali backend,
then runs the original Python command.
"""

from __future__ import annotations

import inspect
import os
import runpy
import sys
from pathlib import Path


def _is_disabled(value: str | None) -> bool:
    return value is not None and value.lower() in {"0", "false", "no", "off", "none"}


def _as_positive_int(value: object) -> int | None:
    try:
        number = int(value)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return None
    return number if number > 0 else None


def _extract_init_value(name: str, index: int, args: tuple[object, ...], kwargs: dict[str, object], bound: dict[str, object]) -> object | None:
    if name in bound:
        return bound[name]
    if name in kwargs:
        return kwargs[name]
    if len(args) > index:
        return args[index]
    return None


def _publish_pyxel_init_geometry(original_init, args: tuple[object, ...], kwargs: dict[str, object]) -> None:
    bound: dict[str, object] = {}
    try:
        signature = inspect.signature(original_init)
        bound = dict(signature.bind_partial(*args, **kwargs).arguments)
    except (TypeError, ValueError):
        pass

    width = _as_positive_int(_extract_init_value("width", 0, args, kwargs, bound))
    height = _as_positive_int(_extract_init_value("height", 1, args, kwargs, bound))
    display_scale = _as_positive_int(_extract_init_value("display_scale", 5, args, kwargs, bound))

    if width is None or height is None:
        return

    logical_width = width
    logical_height = height
    if display_scale is not None:
        logical_width *= display_scale
        logical_height *= display_scale

    os.environ["PLUMOS_A30MALI_PYXEL_CANVAS_SIZE"] = f"{width}x{height}"
    os.environ["PLUMOS_A30MALI_LOGICAL_SIZE"] = f"{logical_width}x{logical_height}"

    log_enabled = os.environ.get("PLUMOS_PYXEL_INIT_SHIM_LOG")
    if log_enabled is not None and not _is_disabled(log_enabled):
        print(
            "plumOS Pyxel init shim "
            f"canvas={width}x{height} "
            f"display_scale={display_scale or 'auto'} "
            f"logical={logical_width}x{logical_height}",
            file=sys.stderr,
            flush=True,
        )


def _patch_pyxel_init() -> None:
    if _is_disabled(os.environ.get("PLUMOS_PYXEL_INIT_SHIM")):
        return

    try:
        import pyxel  # type: ignore
    except Exception:
        return

    original_init = getattr(pyxel, "init", None)
    if original_init is None or getattr(original_init, "_plumos_a30_wrapped", False):
        return

    def init_wrapper(*args, **kwargs):
        _publish_pyxel_init_geometry(original_init, args, kwargs)
        return original_init(*args, **kwargs)

    init_wrapper._plumos_a30_wrapped = True  # type: ignore[attr-defined]
    init_wrapper.__name__ = getattr(original_init, "__name__", "init")
    init_wrapper.__doc__ = getattr(original_init, "__doc__", None)
    pyxel.init = init_wrapper


def _run_python_command(argv: list[str]) -> None:
    if not argv:
        raise SystemExit("plumos_pyxel_a30_shim: missing Python command")

    if argv[0] == "-m":
        if len(argv) < 2:
            raise SystemExit("plumos_pyxel_a30_shim: -m requires a module name")
        module = argv[1]
        sys.argv = [module, *argv[2:]]
        runpy.run_module(module, run_name="__main__", alter_sys=True)
        return

    if argv[0] == "-c":
        if len(argv) < 2:
            raise SystemExit("plumos_pyxel_a30_shim: -c requires code")
        sys.argv = ["-c", *argv[2:]]
        namespace = {"__name__": "__main__", "__package__": None}
        exec(argv[1], namespace)
        return

    script = Path(argv[0])
    if script.parent:
        sys.path[0] = str(script.resolve().parent)
    sys.argv = argv
    runpy.run_path(str(script), run_name="__main__")


def main() -> None:
    _patch_pyxel_init()
    _run_python_command(sys.argv[1:])


if __name__ == "__main__":
    main()
