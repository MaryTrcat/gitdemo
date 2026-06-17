import json
import sys

import cv2
import mediapipe as mp
import numpy as np
from pathlib import Path


def model_path() -> str:
    script_dir = Path(__file__).resolve().parent
    candidates = [
        script_dir.parent / "models" / "hand_landmarker.task",
        script_dir / "models" / "hand_landmarker.task",
        Path.cwd() / "models" / "hand_landmarker.task",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return ""


def detect_image(image_path: str) -> dict:
    image_data = np.fromfile(image_path, dtype=np.uint8)
    image = cv2.imdecode(image_data, cv2.IMREAD_COLOR)
    if image is None:
        return {
            "success": False,
            "message": "无法读取摄像头画面图片。",
            "keypoints": [],
            "stability_deviation": 0.0,
        }

    return detect_frame(image)


def detect_frame(image) -> dict:
    task_model = model_path()
    if not task_model:
        return {
            "success": False,
            "message": "未找到 hand_landmarker.task 模型文件。",
            "keypoints": [],
            "stability_deviation": 0.0,
        }

    rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    vision = mp.tasks.vision
    options = vision.HandLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=task_model),
        running_mode=vision.RunningMode.IMAGE,
        num_hands=1,
        min_hand_detection_confidence=0.35,
        min_hand_presence_confidence=0.35,
        min_tracking_confidence=0.35,
    )

    with vision.HandLandmarker.create_from_options(options) as landmarker:
        result = landmarker.detect(mp_image)

    if not result.hand_landmarks:
        return {
            "success": False,
            "message": "MediaPipe 没有检测到手部。请把手放在画面中央、保证光线充足，并等待摄像头画面稳定后再评分。",
            "keypoints": [],
            "stability_deviation": 0.0,
        }

    landmarks = result.hand_landmarks[0]
    keypoints = [
        {"x": point.x, "y": point.y, "z": point.z}
        for point in landmarks
    ]

    return {
        "success": True,
        "message": "已通过 MediaPipe Hands 识别 21 个手部关键点。",
        "keypoints": keypoints,
        "stability_deviation": 0.04,
    }


def detect_video(video_path: str) -> dict:
    capture = cv2.VideoCapture(video_path)
    if not capture.isOpened():
        return {
            "success": False,
            "message": "无法读取视频文件。",
            "keypoints": [],
            "stability_deviation": 0.0,
        }

    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT) or 0)
    sample_indexes = []
    if frame_count > 0:
        sample_indexes = [int(frame_count * ratio) for ratio in (0.20, 0.35, 0.50, 0.65, 0.80)]
    else:
        sample_indexes = [0, 20, 40, 60, 80]

    best_result = None
    for index in sample_indexes:
        capture.set(cv2.CAP_PROP_POS_FRAMES, max(0, index))
        ok, frame = capture.read()
        if not ok or frame is None:
            continue
        result = detect_frame(frame)
        if result.get("success"):
            result["message"] = f"已从视频第 {index} 帧提取标准手势关键点。"
            capture.release()
            return result
        best_result = result

    capture.release()
    if best_result is not None:
        best_result["message"] = "视频中没有识别到清晰手部关键点，请选择手部完整、光线充足的视频。"
        return best_result
    return {
        "success": False,
        "message": "视频中没有可读取的有效画面。",
        "keypoints": [],
        "stability_deviation": 0.0,
    }


def main() -> int:
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "message": "缺少待检测图片路径。",
            "keypoints": [],
            "stability_deviation": 0.0,
        }, ensure_ascii=False))
        return 0

    if len(sys.argv) >= 3 and sys.argv[1] == "--video":
        result = detect_video(sys.argv[2])
    else:
        result = detect_image(sys.argv[1])

    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
