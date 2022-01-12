#pragma once

void generate(std::filesystem::path startPath, std::vector<std::string> args);
int main(int argc, const char** argv)
{
    try
    {
        auto startPath = std::filesystem::current_path();
        std::filesystem::current_path(BUILD_DIR);
        startPath = std::filesystem::proximate(startPath);
        generate(startPath, std::vector<std::string>(argv, argv+argc));
    }
    catch(const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return -1;
    }
}
