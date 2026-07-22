# counting/main.py — 目标计数插件
# ==================================
# 不修改原文件，仅做包装。依赖 plugin_bridge 自动安装。
PLUGIN_NAME = "counting"

import sys, os, time, threading
from pathlib import Path

# ── 确保当前目录（plugins/counting/）在 sys.path，方便 import 原模块 ──
_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)

_model = None        # {cylinder, roi}
_count_lock = threading.Lock()
_last_result = {}    # 最新计数结果
_count_request = []  # 待处理请求队列: list of (image_path, callback_dict_or_queue)

DEFAULT_CYLINDER_MODEL = os.path.join(_here, "model_counting", "cylinder_segmenter", "weights", "best.pt")
DEFAULT_ROI_MODEL      = os.path.join(_here, "model_counting1", "roi_segmenter", "weights", "best.pt")

# ── RPC ──
def _rpc(method, params=None):
    try: return _bridge_call(method, params or {}, timeout=5.0)
    except Exception: return None

def _notify(method, params=None):
    """发送 notification（无返回值，不等待响应）"""
    import json as _json
    try:
        msg = _json.dumps({"jsonrpc": "2.0", "method": method, "params": params or {}})
        sys.stdout.write(msg + "\n")
        sys.stdout.flush()
    except Exception:
        pass

count_results_dir = os.path.join(_here, "count_results")

# ═══════════════════════════════════════════════════
# 模型加载（延迟，避免启动慢）
# ═══════════════════════════════════════════════════

def _load_models():
    """加载 YOLO 模型（首次调用时导入 ultralytics）。"""
    global _model
    if _model is not None:
        return _model

    from ultralytics import YOLO

    cylinder_path = os.path.join(_here, DEFAULT_CYLINDER_MODEL)
    roi_path      = os.path.join(_here, DEFAULT_ROI_MODEL)

    model = {
        "cylinder": YOLO(cylinder_path) if os.path.isfile(cylinder_path) else None,
        "roi":      YOLO(roi_path)      if os.path.isfile(roi_path)      else None,
    }

    if model["cylinder"] is None and model["roi"] is None:
        raise FileNotFoundError(
            f"No model found at {cylinder_path} or {roi_path}. "
            "Please download YOLO weights first."
        )

    _model = model
    return model


# ═══════════════════════════════════════════════════
# 核心计数逻辑（复用原模块的函数，不修改原文件）
# ═══════════════════════════════════════════════════

def count_from_image(image_path, device="cpu", conf=0.20, iou=0.45):
    """对单张图片计数。返回 dict: {valid: int, total_predicted: float, ...}"""
    import numpy as np
    import cv2

    # 延迟导入原模块函数
    from count_images_in_red_roi import (
        run_model, names_dict, is_cylinder_class, largest_roi_polygon,
        point_inside_roi, predicted_total, draw_label
    )

    models = _load_models()
    if models["cylinder"] is None:
        return {"error": "Cylinder model not found"}

    img = cv2.imread(image_path)
    if img is None:
        return {"error": f"Cannot read image: {image_path}"}

    # 圆柱检测
    cylinder_result = run_model(models["cylinder"], img, conf, iou, device)

    # ROI 检测（可选）
    roi_poly = None
    if models["roi"] is not None:
        roi_result = run_model(models["roi"], img, 0.25, iou, device)
        roi_poly = largest_roi_polygon(roi_result)

    # 计数：圆柱中心是否在 ROI 内
    valid_count = 0
    total_count = 0
    cls_ids = cylinder_result.boxes.cls.cpu().numpy().astype(int) if cylinder_result.boxes is not None else []

    if cylinder_result.boxes is not None:
        boxes = cylinder_result.boxes.xyxy.cpu().numpy()
        ann = img.copy()  # 标注图层
        for i, cls_id in enumerate(cls_ids):
            if not is_cylinder_class(cylinder_result, cls_id):
                continue
            x1, y1, x2, y2 = map(int, boxes[i])
            cx = (boxes[i][0] + boxes[i][2]) / 2.0
            cy = (boxes[i][1] + boxes[i][3]) / 2.0
            total_count += 1
            inside = roi_poly is None or point_inside_roi(cx, cy, roi_poly)
            if inside: valid_count += 1
            color = (0, 255, 0) if inside else (0, 0, 255)
            cv2.rectangle(ann, (x1, y1), (x2, y2), color, 2)
            draw_label(ann, f"{valid_count}" if inside else "X", (x1, y1 - 8), color, 0.8, 1)
        # 画 ROI 多边形
        if roi_poly is not None:
            cv2.polylines(ann, [roi_poly.astype(np.int32)], True, (255, 0, 0), 2)
        # 保存标注图
        ann_path = os.path.join(count_results_dir, "counted_" + os.path.basename(image_path))
        os.makedirs(count_results_dir, exist_ok=True)
        cv2.imwrite(ann_path, ann)

    # 预测总数（基于采样面积比例）
    effective = valid_count if roi_poly is not None else total_count
    predicted = predicted_total(effective, type("Args", (), {
        "sample_area": 2500, "total_area": 14400
    })())

    result = {
        "valid_cylinder_count": valid_count,
        "total_cylinder_count": total_count,
        "has_roi": roi_poly is not None,
        "predicted_total": round(predicted, 1),
        "image": os.path.basename(image_path),
    }
    if 'ann_path' in locals():
        result["annotated"] = ann_path
    return result


