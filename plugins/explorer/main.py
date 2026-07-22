# main.py — Robot-UI Variable Explorer (tkinter GUI)
# ===================================================
# Drop into plugins/ directory, enable to launch the GUI.
# No internal field names required!
#
# GUI features:
#   - Variables tab: all available variables + live values
#   - Data Tree tab: tree view of data structure
#   - Plugin Code tab: copy template code

PLUGIN_NAME = "CLIExplorer"
PLUGIN_VERSION = "1.0.0"
PLUGIN_AUTHOR = "Robot-UI"

import sys
import threading
import traceback
from collections import OrderedDict

# ═══════════════════════════════════════════════════════
# Inline RPC + data extraction
# ═══════════════════════════════════════════════════════

def _rpc(method, params=None):
    try:
        return _bridge_call(method, params or {}, timeout=3.0)
    except Exception:
        return None

def _list_paths():
    r = _rpc("list_paths")
    return r if isinstance(r, list) else []

def _get_state(paths=None):
    return _rpc("get_state", {"paths": paths or []})

def _subscribe(paths, interval=0.033):
    return _rpc("subscribe", {
        "name": PLUGIN_NAME,
        "paths": paths,
        "interval_sec": interval
    })

def _extract(params, *keys):
    d = params
    for k in keys:
        if isinstance(d, dict):
            d = d.get(k, {})
        else:
            return {}
    if isinstance(d, dict):
        return {k: v for k, v in d.items() if isinstance(v, (int, float))}
    return {}

# ═══════════════════════════════════════════════════════
# Template code
# ═══════════════════════════════════════════════════════

TEMPLATE_CODE = (
    "# my_plugin.py\n"
    "PLUGIN_NAME = \"MyPlugin\"\n"
    "PLUGIN_VERSION = \"1.0.0\"\n"
    "PLUGIN_AUTHOR = \"your name\"\n"
    "\n"
    "def _rpc(m, p=None):\n"
    "    try: return _bridge_call(m, p or {}, timeout=3.0)\n"
    "    except: return None\n"
    "\n"
    "def _extract(params, *keys):\n"
    "    d = params\n"
    "    for k in keys:\n"
    "        if isinstance(d, dict): d = d.get(k, {})\n"
    "        else: return {}\n"
    "    return {k: v for k, v in d.items() if isinstance(v, (int, float))} if isinstance(d, dict) else {}\n"
    "\n"
    "def on_load():\n"
    "    paths = _rpc('list_paths')\n"
    "    if paths:\n"
    "        _rpc('subscribe', {'name': PLUGIN_NAME, 'paths': paths, 'interval_sec': 0.033})\n"
    "    return True\n"
    "\n"
    "def on_enable(): return True\n"
    "def on_disable(): return True\n"
    "def on_unload(): return True\n"
    "\n"
    "def on_update(params):\n"
    "    for name, val in _extract(params, 'sensors').items():\n"
    "        print(f'sensor {name} = {val}')\n"
    "    for name, val in _extract(params, 'gamepad').items():\n"
    "        print(f'gamepad {name} = {val}')\n"
    "    return True\n"
)

# ═══════════════════════════════════════════════════════
# Shared state
# ═══════════════════════════════════════════════════════

_lock = threading.Lock()
_latest_values = {}          # path -> value
_on_update_count = 0

# ═══════════════════════════════════════════════════════
# Main GUI (tkinter)
# ═══════════════════════════════════════════════════════

import tkinter as tk
from tkinter import ttk

# Dark theme colors
BG_DARK      = "#2d2d30"
BG_DARKER    = "#1e1e1e"
FG_PRIMARY   = "#d4d4d4"
FG_SECONDARY = "#888888"
FG_GREEN     = "#22aa77"
ACCENT       = "#007acc"
BTN_BG       = "#3e3e42"
BTN_ACTIVE   = "#505050"

