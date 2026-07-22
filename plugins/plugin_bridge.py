#!/usr/bin/env python3
"""
plugin_bridge.py — JSON-RPC 2.0 bridge, stdin/stdout.

Plugin formats:
  1. Folder plugin: python plugin_bridge.py plugins/explorer/
     Must have plugin.json with "main" pointing to entry script.
  2. Single file: python plugin_bridge.py plugins/hello.py  (legacy)

Auto-dependency flow:
  on_load → read plugin.json "dependencies" →
    missing? → auto-create .venv in plugin folder →
    pip install into local .venv →
    inject site-packages → exec_module
"""
import sys, json, traceback, threading, importlib.util, queue, os, subprocess
from pathlib import Path

_stdout_lock = threading.Lock()

try:
    from jsonrpcserver import method, Success, dispatch
except ImportError:
    print(json.dumps({"jsonrpc":"2.0","error":{"code":-32000,"message":"Missing: pip install jsonrpcserver"},"id":None}), flush=True)
    sys.exit(1)

plugin_module = None
plugin_name = "unknown"
plugin_dir = None
plugin_manifest = None
plugin_has_ui = False
ui_enable_event = threading.Event()
ui_should_exit = threading.Event()

_response_callbacks = {}
_response_lock = threading.Lock()
_message_queue = queue.Queue(maxsize=512)

def send_to_host(obj):
    with _stdout_lock:
        print(json.dumps(obj, ensure_ascii=False), flush=True)

def notify_host(method, params=None):
    send_to_host({"jsonrpc":"2.0", "method":method, "params":params or {}})

def call_host(method, params=None, timeout=5.0):
    import time
    rid = int(time.time() * 1000000) % 1000000
    rid_str = str(rid)
    result = [None]
    event = threading.Event()
    with _response_lock:
        _response_callbacks[rid_str] = lambda r: (result.__setitem__(0, r), event.set())
    send_to_host({"jsonrpc":"2.0", "method":method, "params":params or {}, "id":rid})
    ok = event.wait(timeout=timeout)
    with _response_lock:
        _response_callbacks.pop(rid_str, None)
    return result[0] if ok else None

def _call_plugin(func_name, *args):
    if plugin_module and hasattr(plugin_module, func_name):
        try:
            r = getattr(plugin_module, func_name)(*args)
            return Success(r) if r is not None else Success(True)
        except Exception:
            traceback.print_exc(file=sys.stderr)
            return Success(True)
    return Success(True)

@method
def on_load(**params):
    if plugin_module is None:
        notify_host("log", {"level":2,"msg":f"Plugin not ready — deps not installed"})
    else:
        notify_host("log", {"level":1,"msg":f"Plugin '{plugin_name}' loaded"})
    return _call_plugin("on_load")

@method
def on_enable(**params):
    if plugin_module is None:
        notify_host("log", {"level":2,"msg":f"Plugin not ready — run Download first"})
        return Success(True)
    notify_host("log", {"level":1,"msg":f"Plugin '{plugin_name}' enabled"})
    if plugin_has_ui:
        ui_enable_event.set()
    return _call_plugin("on_enable")

@method
def on_disable(**params):
    notify_host("log", {"level":1,"msg":f"Plugin '{plugin_name}' disabled"})
    if plugin_has_ui:
        ui_should_exit.set()
        ui_enable_event.clear()
    return _call_plugin("on_disable")

@method
def on_unload(**params):
    return _call_plugin("on_unload")
@method
def on_update(**params): return _call_plugin("on_update", params)
@method
def on_menu_bar(**params): return _call_plugin("on_menu_bar")
@method
def on_ui_render(**params): return _call_plugin("on_ui_render")

def _stdin_reader():
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line: continue
        if '"id"' in line and ('"result"' in line or '"error"' in line):
            try:
                msg = json.loads(line)
                rid = str(msg.get("id", ""))
                if rid:
                    with _response_lock:
                        cb = _response_callbacks.pop(rid, None)
                    if cb:
                        try: cb(msg.get("result") if "result" in msg else msg.get("error"))
                        except Exception: pass
            except json.JSONDecodeError: pass
            continue
        try: _message_queue.put_nowait(line)
        except queue.Full:
            try: _message_queue.get_nowait()
            except queue.Empty: pass
            try: _message_queue.put_nowait(line)
            except queue.Full: pass

def _dispatch_worker():
    while True:
        try: line = _message_queue.get(timeout=0.5)
        except queue.Empty: continue
        try:
            resp = dispatch(line)
            if isinstance(resp, str):
                with _stdout_lock:
                    try: print(resp, flush=True)
                    except (BrokenPipeError, BlockingIOError, OSError): pass
        except Exception:
            traceback.print_exc(file=sys.stderr)

