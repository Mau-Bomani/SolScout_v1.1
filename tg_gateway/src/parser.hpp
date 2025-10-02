
#pragma once
#include <string>
#include <vector>
#include <optional>

struct ParsedCommand {
    std::string command;
    std::vector<std::string> args;
    
    std::optional<int> get_int_arg(size_t index) const;
    std::optional<double> get_double_arg(size_t index) const;
};

class CommandParser {
public:
    static std::optional<ParsedCommand> parse(const std::string& text);
    
private:
    static std::vector<std::string> split_args(const std::string& args_str);
};
