#pragma once
#include "main.hpp"

// the component parts of a pathname
class PathComponents {
public:
    std::string parent;
    std::string child;
    std::vector<std::string> names;

    PathComponents(const std::string& pathname); // separate a pathanme into its component parts
    ~PathComponents() = default;
};