# ═══════════════════════════════════════════════════
# 依赖自动处理
# ═══════════════════════════════════════════════════

def _is_pkg_installed(name):
    try:
        from importlib.metadata import version as _v, PackageNotFoundError
        _v(name); return True
    except (PackageNotFoundError, ImportError): pass
    spec = importlib.util.find_spec(name.replace("-", "_"))
    return spec is not None and spec.origin is not None

def _pip_install(pkg, python_exe):
    try:
        r = subprocess.run([python_exe, "-m", "pip", "install", pkg],
            capture_output=True, text=True, timeout=600)
        if r.returncode != 0:
            err = r.stderr.strip()[-300:] if r.stderr else "no output"
            _log(f"pip install {pkg} FAILED: {err}")
        return r.returncode == 0
    except subprocess.TimeoutExpired:
        _log(f"TIMEOUT installing {pkg} (10 min)")
        return False
    except Exception as e:
        _log(f"ERROR installing {pkg}: {e}")
        return False

def _log(msg):
    """同时打到 C++ log 和 stderr，保证可见。"""
    print(f"[deps] {msg}", file=sys.stderr, flush=True)
    notify_host("log", {"level": 1, "msg": msg})

def _find_base_python(plugin_dir):
    """Find Python for a plugin. Priority: 1) plugin-local python/  2) sys.executable"""
    emb = os.path.join(plugin_dir, "python", "python.exe")
    if os.path.isfile(emb):
        return emb
    return sys.executable


