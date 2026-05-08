import cv2
import mediapipe as mp
import math


class HandDetector:
    def __init__(self, max_hands=2, detection_conf=0.7, tracking_conf=0.5):
        self.mp_hands = mp.solutions.hands
        self.mp_draw = mp.solutions.drawing_utils
        self.hands = self.mp_hands.Hands(
            max_num_hands=max_hands,
            min_detection_confidence=detection_conf,
            min_tracking_confidence=tracking_conf
        )
        self.results = None

    def find_hands(self, frame, draw=True):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.results = self.hands.process(rgb)

        if self.results.multi_hand_landmarks:
            for hand_lms in self.results.multi_hand_landmarks:
                if draw:
                    self.mp_draw.draw_landmarks(
                        frame, hand_lms, self.mp_hands.HAND_CONNECTIONS
                    )
        return frame

    def get_positions(self, frame, hand_index=0):
        """Returns list of (id, x, y, z) for each landmark of the specified hand."""
        positions = []
        if self.results and self.results.multi_hand_landmarks:
            if hand_index < len(self.results.multi_hand_landmarks):
                hand = self.results.multi_hand_landmarks[hand_index]
                h, w, _ = frame.shape
                for lm_id, lm in enumerate(hand.landmark):
                    cx, cy = int(lm.x * w), int(lm.y * h)
                    positions.append((lm_id, cx, cy, lm.z))
        return positions

    def hand_count(self):
        if self.results and self.results.multi_hand_landmarks:
            return len(self.results.multi_hand_landmarks)
        return 0

    def get_gesture(self, frame, hand_index=0):
        positions = self.get_positions(frame, hand_index)
        if len(positions) < 21:
            return None

        lm = {id: (x, y, z) for id, x, y, z in positions}

        # --- Key landmarks ---
        thumb_tip = lm[4]   # (x, y, z)
        finger_tips = [lm[8], lm[12], lm[16], lm[20]]

        # Average position of the 4 finger tips
        avg_finger_x = sum(t[0] for t in finger_tips) / 4
        avg_finger_y = sum(t[1] for t in finger_tips) / 4

        # Displacement of thumb tip from average finger tip position
        dx = thumb_tip[0] - avg_finger_x   # +ve = thumb is right of fingers
        dy = thumb_tip[1] - avg_finger_y   # +ve = thumb is below fingers (image coords)

        # --- Hand scale: wrist (0) to middle MCP (9) ---
        wrist  = lm[0]
        mid_mcp = lm[9]
        hand_scale = math.hypot(mid_mcp[0] - wrist[0], mid_mcp[1] - wrist[1])
        if hand_scale == 0:
            return None

        # Normalize displacements by hand scale
        ndx = dx / hand_scale
        ndy = dy / hand_scale

        VERT_THRESH  = 0.6
        HORIZ_THRESH = 0.6

        abs_dx = abs(ndx)
        abs_dy = abs(ndy)

        # Dominant axis determines gesture family
        if abs_dx > abs_dy:
            # Horizontal gesture
            if ndx < -HORIZ_THRESH:
                return "thumbs_left"
            elif ndx > HORIZ_THRESH:
                return "thumbs_right"
        else:
            # Vertical gesture
            if ndy < -VERT_THRESH:
                return "thumbs_up"
            elif ndy > VERT_THRESH:
                return "thumbs_down"

        return None