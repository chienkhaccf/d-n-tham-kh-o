import pickle
import cv2
import mediapipe as mp
import numpy as np
import RPi.GPIO as GPIO
import time


# Cấu hình GPIO cho Relay 1 (đèn)
RELAY_PIN_DEN = 17  # GPIO17 (Pin 11) - Thay đổi nếu cần
GPIO.setmode(GPIO.BCM)
GPIO.setup(RELAY_PIN_DEN, GPIO.OUT)
GPIO.output(RELAY_PIN_DEN, GPIO.LOW)  # Ban đầu tắt đèn

# Cấu hình GPIO cho Relay 2 (quạt)
RELAY_PIN_QUAT = 27 # GPIO27 (Pin 13) - Thay đổi nếu cần, phải khác chân đèn
GPIO.setup(RELAY_PIN_QUAT, GPIO.OUT)
GPIO.output(RELAY_PIN_QUAT, GPIO.LOW) # Ban đầu tắt quạt

# Load model đã huấn luyện
model_dict = pickle.load(open('./model3.p', 'rb'))
model = model_dict['model3']

# Camera
cap = cv2.VideoCapture(0)

# Mediapipe
mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils
mp_drawing_styles = mp.solutions.drawing_styles
hands = mp_hands.Hands(static_image_mode=True, min_detection_confidence=0.3)

labels_dict = {0: 'TAT_DEN', 1: 'BAT_DEN', 2: 'TAT_QUAT', 3: 'BAT_QUAT'}
last_command_den = None  # Theo dõi trạng thái đèn
last_command_quat = None # Theo dõi trạng thái quạt

try:
    while True:
        data_aux = []
        x_, y_ = [], []
        ret, frame = cap.read()
        H, W, _ = frame.shape
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        results = hands.process(frame_rgb)
        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                mp_drawing.draw_landmarks(
                    frame, hand_landmarks,
                    mp_hands.HAND_CONNECTIONS,
                    mp_drawing_styles.get_default_hand_landmarks_style(),
                    mp_drawing_styles.get_default_hand_connections_style()
                )

                for i in range(len(hand_landmarks.landmark)):
                    x = hand_landmarks.landmark[i].x
                    y = hand_landmarks.landmark[i].y
                    x_.append(x)
                    y_.append(y)

                for i in range(len(hand_landmarks.landmark)):
                    x = hand_landmarks.landmark[i].x
                    y = hand_landmarks.landmark[i].y
                    data_aux.append(x - min(x_))
                    data_aux.append(y - min(y_))

                x1 = int(min(x_) * W) - 10
                y1 = int(min(y_) * H) - 10
                x2 = int(max(x_) * W) - 10
                y2 = int(max(y_) * H) - 10

                prediction = model.predict([np.asarray(data_aux)])
                predicted_label = int(prediction[0])
                predicted_character = labels_dict[predicted_label]

                # Hiển thị lên màn hình
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 0), 4)
                cv2.putText(frame, predicted_character, (x1, y1 - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 0, 0), 3, cv2.LINE_AA)

                # Điều khiển Relay
                if predicted_character == 'BAT_DEN' and predicted_character != last_command_den:
                    GPIO.output(RELAY_PIN_DEN, GPIO.HIGH)
                    print("Đèn BẬT")
                    last_command_den = predicted_character
                elif predicted_character == 'TAT_DEN' and predicted_character != last_command_den:
                    GPIO.output(RELAY_PIN_DEN, GPIO.LOW)
                    print("Đèn TẮT")
                    last_command_den = predicted_character
                elif predicted_character == 'BAT_QUAT' and predicted_character != last_command_quat:
                    GPIO.output(RELAY_PIN_QUAT, GPIO.HIGH)
                    print("Quạt BẬT")
                    last_command_quat = predicted_character
                elif predicted_character == 'TAT_QUAT' and predicted_character != last_command_quat:
                    GPIO.output(RELAY_PIN_QUAT, GPIO.LOW)
                    print("Quạt TẮT")
                    last_command_quat = predicted_character

        cv2.imshow('frame', frame)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('c'):
            break

finally:
    cap.release()
    cv2.destroyAllWindows()
    GPIO.cleanup()