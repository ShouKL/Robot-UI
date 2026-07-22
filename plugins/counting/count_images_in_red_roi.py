import argparse
import csv
from pathlib import Path

import cv2
import numpy as np


DEFAULT_CYLINDER_MODEL = "model_counting/cylinder_segmenter/weights/best.pt"
DEFAULT_ROI_MODEL = "model_counting1/roi_segmenter/weights/best.pt"
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="D:\\EV\\检测数量\\frames\\frame_0228.jpg", help="Input image or image folder. Default: current directory.")
    parser.add_argument("--cylinder-model", default=DEFAULT_CYLINDER_MODEL)
    parser.add_argument("--roi-model", default=DEFAULT_ROI_MODEL)
    parser.add_argument("--out", default="count_results")
    parser.add_argument("--cylinder-conf", type=float, default=0.20)
    parser.add_argument("--roi-conf", type=float, default=0.25)
    parser.add_argument("--iou", type=float, default=0.45)
    parser.add_argument("--device", default=None, help="Example: 0 or cpu. Empty means auto.")
    parser.add_argument("--show-outside", action="store_true")
    parser.add_argument("--no-popup", action="store_true")
    parser.add_argument("--window-width", type=int, default=1280)
    parser.add_argument("--label-scale", type=float, default=0.9)
    parser.add_argument("--count-scale", type=float, default=2.0)
    parser.add_argument("--total-area", type=float, default=14400.0)
    parser.add_argument("--sample-area", type=float, default=2500.0)
    parser.add_argument("--modenum", type=int, default=None, help="If provided, skip detection and use this number as valid_cylinder_count directly.")
    return parser.parse_args()


def run_model(model, image: np.ndarray, conf: float, iou: float, device: str | None):
    kwargs = {"conf": conf, "iou": iou, "verbose": False}
    if device:
        kwargs["device"] = device
    return model(image, **kwargs)[0]


def names_dict(result) -> dict[int, str]:
    names = result.names
    if isinstance(names, dict):
        return {int(k): str(v) for k, v in names.items()}
    return {i: str(v) for i, v in enumerate(names)}


def is_cylinder_class(result, cls_id: int) -> bool:
    names = names_dict(result)
    return len(names) == 1 or names.get(int(cls_id), "") == "white_cylinder"


def is_roi_class(result, cls_id: int) -> bool:
    names = names_dict(result)
    return len(names) == 1 or names.get(int(cls_id), "") in {"count_roi", "red_frame", "red_frame_roi"}


def largest_roi_polygon(roi_result) -> np.ndarray | None:
    if roi_result.boxes is None or len(roi_result.boxes) == 0:
        return None

    best_poly = None
    best_area = -1.0
    cls_ids = roi_result.boxes.cls.cpu().numpy().astype(int)

    if roi_result.masks is not None and roi_result.masks.xy is not None:
        for i, cls_id in enumerate(cls_ids):
            if not is_roi_class(roi_result, cls_id):
                continue
            poly = np.array(roi_result.masks.xy[i], dtype=np.float32)
            if len(poly) < 3:
                continue
            area = abs(cv2.contourArea(poly))
            if area > best_area:
                best_area = area
                best_poly = poly
        if best_poly is not None:
            return best_poly

    boxes = roi_result.boxes.xyxy.cpu().numpy()
    for i, cls_id in enumerate(cls_ids):
        if not is_roi_class(roi_result, cls_id):
            continue
        x1, y1, x2, y2 = boxes[i]
        poly = np.array([[x1, y1], [x2, y1], [x2, y2], [x1, y2]], dtype=np.float32)
        area = max(0.0, (x2 - x1) * (y2 - y1))
        if area > best_area:
            best_area = area
            best_poly = poly
    return best_poly


def point_inside_roi(cx: float, cy: float, roi_poly: np.ndarray) -> bool:
    return cv2.pointPolygonTest(roi_poly.astype(np.float32), (float(cx), float(cy)), False) >= 0


def predicted_total(count: int, args) -> float:
    if args.sample_area == 0:
        return 0.0
    return count * args.total_area / args.sample_area


def draw_label(
    image: np.ndarray,
    text: str,
    org: tuple[int, int],
    color: tuple[int, int, int],
    scale: float,
    thickness: int,
) -> None:
    cv2.putText(image, text, org, cv2.FONT_HERSHEY_SIMPLEX, scale, (0, 0, 0), thickness + 3, cv2.LINE_AA)
    cv2.putText(image, text, org, cv2.FONT_HERSHEY_SIMPLEX, scale, color, thickness, cv2.LINE_AA)


def make_display_image(image: np.ndarray, max_width: int) -> np.ndarray:
    if max_width <= 0 or image.shape[1] <= max_width:
        return image
    scale = max_width / image.shape[1]
    return cv2.resize(image, (max_width, int(image.shape[0] * scale)), interpolation=cv2.INTER_AREA)


def show_popup(image: np.ndarray, image_name: str, count: int, total: float, args) -> bool:
    if args.no_popup:
        return True
    display = make_display_image(image, args.window_width)
    window_name = f"{image_name} | count: {count} | predicted total: {total:.1f}"
    cv2.imshow(window_name, display)
    key = cv2.waitKey(0) & 0xFF
    cv2.destroyWindow(window_name)
    return key not in (27, ord("q"), ord("Q"))


