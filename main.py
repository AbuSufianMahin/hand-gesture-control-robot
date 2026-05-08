import cv2
import time
import math
import threading
import socket
from hand_detector import HandDetector

ESP32_IP   = "192.168.137.143"
UDP_PORT   = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

GESTURE_COMMANDS = {
    "thumbs_up":    "F",
    "thumbs_down":  "B",
    "thumbs_left":  "L",
    "thumbs_right": "R",
}

GESTURE_LABELS = {
    "thumbs_up":    ("Forward",  (0, 200, 100)),
    "thumbs_down":  ("Backward", (0, 80, 255)),
    "thumbs_left":  ("Left",     (255, 180, 0)),
    "thumbs_right": ("Right",    (255, 100, 200)),
}

DEBOUNCE_SECONDS   = 0.5
REPEAT_INTERVAL    = 0.05
FINGERTIP_IDS      = [4, 8, 12, 16, 20]
MOVEMENT_THRESHOLD = 12

def send_command(action):
    def _send():
        try:
            sock.sendto(action.encode(), (ESP32_IP, UDP_PORT))
            print(f"[CMD] Sent: {action}")
        except Exception as e:
            print(f"[CMD] Error: {e}")
    threading.Thread(target=_send, daemon=True).start()

def send_stop():
    send_command("S")

def landmark_centroid(positions):
    if not positions:
        return None
    xs = [x for _, x, y, z in positions]
    ys = [y for _, x, y, z in positions]
    return (sum(xs) / len(xs), sum(ys) / len(ys))

def log_fingertips(gesture, positions):
    lm = {id: (x, y, z) for id, x, y, z in positions}
    label = gesture if gesture else "NO_GESTURE"
    print(f"\n[STABLE HAND: {label}]")

def main():
    capture  = cv2.VideoCapture(0)
    detector = HandDetector(max_hands=1, detection_conf=0.5, tracking_conf=0.3)

    stable_since            = None
    last_centroid           = None
    logged_for_current_hold = False
    last_sent_gesture       = None
    last_sent_time          = None

    while True:
        success, frame = capture.read()
        if not success:
            break

        frame = cv2.flip(frame, 1)

        detector.find_hands(frame, draw=False)
        if detector.hand_count() > 1:
            cv2.putText(frame, "1 hand only", (10, 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 60, 255), 1)
            if last_sent_gesture is not None:
                send_stop()
                last_sent_gesture = None
            stable_since  = None
            last_centroid = None
            last_sent_time = None
            logged_for_current_hold = False
            cv2.imshow("Hand Detection", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            continue

        frame     = detector.find_hands(frame, draw=True)
        positions = detector.get_positions(frame)
        gesture   = detector.get_gesture(frame)
        now       = time.time()
        centroid  = landmark_centroid(positions)

        if centroid is None:
            if last_sent_gesture is not None:
                send_stop()
                last_sent_gesture = None
            stable_since  = None
            last_centroid = None
            last_sent_time = None
            logged_for_current_hold = False
        else:
            if last_centroid is None:
                stable_since = now
                last_sent_time = None
                logged_for_current_hold = False
            else:
                dist = math.hypot(centroid[0] - last_centroid[0],
                                  centroid[1] - last_centroid[1])
                if dist > MOVEMENT_THRESHOLD:
                    if last_sent_gesture is not None:
                        send_stop()
                        last_sent_gesture = None
                    stable_since = now
                    last_sent_time = None
                    logged_for_current_hold = False

            last_centroid = centroid
            held_duration = now - stable_since

            if held_duration >= DEBOUNCE_SECONDS:
                if not logged_for_current_hold:
                    log_fingertips(gesture, positions)
                    logged_for_current_hold = True

                    if gesture in GESTURE_COMMANDS:
                        send_command(GESTURE_COMMANDS[gesture])
                        last_sent_gesture = gesture
                        last_sent_time = now
                    else:
                        # Unrecognized gesture → stop
                        if last_sent_gesture is not None:
                            send_stop()
                            last_sent_gesture = None
                else:
                    if gesture in GESTURE_COMMANDS:
                        if gesture != last_sent_gesture:
                            log_fingertips(gesture, positions)
                            send_command(GESTURE_COMMANDS[gesture])
                            last_sent_gesture = gesture
                            last_sent_time = now
                        else:
                            if now - last_sent_time >= REPEAT_INTERVAL:
                                send_command(GESTURE_COMMANDS[gesture])
                                last_sent_time = now
                    else:
                        # Unrecognized gesture → stop
                        if last_sent_gesture is not None:
                            send_stop()
                            last_sent_gesture = None
                        stable_since = now
                        logged_for_current_hold = False

            # Progress bar
            progress = min(held_duration / DEBOUNCE_SECONDS, 1.0)
            bar_w = int(120 * progress)
            cv2.rectangle(frame, (10, 28), (130, 38), (60, 60, 60), -1)
            cv2.rectangle(frame, (10, 28), (10 + bar_w, 38), (0, 220, 120), -1)
            cv2.putText(frame, f"{min(held_duration, DEBOUNCE_SECONDS):.1f}s",
                        (135, 38), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (180, 180, 180), 1)

        # Gesture label — only show if recognized
        if gesture and gesture in GESTURE_LABELS:
            label, color = GESTURE_LABELS[gesture]
            cv2.putText(frame, label, (10, 22),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
        elif gesture:
            cv2.putText(frame, "Unknown", (10, 22),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (100, 100, 100), 2)

        # Active robot command
        if last_sent_gesture and last_sent_gesture in GESTURE_LABELS:
            _, color = GESTURE_LABELS[last_sent_gesture]
            cv2.putText(frame, f"ROBOT: {GESTURE_COMMANDS[last_sent_gesture].upper()}",
                        (10, frame.shape[0] - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)

        # Fingertip circles
        for lm_id, cx, cy, cz in positions:
            if lm_id in FINGERTIP_IDS:
                cv2.circle(frame, (cx, cy), 6, (0, 255, 0), cv2.FILLED)

        cv2.imshow("Hand Detection", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            send_stop()
            sock.close()
            break

    capture.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
