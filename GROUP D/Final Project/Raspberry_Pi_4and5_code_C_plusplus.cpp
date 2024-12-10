#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <fstream>
#include <string>
#include <ctime>
#include <array>
#include <memory>
#include <cstdlib>  // For system() function

std::atomic<bool> running(true);
std::string cpu_temp = "Temp: N/A";
std::vector<std::string> log_entries;
std::time_t last_log_time = std::time(nullptr);
std::string pi_ip = "0.0.0.0";  // Replace with your Raspberry Pi's IP address

// Function to get Raspberry Pi CPU temperature using SSH
std::string get_cpu_temperature() {
    std::string command = "ssh pi_username@" + pi_ip + " 'vcgencmd measure_temp'";
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return "Error: popen() failed";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void temperature_thread() {
    while (running) {
        cpu_temp = get_cpu_temperature();
        std::this_thread::sleep_for(std::chrono::seconds(3));  // Fetch temperature every second
    }
}

int main() {
    cv::VideoCapture cap("http://" + pi_ip + ":5000/video_feed");

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video stream." << std::endl;
        return -1;
    }

    // Set the video resolution
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    // Start the temperature fetching thread
    std::thread temp_thread(temperature_thread);

    std::string processing_mode = "none";
    cv::namedWindow("Camera Stream", cv::WINDOW_NORMAL);
    cv::resizeWindow("Camera Stream", 800, 600);

    // Variables for FPS and execution time calculation
    auto prev_frame_time = std::chrono::steady_clock::now();
    auto start_program_time = std::chrono::steady_clock::now();

    while (running) {
        cv::Mat frame;
        auto frame_start_time = std::chrono::steady_clock::now();

        cap >> frame;

        if (frame.empty()) {
            std::cerr << "Failed to grab frame" << std::endl;
            break;
        }

        // Process the frame based on the selected mode
        if (processing_mode == "grayscale") {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        } else if (processing_mode == "remove_red") {
            frame.forEach<cv::Vec3b>([](cv::Vec3b &pixel, const int * position) {
                pixel[2] = 0;  // Set red channel to 0
            });
        } else if (processing_mode == "remove_green") {
            frame.forEach<cv::Vec3b>([](cv::Vec3b &pixel, const int * position) {
                pixel[1] = 0;  // Set green channel to 0
            });
        } else if (processing_mode == "remove_blue") {
            frame.forEach<cv::Vec3b>([](cv::Vec3b &pixel, const int * position) {
                pixel[0] = 0;  // Set blue channel to 0
            });
        } else if (processing_mode == "negative") {
            cv::bitwise_not(frame, frame);
        }

        // Calculate FPS
        auto new_frame_time = std::chrono::steady_clock::now();
        double fps = 1.0 / std::chrono::duration<double>(new_frame_time - prev_frame_time).count();
        prev_frame_time = new_frame_time;

        // Calculate execution time for processing the frame
        auto execution_time = std::chrono::duration<double, std::milli>(new_frame_time - frame_start_time).count();

        // Prepare overlay text
        std::string temp_text = "Temp: " + cpu_temp;
        std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
        cv::putText(frame, temp_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, fps_text, cv::Point(10, 70), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
        cv::putText(frame, "Mode: " + processing_mode, cv::Point(10, 110), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);

        // Add log entry for performance metrics
        std::time_t current_time = std::time(nullptr);
        if (current_time - last_log_time >= 3) {  // Log every 1 second
            log_entries.push_back("FPS: " + std::to_string(static_cast<int>(fps)) +
                                  " | Execution Time: " + std::to_string(execution_time) + " ms" +
                                  " | CPU Temp: " + cpu_temp +
                                  " | Mode: " + processing_mode);
            last_log_time = current_time;
        }

        cv::imshow("Camera Stream", frame);

        char key = (char)cv::waitKey(1);
        if (key == 'y') {
            processing_mode = "grayscale";
        } else if (key == 'r') {
            processing_mode = "remove_red";
        } else if (key == 'g') {
            processing_mode = "remove_green";
        } else if (key == 'b') {
            processing_mode = "remove_blue";
        } else if (key == 'n') {
            processing_mode = "negative";
        } else if (key == ' ') {
            processing_mode = "none";  // Reset to normal
        } else if (key == 'q') {
            running = false;  // Exit loop
        }
    }

    cap.release();
    running = false;  // Stop the temperature thread
    temp_thread.join();  // Wait for the temperature thread to finish
    cv::destroyAllWindows();

    // Calculate total program execution time
    auto end_program_time = std::chrono::steady_clock::now();
    double total_execution_time = std::chrono::duration<double>(end_program_time - start_program_time).count();

    // Write log entries to a file
    std::ofstream log_file("c_plus_plus_event_log.txt");
    log_file << "Program Execution Time: " << total_execution_time << " seconds\n";
    log_file << "Performance Metrics:\n";
    for (const auto &entry : log_entries) {
        log_file << entry << std::endl;
    }
    log_file.close();

    std::cout << "Event log saved to 'c_plus_plus_event_log.txt'." << std::endl;

    // Open the log file using the default application (e.g., text editor)
    std::system("xdg-open c_plus_plus_event_log.txt");

    return 0;
}
