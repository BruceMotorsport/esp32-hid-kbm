"""
Screen Bot v2 — Screen + Webcam + ESP32 HID
Sees your screen, sees you via webcam, controls mouse/keyboard via ESP32
"""
import subprocess
import json
import time
import math
import random
import requests
import base64
import io
from pathlib import Path

try:
    import cv2
    import numpy as np
    HAS_CV = True
except ImportError:
    HAS_CV = False
    print("Installing opencv-python...")
    subprocess.run(["pip", "install", "opencv-python", "numpy"], check=True)
    import cv2
    import numpy as np

try:
    import pyautogui
except ImportError:
    print("Installing pyautogui...")
    subprocess.run(["pip", "install", "pyautogui"], check=True)
    import pyautogui


class ESP32Controller:
    """Controls ESP32 HID via HTTP"""
    
    def __init__(self, ip="192.168.1.64", port=80):
        self.base_url = f"http://{ip}:{port}"
        self.connected = False
        self.screen_w = 1920
        self.screen_h = 1080
    
    def check_status(self):
        try:
            r = requests.get(f"{self.base_url}/status", timeout=3)
            if r.status_code == 200:
                self.connected = True
                return r.json()
        except:
            self.connected = False
        return None
    
    def set_screen(self, w, h):
        self.screen_w = w
        self.screen_h = h
        try:
            requests.get(f"{self.base_url}/screen", 
                        params={"width": w, "height": h}, timeout=3)
        except:
            pass
    
    def tap(self, x, y):
        requests.get(f"{self.base_url}/tap", params={"x": int(x), "y": int(y)}, timeout=3)
    
    def double_tap(self, x, y):
        requests.get(f"{self.base_url}/double_tap", params={"x": int(x), "y": int(y)}, timeout=3)
    
    def long_press(self, x, y, duration=1000):
        requests.get(f"{self.base_url}/long_press", 
                     params={"x": int(x), "y": int(y), "duration": duration}, timeout=5)
    
    def swipe(self, x1, y1, x2, y2, steps=20, delay=12):
        requests.get(f"{self.base_url}/swipe",
                     params={"x1": int(x1), "y1": int(y1), 
                            "x2": int(x2), "y2": int(y2),
                            "steps": steps, "delay": delay}, timeout=5)
    
    def type_text(self, text):
        requests.get(f"{self.base_url}/type", params={"text": text}, timeout=5)
    
    def key(self, key_name):
        requests.get(f"{self.base_url}/key", params={"key": key_name}, timeout=3)
    
    def scroll(self, amount):
        requests.get(f"{self.base_url}/scroll", params={"amount": int(amount)}, timeout=3)
    
    def batch(self, commands):
        requests.get(f"{self.base_url}/batch", params={"cmd": commands}, timeout=10)


class WebcamCapture:
    """Capture and analyze webcam feed"""
    
    def __init__(self, camera_index=0):
        self.camera_index = camera_index
        self.cap = None
        self.running = False
        self.face_cascade = cv2.CascadeClassifier(
            'haarcascade_frontalface_default.xml'
        )
    
    def start(self):
        """Start webcam capture"""
        self.cap = cv2.VideoCapture(self.camera_index)
        if not self.cap.isOpened():
            print(f"ERROR: Could not open webcam {self.camera_index}")
            return False
        self.running = True
        print(f"Webcam {self.camera_index} started")
        return True
    
    def stop(self):
        """Stop webcam capture"""
        self.running = False
        if self.cap:
            self.cap.release()
    
    def get_frame(self):
        """Get a single frame from webcam"""
        if not self.cap or not self.cap.isOpened():
            return None
        ret, frame = self.cap.read()
        return frame if ret else None
    
    def detect_faces(self, frame=None):
        """Detect faces in current frame"""
        if frame is None:
            frame = self.get_frame()
        if frame is None:
            return []
        
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = self.face_cascade.detectMultiScale(gray, 1.3, 5)
        return faces
    
    def save_frame(self, filename="webcam.jpg", frame=None):
        """Save current frame to file"""
        if frame is None:
            frame = self.get_frame()
        if frame is not None:
            cv2.imwrite(filename, frame)
            return True
        return False
    
    def frame_to_base64(self, frame=None):
        """Convert frame to base64 for sending to vision API"""
        if frame is None:
            frame = self.get_frame()
        if frame is None:
            return None
        _, buffer = cv2.imencode('.jpg', frame)
        return base64.b64encode(buffer).decode('utf-8')
    
    def look_for(self, target, template_path, confidence=0.8):
        """Look for a template in webcam feed"""
        frame = self.get_frame()
        if frame is None:
            return None
        
        template = cv2.imread(template_path)
        if template is None:
            return None
        
        result = cv2.matchTemplate(frame, template, cv2.TM_CCOEFF_NORMED)
        min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(result)
        
        if max_val >= confidence:
            h, w = template.shape[:2]
            return {
                "x": max_loc[0] + w // 2,
                "y": max_loc[1] + h // 2,
                "confidence": max_val,
                "frame_shape": frame.shape
            }
        return None


