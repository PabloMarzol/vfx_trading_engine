#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace vfx {
namespace trading {

class ConfigManager {
private:
    std::unordered_map<std::string, std::string> config_values_;
    std::string config_file_path_;
    
    void trim(std::string& str);
    
public:
    ConfigManager(const std::string& config_file = "config/trading.ini");
    
    bool load_config();
    bool save_config();
    
    // Get configuration values
    std::string get_string(const std::string& key, const std::string& default_value = "");
    int get_int(const std::string& key, int default_value = 0);
    double get_double(const std::string& key, double default_value = 0.0);
    bool get_bool(const std::string& key, bool default_value = false);
    
    // Set configuration values
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_double(const std::string& key, double value);
    void set_bool(const std::string& key, bool value);
    
    // Utility
    std::vector<std::string> get_all_keys() const;
    bool has_key(const std::string& key) const;
    void remove_key(const std::string& key);
    void print_config() const;
};

} // namespace trading
} // namespace vfx