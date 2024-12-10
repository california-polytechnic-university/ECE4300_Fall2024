import cv2
import time
import paramiko
import threading
import os

# Global variables
cpu_temp = "Temp: N/A"  # Default temperature text
running = True  # Flag to control the thread execution
log_entries = []  # List to store log entries
last_log_time = time.time()  # Track the last log time
log_interval = 3  # Log every 3 seconds

# Replace with your Raspberry Pi's IP address and credentials
pi_ip = '0.0.0.0'
username = 'pi_username'
password = 'pi_password'
video_stream_url = f'http://{pi_ip}:5000/video_feed'

# Function to get Raspberry Pi CPU temperature over SSH
def get_cpu_temperature():
    global cpu_temp, running
    while running:
        try:
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(pi_ip, username=username, password=password)

            # Fetch the temperature
            stdin, stdout, stderr = ssh.exec_command("cat /sys/class/thermal/thermal_zone0/temp")
            temp = stdout.read().decode('utf-8').strip()
            ssh.close()

            # Convert temperature to Celsius
            if temp.isdigit():
                cpu_temp = f"{int(temp) / 1000:.1f}°C"
            else:
                cpu_temp = f"Unexpected output: {temp}"
        except paramiko.AuthenticationException:
            cpu_temp = "Error: Authentication failed"
            print("Authentication failed. Stopping temperature thread.")
            running = False
        except Exception as e:
            cpu_temp = f"Error: {str(e)}"
            print(f"Exception occurred: {str(e)}")
        time.sleep(1)

# Initialize the camera stream
cap = cv2.VideoCapture(video_stream_url)

# Optional: Set smaller resolution for better performance
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# Frame per second calculation
prev_frame_time = 0

# Start the temperature-fetching thread
temp_thread = threading.Thread(target=get_cpu_temperature, daemon=True)
temp_thread.start()

# Main loop
processing_mode = 'none'  # Default processing mode

# Create a named window to allow resizing
cv2.namedWindow('Camera Stream', cv2.WINDOW_NORMAL)
cv2.resizeWindow('Camera Stream', 800, 600)

try:
    while True:
        # Capture frame-by-frame
        ret, frame = cap.read()

        if not ret:
            print("Failed to grab frame")
            break

        # Record the start time for processing
        start_time = time.time()

        # Process the frame based on the selected mode
        if processing_mode == 'grayscale':
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        elif processing_mode in ['remove_red', 'remove_green', 'remove_blue']:
            color_index = {'remove_red': 2, 'remove_green': 1, 'remove_blue': 0}
            frame[:, :, color_index[processing_mode]] = 0
        elif processing_mode == 'negative':
            frame = cv2.bitwise_not(frame)

        # Calculate FPS
        new_frame_time = time.time()
        fps = 1 / (new_frame_time - prev_frame_time)
        prev_frame_time = new_frame_time
        fps_text = f"FPS: {int(fps)}"

        # Prepare temperature text
        temp_text = f"Temp: {cpu_temp}"

        # Overlay text on the frame
        cv2.putText(frame, fps_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        cv2.putText(frame, temp_text, (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        cv2.putText(frame, f"Mode: {processing_mode}", (10, 110), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)

        # Display the resulting frame
        cv2.imshow('Camera Stream', frame)

        # Check if it's time to log data
        current_time = time.time()
        if current_time - last_log_time >= log_interval:
            # Log execution time, FPS, temperature, and mode
            execution_time = (current_time - start_time) * 1000  # Convert to ms
            log_entries.append(
                f"FPS: {int(fps)} | Execution Time: {execution_time:.6f} ms | CPU Temp: {cpu_temp} | Mode: {processing_mode}"
            )
            last_log_time = current_time  # Update the last log time

        # Check for user input
        key = cv2.waitKey(1) & 0xFF
        if key == ord('y'):
            processing_mode = 'grayscale'
        elif key == ord('r'):
            processing_mode = 'remove_red'
        elif key == ord('g'):
            processing_mode = 'remove_green'
        elif key == ord('b'):
            processing_mode = 'remove_blue'
        elif key == ord('n'):
            processing_mode = 'negative'
        elif key == ord(' '):  # Reset to normal on spacebar
            processing_mode = 'none'
        elif key == ord('q'):
            running = False  # Stop the temperature-fetching thread
            break

finally:
    # Clean up resources
    cap.release()
    running = False  # Stop the temperature thread
    temp_thread.join()  # Wait for the temperature thread to finish
    cv2.destroyAllWindows()

    # Write log entries to a file
    log_file_path = "python_event_log.txt"
    total_program_time = time.time() - last_log_time

    with open(log_file_path, "w") as log_file:
        log_file.write(f"Program Execution Time: {total_program_time:.4f} seconds\n")
        log_file.write("Performance Metrics:\n")
        for entry in log_entries:
            log_file.write(f"{entry}\n")

    print(f"Event log saved to '{log_file_path}'.")

    # Open the log file automatically
    if os.name == 'nt':  # For Windows
        os.startfile(log_file_path)
    elif os.name == 'posix':  # For macOS and Linux
        os.system(f'xdg-open {log_file_path}')