# ═══════════════════════════════════════════════════
# 插件生命周期
# ═══════════════════════════════════════════════════

def on_load():
    """Check model files and preload, report progress."""
    _notify("progress", {"status": "init", "msg": "Checking model files..."})

    cylinder_ok = os.path.isfile(DEFAULT_CYLINDER_MODEL)
    roi_ok      = os.path.isfile(DEFAULT_ROI_MODEL)

    status = []
    if cylinder_ok: status.append("cylinder OK")
    else:           status.append("cylinder MISSING")
    if roi_ok:      status.append("roi OK")
    else:           status.append("roi MISSING")

    print(f"[Counting] Models: {', '.join(status)}", file=sys.stderr, flush=True)

    if not cylinder_ok and not roi_ok:
        _notify("progress", {"status": "error", "msg": "No model weights found!"})
        return False

    # Preload models in background
    _notify("progress", {"status": "loading_models", "msg": "Loading YOLO models..."})
    def _preload():
        try:
            _load_models()
            print("[Counting] Models loaded", file=sys.stderr, flush=True)
            _notify("progress", {"status": "models_ready", "msg": "Models loaded"})
        except Exception as e:
            print(f"[Counting] Model load failed: {e}", file=sys.stderr, flush=True)
            _notify("progress", {"status": "error", "msg": f"Model load failed: {e}"})
    threading.Thread(target=_preload, daemon=True).start()

    return True


def on_enable(): return True
def on_disable(): return True
def on_unload(): return True

def on_update(params=None):
    """每帧检查是否有新计数请求。"""
    global _count_request, _last_result
    with _count_lock:
        if not _count_request:
            return True
        req = _count_request.pop(0)

    image_path = req["path"]
    device     = req.get("device", None)
    conf       = req.get("conf", 0.20)
    iou        = req.get("iou", 0.45)

    try:
        result = count_from_image(image_path, device, conf, iou)
    except Exception as e:
        result = {"error": str(e)}

    with _count_lock:
        _last_result = result

    # 如果有回调队列，把结果放入
    cb = req.get("callback")
    if cb is not None:
        cb.append(result)

    return True


# ═══════════════════════════════════════════════════
# 对外接口（供 RPC 调用或外部脚本）
# ═══════════════════════════════════════════════════

def count(image_path, device=None, conf=0.20, iou=0.45, timeout=10.0):
    """同步计数：直接调用，阻塞等待结果。
    用法（在其他 Python 代码中）:
        result = count("D:/images/frame_0228.jpg")
    """
    global _count_request, _last_result

    # 如果在主程序中（有 _bridge_call），走异步队列
    try:
        _bridge_call
        result_queue = []
        with _count_lock:
            _count_request.append({
                "path": image_path,
                "device": device,
                "conf": conf,
                "iou": iou,
                "callback": result_queue,
            })
        # 等待 on_update 处理
        deadline = time.time() + timeout
        while time.time() < deadline:
            if result_queue:
                return result_queue[0]
            time.sleep(0.05)
        return {"error": "Timeout"}
    except NameError:
        # 独立运行模式
        return count_from_image(image_path, device, conf, iou)


