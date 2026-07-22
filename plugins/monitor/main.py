# monitor/main.py — Robot-UI State Monitor v3
# 用 tkinter 显示机器人状态曲线图
PLUGIN_NAME = "statemonitor"   # must match plugin.json "name"

import time, threading
from collections import defaultdict

# ── 内联 RPC + 数据提取（_bridge_call 由 plugin_bridge 注入）──
def _rpc(method, params=None):
    try: return _bridge_call(method, params or {}, timeout=3.0)
    except Exception: return None

def _extract(params, *keys):
    d = params
    for k in keys:
        if isinstance(d, dict): d = d.get(k, {})
        else: return {}
    return {k: v for k, v in d.items() if isinstance(v, (int, float))} if isinstance(d, dict) else {}

# ── 数据存储 ──
_start_time = time.time()
_on_update_count = 0

_lock = threading.Lock()
_history = defaultdict(list)
MAX_POINTS = 300

# ═══════════════════════════════════════════════════
# 插件生命周期
# ═══════════════════════════════════════════════════

def on_load():   return True
def on_enable(): return True
def on_unload(): return True

def on_disable():
    with _lock: _history.clear()
    threading.Thread(target=lambda: _rpc("unsubscribe", {"name": PLUGIN_NAME}), daemon=True).start()
    return True

def on_update(params=None):
    global _on_update_count
    _on_update_count += 1
    now = time.time() - _start_time

    with _lock:
        for name, val in _extract(params, "sensors").items():
            _history[f"sensors/{name}"].append((now, val))
        for name, val in _extract(params, "gamepad").items():
            _history[f"gamepad/{name}"].append((now, val))
        for name, val in _extract(params, "node_graph", "variables").items():
            _history[f"node_graph/variables/{name}"].append((now, val))
        for name, val in _extract(params, "node_graph", "outputs").items():
            _history[f"node_graph/outputs/{name}"].append((now, val))

        for path in list(_history.keys()):
            if len(_history[path]) > MAX_POINTS:
                _history[path] = _history[path][-MAX_POINTS:]
    return True


# ═══════════════════════════════════════════════════
# UI — MonitorApp 类
# ═══════════════════════════════════════════════════

def run_ui():
    MonitorApp().mainloop()


