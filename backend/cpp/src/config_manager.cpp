#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace vfx {
namespace trading {

ConfigManager::ConfigManager(const std::string& config_file) 
    : config_file_path_(config_file) {
}

bool ConfigManager::load_config() {
    std::ifstream file(config_file_path_);
    if (!file.is_open()) {
        std::cout << "Warning: Could not open config file: " << config_file_path_ << std::endl;
        return false;
    }
    
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Handle sections [SectionName]
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Handle key=value pairs
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);
            
            trim(key);
            trim(value);
            
            // Prepend section name if we have one
            if (!current_section.empty()) {
                key = current_section + "." + key;
            }
            
            config_values_[key] = value;
        }
    }
    
    std::cout << "Loaded " << config_values_.size() << " configuration entries" << std::endl;
    return true;
}

bool ConfigManager::save_config() {
    std::ofstream file(config_file_path_);
    if (!file.is_open()) {
        std::cout << "Error: Could not save config file: " << config_file_path_ << std::endl;
        return false;
    }
    
    // Group by sections
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> sections;
    
    for (const auto& [key, value] : config_values_) {
        size_t dot_pos = key.find('.');
        if (dot_pos != std::string::npos) {
            std::string section = key.substr(0, dot_pos);
            std::string local_key = key.substr(dot_pos + 1);
            sections[section].emplace_back(local_key, value);
        } else {
            sections[""].emplace_back(key, value);
        }
    }
    
    // Write sections
    for (const auto& [section_name, entries] : sections) {
        if (!section_name.empty()) {
            file << "[" << section_name << "]\n";
        }
        
        for (const auto& [key, value] : entries) {
            file << key << "=" << value << "\n";
        }
        file << "\n";
    }
    
    return true;
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_value) {
    auto it = config_values_.find(key);
    return (it != config_values_.end()) ? it->second : default_value;
}

int ConfigManager::get_int(const std::string& key, int default_value) {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

double ConfigManager::get_double(const std::string& key, double default_value) {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

bool ConfigManager::get_bool(const std::string& key, bool default_value) {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }
    return default_value;
}

void ConfigManager::set_string(const std::string& key, const std::string& value) {
    config_values_[key] = value;
}

void ConfigManager::set_int(const std::string& key, int value) {
    config_values_[key] = std::to_string(value);
}

void ConfigManager::set_double(const std::string& key, double value) {
    config_values_[key] = std::to_string(value);
}

void ConfigManager::set_bool(const std::string& key, bool value) {
    config_values_[key] = value ? "true" : "false";
}

std::vector<std::string> ConfigManager::get_all_keys() const {
    std::vector<std::string> keys;
    for (const auto& [key, value] : config_values_) {
        keys.push_back(key);
    }
    return keys;
}

bool ConfigManager::has_key(const std::string& key) const {
    return config_values_.find(key) != config_values_.end();
}

void ConfigManager::remove_key(const std::string& key) {
    config_values_.erase(key);
}

void ConfigManager::print_config() const {
    std::cout << "Configuration entries:" << std::endl;
    for (const auto& [key, value] : config_values_) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
}

void ConfigManager::trim(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

} // namespace trading
} // namespace vfx