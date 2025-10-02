
#include "parser.hpp"
#include <sstream>
#include <algorithm>

std::optional<std::string> ParsedCommand::get_arg(size_t index) const {
    if (index < args.size()) {
        return args[index];
    }
    return std::nullopt;
}

std::optional<int> ParsedCommand::get_int_arg(size_t index) const {
    auto arg = get_arg(index);
    if (!arg) {
        return std::nullopt;
    }
    
    try {
        return std::stoi(*arg);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<ParsedCommand> CommandParser::parse(const std::string& text) {
    if (!is_command(text)) {
        return std::nullopt;
    }
    
    auto args = split_args(text);
    if (args.empty()) {
        return std::nullopt;
    }
    
    ParsedCommand cmd;
    cmd.command = args[0].substr(1); // Remove leading '/'
    cmd.args.assign(args.begin() + 1, args.end());
    
    return cmd;
}

bool CommandParser::is_command(const std::string& text) {
    return !text.empty() && text[0] == '/';
}

std::vector<std::string> CommandParser::split_args(const std::string& text) {
    std::istringstream iss(text);
    std::vector<std::string> args;
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}