class MonitorApp:
    COLORS = ["#FF6B6B","#4ECDC4","#45B7D1","#96CEB4","#FFEAA7",
              "#DDA0DD","#FFD700","#87CEEB","#FFA07A","#00CED1"]
    GROUPS = ["gamepad", "sensors", "node_graph"]

    def __init__(self):
        import tkinter as tk
        from tkinter import ttk
        self.tk, self.ttk = tk, ttk
        self.root = tk.Tk()
        self.root.title("Robot State Monitor")
        self.root.geometry("1100x650")
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        self.paths   = []
        self.chosen  = set()
        self.sub_set = frozenset()
        self.paused  = tk.BooleanVar(value=False)

        self._checks = {}
        self._labels = {}

        self._build()
        self.root.after(500, self._tick)
        self._init_ng_source()

    def mainloop(self): self.root.mainloop()
    def _on_close(self): self.root.destroy()

    def _build(self):
        hpaned = self.ttk.PanedWindow(self.root, orient=self.tk.HORIZONTAL)
        hpaned.pack(fill=self.tk.BOTH, expand=True, padx=4, pady=4)
        self._left(hpaned)
        self._right(hpaned)

    def _left(self, parent):
        lf = self.ttk.Frame(parent, width=300)
        parent.add(lf, weight=0)

        self._status_var = self.tk.StringVar(value="Click Refresh")
        self.ttk.Label(lf, text="Variables", font=("", 11, "bold")).pack(anchor=self.tk.W)
        self.ttk.Label(lf, textvariable=self._status_var, foreground="gray").pack(anchor=self.tk.W)

        self._ng_src = self.tk.StringVar(value="evaluator")
        f = self.ttk.Frame(lf); f.pack(fill=self.tk.X, pady=(2, 0))
        self.ttk.Label(f, text="NG Source:", font=("", 8)).pack(side=self.tk.LEFT)
        self.ttk.Radiobutton(f, text="Eval", variable=self._ng_src, value="evaluator",
                              command=lambda: self._switch_ng("evaluator")).pack(side=self.tk.LEFT, padx=2)
        self.ttk.Radiobutton(f, text="Edit", variable=self._ng_src, value="editor",
                              command=lambda: self._switch_ng("editor")).pack(side=self.tk.LEFT, padx=2)

        bf = self.ttk.Frame(lf); bf.pack(fill=self.tk.X, pady=(4, 2))
        self.ttk.Button(bf, text="Refresh", command=self._refresh).pack(side=self.tk.LEFT, padx=2)
        self.ttk.Button(bf, text="All", command=self._all).pack(side=self.tk.LEFT, padx=2)
        self.ttk.Button(bf, text="None", command=self._none).pack(side=self.tk.LEFT, padx=2)

        cv = self.tk.Canvas(lf)
        sb = self.ttk.Scrollbar(lf, orient=self.tk.VERTICAL, command=cv.yview)
        self._list_frame = self.ttk.Frame(cv)
        win = cv.create_window((0, 0), window=self._list_frame, anchor=self.tk.NW)
        cv.configure(yscrollcommand=sb.set)
        cv.pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True); sb.pack(side=self.tk.RIGHT, fill=self.tk.Y)
        cv.bind("<Enter>", lambda e: cv.bind_all("<MouseWheel>", lambda e: cv.yview_scroll(int(-e.delta/120), "units")))
        cv.bind("<Leave>", lambda e: cv.unbind_all("<MouseWheel>"))
        self._list_frame.bind("<Configure>", lambda e: cv.configure(scrollregion=cv.bbox("all")))
        cv.bind("<Configure>", lambda e: cv.itemconfig(win, width=e.width))

    def _right(self, parent):
        rf = self.ttk.Frame(parent); parent.add(rf, weight=1)
        self._info_var = self.tk.StringVar()
        self.ttk.Label(rf, textvariable=self._info_var, foreground="#999",
                        font=("Consolas", 8)).pack(anchor=self.tk.W, fill=self.tk.X)
        self._chart = self.tk.Canvas(rf, bg="#1a1a2e", highlightthickness=0)
        self._chart.pack(fill=self.tk.BOTH, expand=True)
        cf = self.ttk.Frame(rf); cf.pack(fill=self.tk.X, pady=2)
        self.ttk.Button(cf, text="Clear", command=_history.clear).pack(side=self.tk.LEFT, padx=4)
        self.ttk.Checkbutton(cf, text="Pause", variable=self.paused).pack(side=self.tk.LEFT, padx=4)

    def _switch_ng(self, v):
        _rpc("set_option", {"key": "ng_source", "value": v})
        self._refresh()

    def _init_ng_source(self):
        r = _rpc("get_option", {"key": "ng_source"})
        if r: self._ng_src.set(r)

    def _refresh(self):
        self._status_var.set("Refreshing...")
        def _do():
            r = _rpc("list_paths")
            if r:
                self.paths[:] = sorted(r)
                self.root.after(0, self._on_paths)
            else:
                self.root.after(0, lambda: self._status_var.set("No data"))
        threading.Thread(target=_do, daemon=True).start()

    def _on_paths(self):
        self._status_var.set(f"OK: {len(self.paths)} paths")
        self._rebuild_list()

    def _all(self):
        self.chosen.update(self.paths)
        for p, bv in self._checks.items(): bv.set(True)

    def _none(self):
        self.chosen.clear()
        for bv in self._checks.values(): bv.set(False)

    def _rebuild_list(self):
        for w in self._list_frame.winfo_children(): w.destroy()
        self._checks.clear(); self._labels.clear()

        groups = {}
        for p in self.paths:
            g = p.split("/")[0]
            groups.setdefault(g, []).append(p)

        for gn in self.GROUPS:
            for p in sorted(groups.get(gn, [])):
                row = self.ttk.Frame(self._list_frame); row.pack(fill=self.tk.X, padx=(8, 0))
                bv = self.tk.BooleanVar(value=p in self.chosen)
                bv.trace_add("write", lambda *a, p=p, v=bv:
                             (self.chosen.add(p) if v.get() else self.chosen.discard(p)))
                self.ttk.Checkbutton(row, variable=bv).pack(side=self.tk.LEFT)
                parts = p.split("/")
                lab = parts[-1] if len(parts) < 3 else f"{parts[-2]}.{parts[-1]}"
                self.ttk.Label(row, text=lab, font=("Consolas", 9)).pack(side=self.tk.LEFT, padx=2)
                vl = self.tk.StringVar()
                self.ttk.Label(row, textvariable=vl, foreground="#888",
                                font=("Consolas", 9)).pack(side=self.tk.LEFT, padx=6)
                self._checks[p] = bv; self._labels[p] = vl

    def _draw(self):
        cur = sorted(self.chosen)
        cv = self._chart
        if not cur:
            cv.delete("all")
            cv.create_text(200, 100, text="(Select variables on the left)", fill="#555", font=("", 12))
            return

        draw = cur[:8]
        with _lock:
            series = {v: list(_history.get(v, [])) for v in draw}

        all_t, all_y = [], []
        for pts in series.values():
            for t, y in pts: all_t.append(t); all_y.append(y)

        cv.delete("all")
        w, h = cv.winfo_width(), cv.winfo_height()
        if w < 20 or not all_t or len(all_t) < 2:
            return

        y_min, y_max = min(all_y), max(all_y)
        if y_min == y_max: y_min -= 0.5; y_max += 0.5
        t_min, t_max = min(all_t), max(all_t)
        if t_min == t_max: t_max += 1
        M = 35
        tx = lambda t: M + (t - t_min) / (t_max - t_min) * (w - 2 * M)
        ty = lambda y: h - M - (y - y_min) / (y_max - y_min) * (h - 2 * M)

        for i in range(5):
            xp, yp = tx(t_min + (t_max - t_min) * i / 4), ty(y_min + (y_max - y_min) * i / 4)
            cv.create_line(xp, M, xp, h - M, fill="#2a2a3e")
            cv.create_line(M, yp, w - M, yp, fill="#2a2a3e")
            cv.create_text(xp, h - 16, text=f"{t_min+(t_max-t_min)*i/4:.1f}", fill="#666", font=("", 7))
            cv.create_text(M - 3, yp, text=f"{y_min+(y_max-y_min)*i/4:.2f}",
                           anchor=self.tk.E, fill="#666", font=("", 7))

        for i, v in enumerate(draw):
            pts = series.get(v, [])
            if len(pts) < 2: continue
            cc = [c for t, y in pts for c in (tx(t), ty(y))]
            cv.create_line(*cc, fill=self.COLORS[i % len(self.COLORS)], width=1.5)
            cv.create_text(w - 8, 8 + i * 14, text=v.rsplit("/", 1)[-1],
                           fill=self.COLORS[i % len(self.COLORS)], anchor=self.tk.E, font=("", 8))

    def _tick(self):
        if not self.root.winfo_exists(): return

        cur = sorted(self.chosen)
        with _lock:
            for p in cur:
                h = _history.get(p, [])
                if h:
                    self._labels.get(p, self.tk.StringVar()).set(f"= {h[-1][1]:.4f}")

        self._status_var.set(f"x{_on_update_count} | {len(self.paths)}v | "
                             f"{len(self.chosen)}sel | {sum(1 for v in _history.values() if v)}d")

        cur_set = frozenset(cur)
        if cur_set != self.sub_set:
            self.sub_set = cur_set
            def _sync():
                ps = list(cur_set)
                if ps:
                    _rpc("subscribe", {"name": PLUGIN_NAME, "paths": ps, "interval_sec": 0.033})
                else:
                    _rpc("unsubscribe", {"name": PLUGIN_NAME})
            threading.Thread(target=_sync, daemon=True).start()

        if not self.paused.get():
            self._draw()
        self._info_var.set(f"{len(cur)} checked, x{_on_update_count}")
        self.root.after(100, self._tick)