# ═══════════════════════════════════════════════════
# 独立运行: python main.py D:/images/test.jpg
# ═══════════════════════════════════════════════════

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python main.py <image_path>")
        print("  e.g.: python main.py D:/images/test.jpg")
        sys.exit(1)

    result = count_from_image(sys.argv[1])
    import json
    print(json.dumps(result, indent=2, ensure_ascii=False))


# ═══════════════════════════════════════════════════
# GUI — 点启用弹窗，选图片 → 计数 → 看结果
# ═══════════════════════════════════════════════════

class CountingApp:
    def __init__(self):
        self._last_img_path = None   # 被标注的临时图片路径

    def run(self):
        import tkinter as tk
        from tkinter import ttk, filedialog, messagebox
        self.tk = tk
        self.ttk = ttk
        self.filedialog = filedialog
        self.messagebox = messagebox

        self.root = tk.Tk()
        self.root.title("Object Counting")
        self.root.geometry("800x600")
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._build()
        self.root.mainloop()

    def _on_close(self):
        # 清理临时标注图片
        if self._last_img_path and os.path.isfile(self._last_img_path):
            try: os.remove(self._last_img_path)
            except Exception: pass
        self.root.destroy()

    # ── 布局 ──
    def _build(self):
        # 顶部：文件选择
        top = self.ttk.Frame(self.root)
        top.pack(fill=self.tk.X, padx=8, pady=(8, 0))
        self.ttk.Label(top, text="Image:").pack(side=self.tk.LEFT)
        self._path_var = self.tk.StringVar()
        self.ttk.Entry(top, textvariable=self._path_var, width=60).pack(side=self.tk.LEFT, padx=4, fill=self.tk.X, expand=True)
        self.ttk.Button(top, text="Browse", command=self._browse).pack(side=self.tk.LEFT, padx=2)

        # 参数栏
        param = self.ttk.Frame(self.root)
        param.pack(fill=self.tk.X, padx=8, pady=4)

        self.ttk.Label(param, text="Conf:").pack(side=self.tk.LEFT)
        self._conf_var = self.tk.StringVar(value="0.20")
        self.ttk.Entry(param, textvariable=self._conf_var, width=6).pack(side=self.tk.LEFT, padx=2)

        self.ttk.Label(param, text="IoU:").pack(side=self.tk.LEFT, padx=(12, 0))
        self._iou_var = self.tk.StringVar(value="0.45")
        self.ttk.Entry(param, textvariable=self._iou_var, width=6).pack(side=self.tk.LEFT, padx=2)

        self.ttk.Label(param, text="Device:").pack(side=self.tk.LEFT, padx=(12, 0))
        self._device_var = self.tk.StringVar(value="cpu")
        self.ttk.Entry(param, textvariable=self._device_var, width=8).pack(side=self.tk.LEFT, padx=2)

        self._count_btn = self.ttk.Button(param, text="Count", command=self._do_count)
        self._count_btn.pack(side=self.tk.RIGHT, padx=4)

        # 主区域：左右分栏
        pane = self.ttk.PanedWindow(self.root, orient=self.tk.HORIZONTAL)
        pane.pack(fill=self.tk.BOTH, expand=True, padx=8, pady=8)

        # 左侧：图片预览
        left = self.ttk.Frame(pane)
        pane.add(left, weight=1)
        self.ttk.Label(left, text="Preview", font=("", 9, "bold")).pack(anchor=self.tk.W)
        self._img_canvas = self.tk.Canvas(left, bg="#1a1a1a", highlightthickness=0)
        self._img_canvas.pack(fill=self.tk.BOTH, expand=True)

        # 右侧：结果
        right = self.ttk.Frame(pane, width=280)
        pane.add(right, weight=0)
        self.ttk.Label(right, text="Results", font=("", 9, "bold")).pack(anchor=self.tk.W)
        self._result_text = self.tk.Text(right, font=("Consolas", 11), bg="#1e1e1e",
                                          fg="#d4d4d4", width=32, wrap=self.tk.WORD,
                                          state=self.tk.DISABLED)
        self._result_text.pack(fill=self.tk.BOTH, expand=True, pady=(2, 0))

        # 底部状态
        self._status_var = self.tk.StringVar(value="Ready — select an image and click Count")
        self.ttk.Label(self.root, textvariable=self._status_var,
                        foreground="gray", font=("", 8)).pack(fill=self.tk.X, padx=8, pady=(0, 4))

    # ── 动作 ──
    def _browse(self):
        path = self.filedialog.askopenfilename(
            title="Select Image",
            filetypes=[("Images", "*.jpg *.jpeg *.png *.bmp *.webp"), ("All files", "*.*")]
        )
        if path:
            self._path_var.set(path)
            self._show_preview(path)

    def _show_preview(self, path):
        try:
            import cv2
            img = cv2.imread(path)
            if img is None: return
            # 缩放适应 canvas
            max_w = self._img_canvas.winfo_width() or 600
            max_h = self._img_canvas.winfo_height() or 400
            h, w = img.shape[:2]
            scale = min(max_w / w, max_h / h, 1.0)
            nw, nh = int(w * scale), int(h * scale)
            img = cv2.resize(img, (nw, nh))
            img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

            from PIL import Image, ImageTk
            pil = Image.fromarray(img)
            self._tk_img = ImageTk.PhotoImage(pil)

            self._img_canvas.delete("all")
            cx, cy = max_w // 2, max_h // 2
            self._img_canvas.create_image(cx, cy, image=self._tk_img, anchor=self.tk.CENTER)
        except Exception:
            pass

    def _do_count(self):
        path = self._path_var.get().strip()
        if not path or not os.path.isfile(path):
            self.messagebox.showwarning("Warning", "Select a valid image first")
            return

        try:
            conf = float(self._conf_var.get() or 0.20)
            iou  = float(self._iou_var.get() or 0.45)
        except ValueError:
            self.messagebox.showwarning("Warning", "Conf and IoU must be numbers")
            return
        dev = self._device_var.get().strip() or None

        self._count_btn.configure(state=self.tk.DISABLED)
        self._status_var.set("Running YOLO...")
        self._result_text.configure(state=self.tk.NORMAL)
        self._result_text.delete("1.0", self.tk.END)
        self._result_text.insert("1.0", "Running... please wait")
        self._result_text.configure(state=self.tk.DISABLED)

        def _run():
            try:
                r = count_from_image(path, dev, conf, iou)
            except Exception as e:
                import traceback
                traceback.print_exc(file=sys.stderr)
                r = {"error": str(e)}
            self.root.after(0, lambda: self._show_result(r))

        threading.Thread(target=_run, daemon=True).start()

    def _show_result(self, result):
        self._count_btn.configure(state=self.tk.NORMAL)
        self._result_text.configure(state=self.tk.NORMAL)
        self._result_text.delete("1.0", self.tk.END)
        if "error" in result:
            self._result_text.insert("1.0", f"ERROR: {result['error']}")
            self._status_var.set("Failed")
        else:
            lines = [
                f"Image:              {result.get('image', '?')}",
                f"",
                f"Valid (in ROI):     {result.get('valid_cylinder_count', '?')}",
                f"Total cylinders:    {result.get('total_cylinder_count', '?')}",
                f"ROI detected:       {'Yes' if result.get('has_roi') else 'No'}",
                f"Predicted total:    {result.get('predicted_total', '?')}",
            ]
            self._result_text.insert("1.0", "\n".join(lines))
            self._status_var.set(f"Done — {result.get('valid_cylinder_count', '?')} valid")

            # 显示标注后的处理图
            annotated = result.get("annotated")
            if annotated and os.path.isfile(annotated):
                self._last_img_path = annotated
                self._show_preview(annotated)
        self._result_text.configure(state=self.tk.DISABLED)


def run_ui():
    """由 plugin_bridge 后台线程调用启动 GUI"""
    try:
        CountingApp().run()
    except Exception as e:
        import traceback
        msg = traceback.format_exc()
        print(msg, file=sys.stderr, flush=True)
        try: _notify("progress", {"status": "error", "msg": f"UI failed: {e}"})
        except: pass