class HumanMouse:
    """Simulates human-like mouse movement"""
    
    def __init__(self, esp32: ESP32Controller):
        self.esp32 = esp32
        self.cur_x = 0
        self.cur_y = 0
    
    def _bezier_curve(self, start, end, control, steps=20):
        points = []
        for i in range(steps + 1):
            t = i / steps
            x = (1-t)**2 * start[0] + 2*(1-t)*t * control[0] + t**2 * end[0]
            y = (1-t)**2 * start[1] + 2*(1-t)*t * control[1] + t**2 * end[1]
            points.append((int(x), int(y)))
        return points
    
    def move_to(self, x, y, speed="normal"):
        if speed == "fast":
            steps = random.randint(8, 12)
            delay = 6
        elif speed == "slow":
            steps = random.randint(25, 35)
            delay = 15
        else:
            steps = random.randint(15, 25)
            delay = random.randint(8, 14)
        
        ctrl_x = (self.cur_x + x) / 2 + random.randint(-100, 100)
        ctrl_y = (self.cur_y + y) / 2 + random.randint(-50, 50)
        
        points = self._bezier_curve(
            (self.cur_x, self.cur_y), (x, y), (ctrl_x, ctrl_y), steps
        )
        
        for px, py in points:
            self.esp32.batch(f"move:{px - self.cur_x},{py - self.cur_y};wait:{delay}")
            self.cur_x = px
            self.cur_y = py
            time.sleep(random.uniform(0.005, 0.015))
    
    def tap(self, x, y, speed="normal"):
        self.move_to(x, y, speed)
        time.sleep(random.uniform(0.05, 0.15))
        self.esp32.tap(x, y)
        time.sleep(random.uniform(0.03, 0.08))
    
    def double_tap(self, x, y, speed="normal"):
        self.move_to(x, y, speed)
        time.sleep(random.uniform(0.05, 0.12))
        self.esp32.double_tap(x, y)
    
    def type_text(self, text, wpm=60):
        delay_per_char = 60 / (wpm * 5)
        for char in text:
            time.sleep(delay_per_char + random.uniform(-0.02, 0.02))
            self.esp32.type_text(char)
    
    def swipe(self, x1, y1, x2, y2, speed="normal"):
        self.move_to(x1, y1, speed)
        time.sleep(random.uniform(0.05, 0.1))
        self.esp32.swipe(x1, y1, x2, y2)


