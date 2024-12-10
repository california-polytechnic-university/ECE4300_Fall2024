import cv2
import time
from flask import Flask, Response

app = Flask(__name__)

# Initialize camera (use the correct video device)
cap = cv2.VideoCapture(0)

# Optional: Set a smaller resolution to improve performance
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)

@app.route('/')
def index():
    return "Camera Streaming. Go to /video_feed to see the stream."

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

def generate_frames():
    while True:
        # Read a frame from the camera
        ret, frame = cap.read()
        if not ret:
            break

        # Optionally resize the frame for better performance
        # frame = cv2.resize(frame, (320, 240))

        # Encode the frame in JPEG format
        ret, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()

        # Yield the output frame in the required format
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

if __name__ == '__main__':
    # Ensure the camera is opened
    if not cap.isOpened():
        print("Error: Could not open camera.")
    else:
        # Start the Flask server
        app.run(host='0.0.0.0', port=5000, debug=False)

    # Release the camera on exit
    cap.release()
