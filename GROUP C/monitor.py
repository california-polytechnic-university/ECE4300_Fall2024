import psutil
import time
import subprocess
import sys
import pynvml

def monitor_resources():
    # Track CPU frequency, usage, and RAM usage
    cpu_freq = psutil.cpu_freq()
    cpu_usage = psutil.cpu_percent(interval=1)
    memory_info = psutil.virtual_memory()

    print(f"\nCPU Frequency: {cpu_freq.current:.2f} MHz")
    print(f"CPU Usage: {cpu_usage}%")
    print(f"Total RAM: {memory_info.total / (1024 ** 3):.2f} GB")
    print(f"Available RAM: {memory_info.available / (1024 ** 3):.2f} GB")
    print(f"RAM Usage: {memory_info.percent}%\n")

    # Track GPU metrics using NVML
    pynvml.nvmlInit()
    handle = pynvml.nvmlDeviceGetHandleByIndex(0)  # Assuming one GPU
    
    # GPU Name
    gpu_name = pynvml.nvmlDeviceGetName(handle)
    print(f"GPU: {gpu_name}")

    # # GPU Clock Speeds
    graphics_clock = pynvml.nvmlDeviceGetClockInfo(handle, pynvml.NVML_CLOCK_GRAPHICS)
    memory_clock = pynvml.nvmlDeviceGetClockInfo(handle, pynvml.NVML_CLOCK_MEM)
    print(f"Graphics Clock: {graphics_clock} MHz")
    print(f"Memory Clock: {memory_clock} MHz")

    # # GPU Memory Usage
    memory_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
    print(f"Total GPU Memory: {memory_info.total / (1024**2):.2f} MB")
    print(f"Used GPU Memory: {memory_info.used / (1024**2):.2f} MB")
    print(f"Free GPU Memory: {memory_info.free / (1024**2):.2f} MB")

    # # GPU Utilization
    utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)
    print(f"GPU Utilization: {utilization.gpu}%")
    print(f"Memory Utilization: {utilization.memory}%")

    # # GPU Power Usage
    power_usage = pynvml.nvmlDeviceGetPowerUsage(handle)
    print(f"Power Usage: {power_usage / 1000:.2f} Watts\n")

    pynvml.nvmlShutdown()

def execute_script(script_name):
    # Use the current Python interpreter to run the script
    python_path = sys.executable
    print(f"Using Python path: {python_path}")
    
    print(f"Running {script_name}...")
    start_time = time.time()
    
    # Run the specified script
    process = subprocess.Popen([python_path, script_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    # Monitor the resource usage while the script runs
    while process.poll() is None:
        monitor_resources()
        time.sleep(1)  # Monitor every 1 second
    
    # Wait for the process to complete
    stdout, stderr = process.communicate()
    end_time = time.time()
    
    # Output process details
    execution_time = end_time - start_time
    print(f"Execution Time for {script_name}: {execution_time:.2f} seconds")
    print(f"Output:\n{stdout.strip()}")
    if stderr:
        print(f"Errors:\n{stderr.strip()}")
    
    return execution_time

if __name__ == "__main__":
    # List of scripts to execute
    # scripts = ["train_model.py", "facial_req.py"]
    scripts = ["facial_req.py"]

    for script in scripts:
        execution_time = execute_script(script)
        print(f"-" * 40)  # Separator between script outputs
        # if script == "train_model.py":
        #     print(f"[INFO] {script} completed in {execution_time:.2f} seconds. Proceeding to the next script...")