def count_one_image(image_path: Path, cylinder_model, roi_model, args) -> tuple[int, float, Path, np.ndarray]:
    image = cv2.imread(str(image_path))
    if image is None:
        raise RuntimeError(f"Cannot read image: {image_path}")

    annotated = image.copy()
    roi_result = run_model(roi_model, image, args.roi_conf, args.iou, args.device)
    roi_poly = largest_roi_polygon(roi_result)

    # --- manual-count shortcut: skip cylinder detection, use user-supplied number ---
    if getattr(args, "modenum", None) is not None:
        count = args.modenum
        total = predicted_total(count, args)

        if roi_poly is not None:
            roi_i32 = roi_poly.astype(np.int32)
            overlay = annotated.copy()
            cv2.fillPoly(overlay, [roi_i32], (0, 0, 255))
            annotated = cv2.addWeighted(overlay, 0.18, annotated, 0.82, 0)
            cv2.polylines(annotated, [roi_i32], True, (0, 0, 255), 3)

        draw_label(annotated, f"valid cylinders: {count}", (30, 70), (0, 255, 255), args.count_scale, 4)
        draw_label(annotated, f"predicted total: {total:.1f}", (30, 145), (0, 255, 255), args.count_scale, 4)

        out_path = Path(args.out) / f"{image_path.stem}_count.jpg"
        cv2.imwrite(str(out_path), annotated)
        return count, total, out_path, annotated

    if roi_poly is None:
        count = 0
        total = predicted_total(count, args)
        draw_label(annotated, "ROI not found", (30, 70), (0, 0, 255), args.count_scale, 4)
        draw_label(annotated, f"valid cylinders: {count}", (30, 145), (0, 0, 255), args.count_scale, 4)
        draw_label(annotated, f"predicted total: {total:.1f}", (30, 220), (0, 0, 255), args.count_scale, 4)
        out_path = Path(args.out) / f"{image_path.stem}_count.jpg"
        cv2.imwrite(str(out_path), annotated)
        return count, total, out_path, annotated

    roi_i32 = roi_poly.astype(np.int32)
    overlay = annotated.copy()
    cv2.fillPoly(overlay, [roi_i32], (0, 0, 255))
    annotated = cv2.addWeighted(overlay, 0.18, annotated, 0.82, 0)
    cv2.polylines(annotated, [roi_i32], True, (0, 0, 255), 3)

    cyl_result = run_model(cylinder_model, image, args.cylinder_conf, args.iou, args.device)
    count = 0

    if cyl_result.boxes is not None and len(cyl_result.boxes) > 0:
        boxes = cyl_result.boxes.xyxy.cpu().numpy()
        cls_ids = cyl_result.boxes.cls.cpu().numpy().astype(int)
        confs = cyl_result.boxes.conf.cpu().numpy()

        for box, cls_id, conf in zip(boxes, cls_ids, confs):
            if not is_cylinder_class(cyl_result, cls_id):
                continue

            x1, y1, x2, y2 = box
            cx = (x1 + x2) / 2
            cy = (y1 + y2) / 2
            inside = point_inside_roi(cx, cy, roi_poly)

            if inside:
                count += 1
                color = (0, 255, 0)
            elif args.show_outside:
                color = (150, 150, 150)
            else:
                continue

            cv2.rectangle(annotated, (int(x1), int(y1)), (int(x2), int(y2)), color, 2)
            cv2.circle(annotated, (int(cx), int(cy)), 4, color, -1)
            draw_label(
                annotated,
                f"{conf:.2f}",
                (int(x1), max(30, int(y1) - 8)),
                color,
                args.label_scale,
                2,
            )

    total = predicted_total(count, args)
    draw_label(annotated, f"valid cylinders: {count}", (30, 70), (0, 255, 255), args.count_scale, 4)
    draw_label(annotated, f"predicted total: {total:.1f}", (30, 145), (0, 255, 255), args.count_scale, 4)

    out_path = Path(args.out) / f"{image_path.stem}_count.jpg"
    cv2.imwrite(str(out_path), annotated)
    return count, total, out_path, annotated


def iter_images(source: Path) -> list[Path]:
    if source.is_file():
        if source.suffix.lower() not in IMAGE_EXTS:
            raise ValueError(f"Unsupported image file: {source}")
        return [source]
    if source.is_dir():
        return [p for p in sorted(source.iterdir()) if p.suffix.lower() in IMAGE_EXTS]
    raise FileNotFoundError(f"Source not found: {source}")


def main() -> None:
    args = parse_args()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    images = iter_images(Path(args.source))

    if args.modenum is not None:
        import time
        time.sleep(5)

    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise RuntimeError("Install Ultralytics first: python -m pip install -U ultralytics") from exc

    cylinder_model = YOLO(args.cylinder_model)
    roi_model = YOLO(args.roi_model)

    csv_path = out_dir / "counts.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["image", "valid_cylinder_count", "predicted_total", "annotated_image"])

        for image_path in images:
            count, total, out_path, annotated = count_one_image(image_path, cylinder_model, roi_model, args)
            writer.writerow([str(image_path), count, f"{total:.3f}", str(out_path)])
            print(f"{image_path.name}: count={count}, predicted_total={total:.1f}")
            if not show_popup(annotated, image_path.name, count, total, args):
                break

    cv2.destroyAllWindows()
    print(f"Done. CSV: {csv_path}")


if __name__ == "__main__":
    main()
