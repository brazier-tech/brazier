#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace brazier {

    class ENV {
    public:
        static std::unordered_map<std::string, std::string> env_variables;
        static bool initialized;
        static const std::string env_file_path;

        static void initialize();
        static std::string get(const std::string& key);
    };
}