class ExplorerApp:
    def __init__(self):
        self._root = None
        self._status_var = None
        self._paths = []
        self._value_labels = {}       # path -> ttk.Label
        self._check_vars = {}         # path -> tk.BooleanVar
        self._tree_text = None        # tk.Text
        self._code_text = None        # tk.Text
        self._notebook = None
        self._var_frame = None
        self._canvas = None
        self._running = False

    # ── run_ui() — entry point called by plugin_bridge ──
    def run_ui(self):
        self._root = tk.Tk()
        self._root.title("Robot-UI Explorer")
        self._root.geometry("900x600")
        self._root.configure(bg=BG_DARK)
        self._root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._running = True

        self._setup_style()
        self._build()

        # Initial data load
        self._root.after(300, self._refresh)

        # Periodic tick
        self._tick()

        self._root.mainloop()
        self._running = False

    def _setup_style(self):
        style = ttk.Style()
        style.theme_use("clam")

        style.configure(".", background=BG_DARK, foreground=FG_PRIMARY, fieldbackground=BG_DARKER)
        style.configure("TLabel", background=BG_DARK, foreground=FG_PRIMARY)
        style.configure("TButton", background=BTN_BG, foreground=FG_PRIMARY,
                        borderwidth=1, focusthickness=0, padding=(8, 3))
        style.map("TButton", background=[("active", BTN_ACTIVE), ("pressed", ACCENT)])
        style.configure("TNotebook", background=BG_DARK, borderwidth=0)
        style.configure("TNotebook.Tab", background=BTN_BG, foreground=FG_PRIMARY,
                        padding=(16, 6), borderwidth=0)
        style.map("TNotebook.Tab",
                  background=[("selected", BG_DARKER), ("active", BTN_ACTIVE)],
                  foreground=[("selected", "#ffffff")])
        style.configure("TCheckbutton", background=BG_DARK, foreground=FG_PRIMARY)
        style.map("TCheckbutton", background=[("active", BG_DARK)])
        style.configure("TFrame", background=BG_DARK)

        # Scrollbar style
        style.configure("Vertical.TScrollbar", background=BTN_BG, troughcolor=BG_DARKER,
                        arrowcolor=FG_PRIMARY, borderwidth=0)
        style.map("Vertical.TScrollbar", background=[("active", BTN_ACTIVE)])

    # ── Layout ──
    def _build(self):
        # Top status bar
        top_frame = ttk.Frame(self._root)
        top_frame.pack(fill=tk.X, padx=8, pady=(8, 0))

        title_lbl = tk.Label(top_frame, text="Robot-UI Explorer", bg=BG_DARK, fg=FG_PRIMARY,
                             font=("TkDefaultFont", 13, "bold"))
        title_lbl.pack(side=tk.LEFT)

        self._status_var = tk.StringVar(value="Loading...")
        status_lbl = tk.Label(top_frame, textvariable=self._status_var, bg=BG_DARK,
                              fg=FG_SECONDARY, font=("TkDefaultFont", 9))
        status_lbl.pack(side=tk.RIGHT)

        # Tab widget
        self._notebook = ttk.Notebook(self._root)
        self._notebook.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        self._tab_vars = ttk.Frame(self._notebook)
        self._tab_tree = ttk.Frame(self._notebook)
        self._tab_code = ttk.Frame(self._notebook)
        self._notebook.add(self._tab_vars, text="Variables")
        self._notebook.add(self._tab_tree, text="Data Tree")
        self._notebook.add(self._tab_code, text="Plugin Code")

        self._build_vars_tab()
        self._build_tree_tab()
        self._build_code_tab()

    # ── Tab 1: Variables ──
    def _build_vars_tab(self):
        # Button bar
        bf = ttk.Frame(self._tab_vars)
        bf.pack(fill=tk.X, padx=4, pady=4)

        ttk.Button(bf, text="Refresh", command=self._refresh).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(bf, text="Select All", command=self._select_all).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(bf, text="Select None", command=self._select_none).pack(side=tk.LEFT)

        # Scrollable area: Canvas + Scrollbar + inner Frame
        scroll_frame = ttk.Frame(self._tab_vars)
        scroll_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        self._canvas = tk.Canvas(scroll_frame, bg=BG_DARK, highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(scroll_frame, orient=tk.VERTICAL, command=self._canvas.yview)
        self._var_frame = ttk.Frame(self._canvas)

        self._var_frame.bind("<Configure>",
            lambda e: self._canvas.configure(scrollregion=self._canvas.bbox("all")))

        self._canvas.create_window((0, 0), window=self._var_frame, anchor="nw",
                                   tags="var_frame")
        self._canvas.configure(yscrollcommand=scrollbar.set)

        self._canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Mouse wheel scrolling
        def _on_mousewheel(event):
            self._canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        self._canvas.bind("<Enter>", lambda e: self._canvas.bind_all("<MouseWheel>", _on_mousewheel))
        self._canvas.bind("<Leave>", lambda e: self._canvas.unbind_all("<MouseWheel>"))

        # Update canvas width when resized
        self._canvas.bind("<Configure>", self._on_canvas_configure)

    def _on_canvas_configure(self, event):
        self._canvas.itemconfig("var_frame", width=event.width)

    # ── Data fetch (RPC in background thread, schedules UI update via after) ──
    def _refresh(self):
        def _do():
            r = _list_paths()
            if self._root and self._running:
                self._root.after(0, lambda: self._on_refresh(r))

        threading.Thread(target=_do, daemon=True).start()

    def _on_refresh(self, paths):
        if not paths:
            self._status_var.set("No data - open a project")
            return
        self._paths = sorted(paths)
        self._status_var.set(f"{len(self._paths)} variables")
        self._rebuild_var_list()
        self._update_tree()

    def _select_all(self):
        self._rebuild_var_list(checked=set(self._paths))

    def _select_none(self):
        self._rebuild_var_list(checked=set())

    def _rebuild_var_list(self, checked=None):
        # Clear existing
        for widget in self._var_frame.winfo_children():
            widget.destroy()
        self._value_labels.clear()
        self._check_vars.clear()

        if checked is None:
            checked = set(self._paths)

        # Group by category
        groups = OrderedDict()
        for p in self._paths:
            g = p.split("/")[0]
            groups.setdefault(g, []).append(p)

        for cat, items in groups.items():
            # Category header
            cat_lbl = tk.Label(self._var_frame, text=cat.upper(), bg=BG_DARK,
                               fg=FG_SECONDARY, font=("TkDefaultFont", 9, "bold"),
                               anchor="w")
            cat_lbl.pack(fill=tk.X, padx=(16, 4), pady=(8, 1))

            for p in items:
                name = p.split("/", 1)[1] if "/" in p else p
                row = ttk.Frame(self._var_frame)
                row.pack(fill=tk.X, padx=(16, 4), pady=1)

                var = tk.BooleanVar(value=(p in checked))
                self._check_vars[p] = var
                cb = ttk.Checkbutton(row, variable=var)
                cb.pack(side=tk.LEFT)

                name_lbl = tk.Label(row, text=name, bg=BG_DARK, fg=FG_PRIMARY,
                                    font=("Consolas", 9), anchor="w", width=28)
                name_lbl.pack(side=tk.LEFT, padx=(4, 0))

                val_str = tk.StringVar(value="--")
                with _lock:
                    if p in _latest_values:
                        val_str.set(f"{_latest_values[p]:.4f}")

                val_lbl = tk.Label(row, textvariable=val_str, bg=BG_DARK, fg=FG_GREEN,
                                   font=("Consolas", 9, "bold"), anchor="e", width=10)
                val_lbl.pack(side=tk.RIGHT)
                self._value_labels[p] = val_str

    # ── Tab 2: Data Tree ──
    def _build_tree_tab(self):
        bf = ttk.Frame(self._tab_tree)
        bf.pack(fill=tk.X, padx=4, pady=4)

        ttk.Button(bf, text="Refresh", command=self._update_tree).pack(side=tk.LEFT)

        text_frame = ttk.Frame(self._tab_tree)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        self._tree_text = tk.Text(text_frame, bg=BG_DARKER, fg=FG_PRIMARY,
                                  font=("Consolas", 10), wrap="none",
                                  insertbackground=FG_PRIMARY, relief="flat",
                                  borderwidth=0, state="disabled")
        tree_scroll_y = ttk.Scrollbar(text_frame, orient=tk.VERTICAL,
                                      command=self._tree_text.yview)
        tree_scroll_x = ttk.Scrollbar(text_frame, orient=tk.HORIZONTAL,
                                      command=self._tree_text.xview)
        self._tree_text.configure(yscrollcommand=tree_scroll_y.set,
                                  xscrollcommand=tree_scroll_x.set)

        self._tree_text.grid(row=0, column=0, sticky="nsew")
        tree_scroll_y.grid(row=0, column=1, sticky="ns")
        tree_scroll_x.grid(row=1, column=0, sticky="ew")
        text_frame.grid_rowconfigure(0, weight=1)
        text_frame.grid_columnconfigure(0, weight=1)

    def _update_tree(self):
        def _do():
            state = _get_state()
            text = self._format_tree(state) if state else "(No data - open a project)"
            if self._root and self._running:
                self._root.after(0, lambda: self._set_tree(text))

        threading.Thread(target=_do, daemon=True).start()

    def _set_tree(self, text):
        if self._tree_text:
            self._tree_text.configure(state="normal")
            self._tree_text.delete("1.0", tk.END)
            self._tree_text.insert("1.0", text)
            self._tree_text.configure(state="disabled")

    def _format_tree(self, obj, indent=0):
        lines = []
        if isinstance(obj, dict):
            for k, v in obj.items():
                if isinstance(v, (int, float)):
                    lines.append(f"{' ' * indent}{k} = {v}")
                elif isinstance(v, dict):
                    lines.append(f"{' ' * indent}{k}/")
                    lines.append(self._format_tree(v, indent + 4))
                elif isinstance(v, list):
                    lines.append(f"{' ' * indent}{k}/ [{len(v)} items]")
                    for item in v[:10]:
                        if isinstance(item, dict):
                            nm = item.get("name", item.get("id", "?"))
                            lines.append(f"{' ' * (indent + 2)}{nm}")
        return "\n".join(lines)

    # ── Tab 3: Plugin Code ──
    def _build_code_tab(self):
        bf = ttk.Frame(self._tab_code)
        bf.pack(fill=tk.X, padx=4, pady=4)

        ttk.Button(bf, text="Copy to Clipboard", command=self._copy_code).pack(side=tk.LEFT)

        text_frame = ttk.Frame(self._tab_code)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        self._code_text = tk.Text(text_frame, bg=BG_DARKER, fg=FG_PRIMARY,
                                  font=("Consolas", 10), wrap="none",
                                  insertbackground=FG_PRIMARY, relief="flat",
                                  borderwidth=0, state="disabled")
        code_scroll_y = ttk.Scrollbar(text_frame, orient=tk.VERTICAL,
                                      command=self._code_text.yview)
        code_scroll_x = ttk.Scrollbar(text_frame, orient=tk.HORIZONTAL,
                                      command=self._code_text.xview)
        self._code_text.configure(yscrollcommand=code_scroll_y.set,
                                  xscrollcommand=code_scroll_x.set)

        self._code_text.grid(row=0, column=0, sticky="nsew")
        code_scroll_y.grid(row=0, column=1, sticky="ns")
        code_scroll_x.grid(row=1, column=0, sticky="ew")
        text_frame.grid_rowconfigure(0, weight=1)
        text_frame.grid_columnconfigure(0, weight=1)

        # Insert template code
        self._code_text.configure(state="normal")
        self._code_text.insert("1.0", TEMPLATE_CODE)
        self._code_text.configure(state="disabled")

    def _copy_code(self):
        code = self._code_text.get("1.0", tk.END).rstrip("\n")
        self._root.clipboard_clear()
        self._root.clipboard_append(code)
        self._status_var.set("Copied to clipboard!")

    # ── Periodic refresh ──
    def _tick(self):
        if not self._running:
            return
        with _lock:
            for path, val_var in self._value_labels.items():
                if path in _latest_values:
                    val_var.set(f"{_latest_values[path]:.4f}")
            count = _on_update_count
        self._status_var.set(f"{len(self._paths)} vars | updates: {count}")
        self._root.after(100, self._tick)

    # ── Window close ──
    def _on_close(self):
        self._running = False
        if self._root:
            self._root.destroy()


# ═══════════════════════════════════════════════════════
# Global instance
# ═══════════════════════════════════════════════════════

_app = None

# ═══════════════════════════════════════════════════════
# Plugin lifecycle
# ═══════════════════════════════════════════════════════

def on_load():
    paths = _list_paths()
    if paths:
        _subscribe(paths)
        print(f"[Explorer] Found {len(paths)} variables", file=sys.stderr, flush=True)
    else:
        print("[Explorer] No variables found (open a project first)", file=sys.stderr, flush=True)
    return True


def on_enable():
    global _app
    if _app is None:
        _app = ExplorerApp()
    print("[Explorer] Enabled", file=sys.stderr, flush=True)
    return True


def on_disable():
    global _app
    if _app is not None and _app._running:
        if _app._root:
            _app._root.after(0, _app._on_close)
    print("[Explorer] Disabled", file=sys.stderr, flush=True)
    return True


def on_unload():
    on_disable()
    global _app
    _app = None
    return True


def on_update(params=None):
    global _on_update_count
    _on_update_count += 1
    if params is None:
        return True
    with _lock:
        for name, val in _extract(params, "sensors").items():
            _latest_values[f"sensors/{name}"] = val
        for name, val in _extract(params, "gamepad").items():
            _latest_values[f"gamepad/{name}"] = val
        for name, val in _extract(params, "node_graph", "variables").items():
            _latest_values[f"node_graph/variables/{name}"] = val
        for name, val in _extract(params, "node_graph", "outputs").items():
            _latest_values[f"node_graph/outputs/{name}"] = val
    return True


# ═══════════════════════════════════════════════════════
# run_ui() — called by plugin_bridge when UI is enabled
# ═══════════════════════════════════════════════════════

def run_ui():
    """Entry point for plugin_bridge UI loop."""
    global _app
    if _app is None:
        _app = ExplorerApp()
    _app.run_ui()



def run_ui():
    """Entry point for plugin_bridge UI loop."""
    global _app
    if _app is None:
        _app = ExplorerApp()
    _app.run_ui()