def _setup_local_venv(plugin_dir):
    """Ensure plugin has a usable Python environment.
    Priority: 1) bundled python/  2) existing .venv  3) create .venv"""
    # Already has bundled python/
    emb_py = os.path.join(plugin_dir, "python", "python.exe")
    emb_sp = os.path.join(plugin_dir, "python", "Lib", "site-packages")
    if os.path.isfile(emb_py) and os.path.isdir(emb_sp):
        return emb_py, emb_sp

    # Fallback: use or create .venv
    venv = os.path.join(plugin_dir, ".venv")
    if os.path.isdir(venv):
        py = os.path.join(venv, "Scripts", "python.exe")
        sp = os.path.join(venv, "Lib", "site-packages")
        if os.path.isfile(py) and os.path.isdir(sp): return py, sp

    # Create .venv via virtualenv (for plugins without bundled python/)
    base_py = _find_base_python(plugin_dir)
    _log("Creating local Python environment...")
    r = subprocess.run(
        [base_py, "-m", "virtualenv", venv],
        capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        err = r.stderr.strip()[-400:] if r.stderr else "no output"
        _log(f"virtualenv FAILED: {err}")
        return None, None
    _log("Local environment created")
    py = os.path.join(venv, "Scripts", "python.exe")
    sp = os.path.join(venv, "Lib", "site-packages")
    return (py, sp) if os.path.isfile(py) and os.path.isdir(sp) else (None, None)

def _auto_deps(manifest, plugin_dir):
    """同步安装插件依赖。返回 True=就绪, False=失败。"""
    deps = manifest.get("dependencies", [])
    if not isinstance(deps, list) or not deps:
        return True

    py, sp = _setup_local_venv(plugin_dir)
    if sp and sp not in sys.path:
        sys.path.insert(0, sp)

    if py is None:
        _log("Cannot setup local environment")
        return False

    missing = []
    for d in deps:
        name = d.split(">=")[0].split("==")[0].split("<")[0].strip()
        pkg_name = name.replace("-", "_")
        found = False
        if sp and os.path.isdir(sp):
            for item in os.listdir(sp):
                if item.lower() == pkg_name.lower() or \
                   (item.lower().startswith(pkg_name.lower() + "-") and item.endswith(".dist-info")):
                    found = True
                    break
        if not found:
            missing.append(d)

    if not missing:
        _log("All dependencies already installed")
        return True

    total = len(missing)
    _log(f"Installing {total} packages: {', '.join(missing)}")

    for i, pkg in enumerate(missing):
        _log(f"[{i+1}/{total}] pip install {pkg} ...")
        ok = _pip_install(pkg, py)
        if not ok:
            _log(f"WARNING: [{i+1}/{total}] {pkg} install FAILED")
            return False
        else:
            _log(f"[{i+1}/{total}] {pkg} install OK")

    _log("All dependencies ready")
    return True

# ═══════════════════════════════════════════════════
# Plugin loader
# ═══════════════════════════════════════════════════

def _read_plugin_json(folder):
    p = os.path.join(folder, "plugin.json")
    if not os.path.isfile(p): return None
    try:
        with open(p, encoding="utf-8") as f:
            m = json.load(f)
        if "name" not in m or "main" not in m: return None
        if not os.path.isfile(os.path.join(folder, m["main"])): return None
        return m
    except Exception: return None

def load_plugin(plugin_path):
    global plugin_module, plugin_name, plugin_has_ui, plugin_dir, plugin_manifest
    pp = Path(plugin_path)

    # ── Folder plugin ──
    if pp.is_dir():
        manifest = _read_plugin_json(str(pp))
        if manifest is None:
            notify_host("log", {"level":3,"msg":f"Invalid plugin.json in {plugin_path}"})
            return False

        entry = os.path.join(str(pp), manifest["main"])
        mod_name = manifest.get("name", pp.name)

        plugin_dir = str(pp)
        plugin_manifest = manifest

        # ★ 全自动：创建本地 venv + 装依赖 + 注入 path
        if not _auto_deps(manifest, plugin_dir):
            notify_host("log", {"level":3,"msg":f"Dependency setup failed for '{mod_name}' — use Download first"})
            return False

        if plugin_dir not in sys.path:
            sys.path.insert(0, plugin_dir)

        spec = importlib.util.spec_from_file_location(mod_name, entry)
        if spec is None or spec.loader is None:
            notify_host("log", {"level":3,"msg":f"Cannot load: {entry}"})
            return False

        mod = importlib.util.module_from_spec(spec)
        sys.modules[mod_name] = mod
        mod._bridge_exit_event = ui_should_exit
        mod._bridge_call = call_host
        mod._plugin_dir = plugin_dir
        mod._plugin_manifest = manifest
        spec.loader.exec_module(mod)
        mod._bridge_exit_event = ui_should_exit
        mod._bridge_call = call_host
        mod._plugin_dir = plugin_dir
        mod._plugin_manifest = manifest
        plugin_module = mod
        plugin_has_ui = hasattr(mod, "run_ui")

        notify_host("plugin_info", {
            "name": mod_name,
            "version": manifest.get("version","1.0.0"),
            "author": manifest.get("author",""),
            "displayName": manifest.get("displayName",mod_name),
            "description": manifest.get("description","")
        })
        plugin_name = mod_name
        return True

    # ── Single file (legacy) ──
    if not pp.is_file():
        notify_host("log", {"level":3,"msg":f"Not found: {plugin_path}"})
        return False

    spec = importlib.util.spec_from_file_location("user_plugin", str(pp))
    if spec is None or spec.loader is None:
        notify_host("log", {"level":3,"msg":f"Cannot load: {plugin_path}"})
        return False

    mod = importlib.util.module_from_spec(spec)
    sys.modules["user_plugin"] = mod
    mod._bridge_exit_event = ui_should_exit
    mod._bridge_call = call_host
    spec.loader.exec_module(mod)
    mod._bridge_exit_event = ui_should_exit
    mod._bridge_call = call_host
    plugin_module = mod
    plugin_name = getattr(mod, "PLUGIN_NAME", pp.stem)
    plugin_has_ui = hasattr(mod, "run_ui")
    plugin_dir = None; plugin_manifest = None

    notify_host("plugin_info", {
        "name": plugin_name,
        "version": getattr(mod,"PLUGIN_VERSION","1.0.0"),
        "author": getattr(mod,"PLUGIN_AUTHOR","")
    })
    return True

# ── Entry ──
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(json.dumps({"jsonrpc":"2.0","error":{"code":-32602,"message":"Usage: python plugin_bridge.py <plugin_dir_or_file>"},"id":None}), flush=True)
        sys.exit(1)

    ok = load_plugin(sys.argv[1])

    threading.Thread(target=_stdin_reader, daemon=True).start()
    threading.Thread(target=_dispatch_worker, daemon=True).start()

    if not ok:
        # 依赖未就绪 — 桥继续存活，等用户 Download 后重新 Activate
        notify_host("log", {"level":2,"msg":"Plugin not ready — run Download first"})
        # 保持桥存活，等待后续 load 指令
        while True:
            try: line = _message_queue.get(timeout=1.0)
            except queue.Empty: continue
            try:
                resp = dispatch(line)
                if isinstance(resp, str):
                    with _stdout_lock:
                        try: print(resp, flush=True)
                        except (BrokenPipeError, BlockingIOError, OSError): pass
            except Exception:
                traceback.print_exc(file=sys.stderr)
    elif plugin_has_ui:
        while True:
            ui_enable_event.wait()
            ui_should_exit.clear()
            ui_enable_event.clear()
            try: plugin_module.run_ui()
            except Exception as e:
                notify_host("log", {"level":3,"msg":f"UI error: {e}"})
                traceback.print_exc(file=sys.stderr)
    else:
        threading.Thread(target=_stdin_reader, daemon=True).start()
        _dispatch_worker()