class ScreenBot:
    """Main bot — Screen + Webcam + ESP32"""
    
    def __init__(self, esp32_ip="192.168.1.64", webcam_index=0):
        self.esp32 = ESP32Controller(esp32_ip)
        self.mouse = HumanMouse(self.esp32)
        self.webcam = WebcamCapture(webcam_index)
        self.screen_w, self.screen_h = pyautogui.size()
        
        # Set ESP32 screen size
        if self.esp32.check_status():
            self.esp32.set_screen(self.screen_w, self.screen_h)
            print(f"ESP32 connected — Screen: {self.screen_w}x{self.screen_h}")
        else:
            print("ESP32 not found — running offline")
    
    # ===== SCREEN OPERATIONS =====
    
    def screenshot(self, region=None):
        img = pyautogui.screenshot(region=region)
        return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
    
    def find_on_screen(self, template_path, confidence=0.8):
        screenshot = self.screenshot()
        template = cv2.imread(template_path)
        if template is None:
            return None
        
        result = cv2.matchTemplate(screenshot, template, cv2.TM_CCOEFF_NORMED)
        min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(result)
        
        if max_val >= confidence:
            h, w = template.shape[:2]
            return {"x": max_loc[0] + w // 2, "y": max_loc[1] + h // 2, "confidence": max_val}
        return None
    
    def grid_search(self, template_path, grid_size=5, confidence=0.8):
        screenshot = self.screenshot()
        template = cv2.imread(template_path)
        if template is None:
            return None
        
        h, w = template.shape[:2]
        cell_w = self.screen_w // grid_size
        cell_h = self.screen_h // grid_size
        best_match = None
        best_val = 0
        
        for row in range(grid_size):
            for col in range(grid_size):
                x1 = col * cell_w
                y1 = row * cell_h
                x2 = min(x1 + cell_w + w, self.screen_w)
                y2 = min(y1 + cell_h + h, self.screen_h)
                region = screenshot[y1:y2, x1:x2]
                if region.shape[0] < h or region.shape[1] < w:
                    continue
                result = cv2.matchTemplate(region, template, cv2.TM_CCOEFF_NORMED)
                _, max_val, _, max_loc = cv2.minMaxLoc(result)
                if max_val > best_val and max_val >= confidence:
                    best_val = max_val
                    best_match = {"x": x1 + max_loc[0] + w // 2, "y": y1 + max_loc[1] + h // 2, "confidence": max_val}
        return best_match
    
    def wait_for(self, template_path, timeout=30, interval=0.5, confidence=0.8):
        start = time.time()
        while time.time() - start < timeout:
            match = self.find_on_screen(template_path, confidence)
            if match:
                return match
            time.sleep(interval)
        return None
    
    # ===== WEBCAM OPERATIONS =====
    
    def start_webcam(self):
        return self.webcam.start()
    
    def stop_webcam(self):
        self.webcam.stop()
    
    def see_user(self):
        """Get current webcam frame as base64 for vision API"""
        return self.webcam.frame_to_base64()
    
    def user_present(self):
        """Check if a person is visible in webcam"""
        faces = self.webcam.detect_faces()
        return len(faces) > 0
    
    def look_at_user(self):
        """Capture and describe what's in the webcam"""
        frame = self.webcam.get_frame()
        if frame is None:
            return "No webcam feed"
        
        # Save for vision analysis
        path = "C:/Users/Bruce/Desktop/re-maps/esp32-hid/webcam_capture.jpg"
        cv2.imwrite(path, frame)
        return path
    
    # ===== COMBINED OPERATIONS =====
    
    def screenshot_and_send(self):
        """Take screenshot and save for vision API"""
        img = self.screenshot()
        path = "C:/Users/Bruce/Desktop/re-maps/esp32-hid/screen_capture.jpg"
        cv2.imwrite(path, img)
        return path
    
    def macro(self, actions):
        """Execute a list of actions"""
        for action in actions:
            cmd = action.get("action")
            if cmd == "tap":
                self.mouse.tap(action["x"], action["y"], action.get("speed", "normal"))
            elif cmd == "type":
                self.mouse.type_text(action["text"], action.get("wpm", 60))
            elif cmd == "key":
                self.esp32.key(action["key"])
            elif cmd == "wait":
                time.sleep(action["seconds"])
            elif cmd == "swipe":
                self.mouse.swipe(action["x1"], action["y1"], action["x2"], action["y2"])
            elif cmd == "find_and_click":
                match = self.find_on_screen(action["template"], action.get("confidence", 0.8))
                if match:
                    self.mouse.tap(match["x"], match["y"])
            elif cmd == "scroll":
                self.esp32.scroll(action["amount"])
            elif cmd == "webcam_check":
                if not self.user_present():
                    print("User left — pausing")
                    while not self.user_present():
                        time.sleep(1)
                    print("User returned — resuming")
            time.sleep(random.uniform(0.1, 0.3))


# ===== MAIN =====
if __name__ == "__main__":
    import sys
    
    bot = ScreenBot("192.168.1.64")
    
    # Start webcam
    if bot.start_webcam():
        print("Webcam: ACTIVE")
    else:
        print("Webcam: FAILED")
    
    status = bot.esp32.check_status()
    if status:
        print(f"ESP32: {json.dumps(status, indent=2)}")
    else:
        print("ESP32 not found")
    
    print("\n=== Screen Bot v2 Ready ===")
    print("Commands:")
    print("  screenshot       - Capture screen")
    print("  webcam           - Capture webcam")
    print("  user             - Check if user visible")
    print("  tap x,y          - Tap at coordinates")
    print("  type text        - Type text")
    print("  find template    - Find image on screen")
    print("  grid template    - Grid search")
    print("  look             - See + describe")
    print("  quit             - Exit")
    
    while True:
        cmd = input("\n> ").strip()
        
        if cmd == "quit" or cmd == "exit":
            bot.stop_webcam()
            break
        elif cmd == "screenshot":
            path = bot.screenshot_and_send()
            print(f"Saved: {path}")
        elif cmd == "webcam":
            path = bot.look_at_user()
            print(f"Saved: {path}")
        elif cmd == "user":
            print("User visible:" if bot.user_present() else "No user")
        elif cmd.startswith("tap "):
            parts = cmd.split()[1].split(",")
            bot.mouse.tap(int(parts[0]), int(parts[1]))
            print(f"Tapped ({parts[0]}, {parts[1]})")
        elif cmd.startswith("type "):
            bot.mouse.type_text(cmd[5:])
            print(f"Typed: {cmd[5:]}")
        elif cmd.startswith("find "):
            match = bot.find_on_screen(cmd[5:])
            if match:
                print(f"Found at ({match['x']}, {match['y']}) — {match['confidence']:.2f}")
            else:
                print("Not found")
        elif cmd.startswith("grid "):
            match = bot.grid_search(cmd[5:])
            if match:
                print(f"Found at ({match['x']}, {match['y']}) — {match['confidence']:.2f}")
            else:
                print("Not found")
        elif cmd == "look":
            # Take both screenshots
            screen_path = bot.screenshot_and_send()
            webcam_path = bot.look_at_user()
            print(f"Screen: {screen_path}")
            print(f"Webcam: {webcam_path}")
        else:
            print("Unknown command")
