import cv2
import time
import paramiko
import threading
import os  # Import the os module

# Global variable to store CPU temperature
cpu_temp = "Temp: N/A"
running = True  # Flag to control the thread execution
log_entries = []  # List to store log entries
last_log_time = time.time()  # Track last log time

# Replace with your Raspberry Pi's IP address
pi_ip = '0.0.0.0'  # Update with your Pi's actual IP address
video_stream_url = f'http://{pi_ip}:5000/video_feed'

# Function to get Raspberry Pi CPU temperature over SSH
def get_cpu_temperature():
    global cpu_temp, running  # Declare both variables as global
    while running:
        try:
            # SSH connection details
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(pi_ip, username='pi_username', password='pi_password')  # Replace with your credentials
            
            stdin, stdout, stderr = ssh.exec_command("vcgencmd measure_temp")
            temp = stdout.read().decode('utf-8').strip()
            ssh.close()
            
            # Check if the output is valid
            if "temp=" in temp:
                cpu_temp = temp.replace("temp=", "").strip()
            else:
                cpu_temp = "Error: Unexpected output"
        except paramiko.AuthenticationException:
            cpu_temp = "Error: Authentication failed"
            running = False  # Stop the loop on auth failure
        except Exception as e:
            cpu_temp = f"Error: {str(e)}"
        time.sleep(1)  # Fetch temperature every second

# Initialize camera
cap = cv2.VideoCapture(video_stream_url)

# Optional: Set a smaller resolution to improve performance
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# Frame per second calculation
prev_frame_time = 0

# Start the temperature fetching thread
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

        # Check for user input
        key = cv2.waitKey(1) & 0xFF
        if key == ord('y'):
            processing_mode = 'grayscale'
            log_entries.append(f"Changed mode to grayscale at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord('r'):
            processing_mode = 'remove_red'
            log_entries.append(f"Changed mode to remove red at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord('g'):
            processing_mode = 'remove_green'
            log_entries.append(f"Changed mode to remove green at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord('b'):
            processing_mode = 'remove_blue'
            log_entries.append(f"Changed mode to remove blue at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord('n'):
            processing_mode = 'negative'
            log_entries.append(f"Changed mode to negative at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord(' '):  # Reset to normal on spacebar
            processing_mode = 'none'
            log_entries.append(f"Reset to normal mode at {time.strftime('%Y-%m-%d %H:%M:%S')}")
        elif key == ord('q'):
            running = False  # Stop the temperature fetching thread
            break

        # Calculate and log execution time, fps, and temperature
        execution_time = time.time() - start_time
        if time.time() - last_log_time >= 3:
            log_entries.append(f"Execution Time for mode '{processing_mode}': {execution_time:.4f} seconds at {time.strftime('%Y-%m-%d %H:%M:%S')}")
            log_entries.append(f"FPS: {int(fps)} at {time.strftime('%Y-%m-%d %H:%M:%S')}")
            log_entries.append(f"Temperature: {cpu_temp} at {time.strftime('%Y-%m-%d %H:%M:%S')}")
            last_log_time = time.time()

finally:
    # Clean up
    cap.release()
    running = False  # Stop the temperature thread
    temp_thread.join()  # Wait for the temperature thread to finish
    cv2.destroyAllWindows()

    # Write log entries to a file
    log_file_path = "python_event_log.txt"  # Updated file name
    total_program_time = time.time() - last_log_time  # Calculate total execution time

    with open(log_file_path, "w") as log_file:
        # Write the total execution time
        log_file.write(f"Program Execution Time: {total_program_time:.4f} seconds\n")
        log_file.write("Performance Metrics:\n")
        
        # Process log entries and format them correctly
        for i, entry in enumerate(log_entries):
            if "Execution Time" in entry:
                # Extract execution time and mode
                parts = entry.split(" at ")
                execution_time_str = parts[0].split(": ")[1].replace(" seconds", "")  # Remove " seconds"
                execution_time = float(execution_time_str) * 1000  # Convert to milliseconds
                mode = parts[0].split("'")[1]

                # Find corresponding FPS and Temperature log entries
                fps_entry = next((e for e in log_entries[i:] if "FPS" in e), None)
                temp_entry = next((e for e in log_entries[i:] if "Temperature" in e), None)

                # Extract values for FPS and Temperature
                fps = fps_entry.split(": ")[1].split(" ")[0] if fps_entry else "N/A"
                temperature = temp_entry.split(": ")[1].split(" ")[0] if temp_entry else "N/A"

                # Write the formatted log entry
                log_file.write(
                    f"FPS: {fps} | Execution Time: {execution_time:.6f} ms | "
                    f"CPU Temp: temp={temperature} | Mode: {mode}\n"
                )

    print(f"Event log saved to '{log_file_path}'.")

    # Open the log file automatically
    if os.name == 'nt':  # For Windows
        os.startfile(log_file_path)
    elif os.name == 'posix':  # For macOS and Linux
        os.system(f'xdg-open {log_file_path}')


  # macOS
        # For Linux, you might want to use 'xdg-open' instead
        # os.system(f'xdg-open {log_file_path}')
