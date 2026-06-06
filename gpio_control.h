#ifndef GPIO_CONTROL_H__
#define GPIO_CONTROL_H__

#include <string>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

class GpioControl {
public:
    GpioControl() : led1_fd(-1), led2_fd(-1), led3_fd(-1) {}
    
    ~GpioControl() {
        if (led1_fd >= 0) close(led1_fd);
        if (led2_fd >= 0) close(led2_fd);
        if (led3_fd >= 0) close(led3_fd);
    }
    
    bool init() {
        if (!export_gpio(LED1_PIN) || !export_gpio(LED2_PIN) || !export_gpio(LED3_PIN)) {
            return false;
        }
        
        if (!set_direction(LED1_PIN, "out") || !set_direction(LED2_PIN, "out") || !set_direction(LED3_PIN, "out")) {
            return false;
        }
        
        std::string led1_path = "/sys/class/gpio/gpio" + std::to_string(LED1_PIN) + "/value";
        std::string led2_path = "/sys/class/gpio/gpio" + std::to_string(LED2_PIN) + "/value";
        std::string led3_path = "/sys/class/gpio/gpio" + std::to_string(LED3_PIN) + "/value";
        
        led1_fd = open(led1_path.c_str(), O_WRONLY);
        led2_fd = open(led2_path.c_str(), O_WRONLY);
        led3_fd = open(led3_path.c_str(), O_WRONLY);
        
        if (led1_fd < 0 || led2_fd < 0 || led3_fd < 0) {
            return false;
        }
        
        write_gpio(led1_fd, 0);
        write_gpio(led2_fd, 0);
        write_gpio(led3_fd, 0);
        
        return true;
    }
    
    void control_by_gesture(const std::string& gesture) {
        if (led1_fd < 0 || led2_fd < 0 || led3_fd < 0) {
            return;
        }
        
        if (gesture == "fist") {
            write_gpio(led1_fd, 1);
            write_gpio(led2_fd, 0);
            write_gpio(led3_fd, 0);
        } else if (gesture == "palm") {
            write_gpio(led1_fd, 0);
            write_gpio(led2_fd, 1);
            write_gpio(led3_fd, 0);
        } else if (gesture == "five") {
            write_gpio(led1_fd, 0);
            write_gpio(led2_fd, 0);
            write_gpio(led3_fd, 1);
        } else if (gesture == "victory") {
            write_gpio(led1_fd, 1);
            write_gpio(led2_fd, 1);
            write_gpio(led3_fd, 0);
        } else {
            write_gpio(led1_fd, 0);
            write_gpio(led2_fd, 0);
            write_gpio(led3_fd, 0);
        }
    }
    
private:
    static const int LED1_PIN = 10;
    static const int LED2_PIN = 11;
    static const int LED3_PIN = 12;
    
    int led1_fd;
    int led2_fd;
    int led3_fd;
    
    bool export_gpio(int pin) {
        std::ofstream export_file("/sys/class/gpio/export");
        if (!export_file.is_open()) {
            return false;
        }
        export_file << pin;
        return true;
    }
    
    bool set_direction(int pin, const std::string& direction) {
        std::string path = "/sys/class/gpio/gpio" + std::to_string(pin) + "/direction";
        std::ofstream dir_file(path);
        if (!dir_file.is_open()) {
            return false;
        }
        dir_file << direction;
        return true;
    }
    
    void write_gpio(int fd, int value) {
        if (fd >= 0) {
            lseek(fd, 0, SEEK_SET);
            write(fd, value ? "1" : "0", 1);
        }
    }
};

#endif