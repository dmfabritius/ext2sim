#include "PathComponents.hpp"

// separate a pathanme into its component parts
PathComponents::PathComponents(const std::string& pathname) {
    if (pathname == "") return;

    // split the given name into its directory and its base name, e.g., "/abc/def/ghi" -> "/abc/def", "ghi"
    char temp[STRING_SIZE]; // dirname(), basename() destroy original pathname
    strcpy(temp, pathname.c_str());
    parent = std::string(dirname(temp));
    strcpy(temp, pathname.c_str());
    child = std::string(basename(temp));
    TRACE(1, "pathname = '%s', dir/parent = '%s', base/child = '%s'\n",
        pathname.c_str(), parent.c_str(), child.c_str());

    // split the given name into its component parts, e.g., "/abc/def/ghi" -> ["abc", "def", "ghi"]
    std::ostringstream trace;
    trace << pathname << " tokenizes to: ";
    std::istringstream stream(pathname);
    std::string component;
    while (std::getline(stream, component, '/')) {
        if (component != "") names.push_back(component);
        trace << component << " ";
    }
    TRACE(1, "%s\n", trace.str().c_str());
}
