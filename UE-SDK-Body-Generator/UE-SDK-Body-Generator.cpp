#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace fs = std::filesystem;

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string generateFunctionBody(const std::string& returnType, const std::string& className,
    const std::string& funcName, const std::string& params)
{
    std::ostringstream body;
    body.str().reserve(256);

    std::string structName = className + "_" + funcName + "_Params";
    body << structName << " params;\n";

    std::string trimmedParams = trim(params);
    if (!trimmedParams.empty()) {
        std::stringstream paramStream(trimmedParams);
        std::string param;
        while (std::getline(paramStream, param, ',')) {
            std::string t = trim(param);
            if (t.empty()) continue;
            size_t lastSpace = t.find_last_of(' ');
            if (lastSpace != std::string::npos) {
                std::string pName = t.substr(lastSpace + 1);
                body << "    params." << pName << " = " << pName << ";\n";
            }
        }
    }

    body << "    ProcessEvent(FindFunctionChecked(FName(TEXT(\"" << funcName << "\"))), &params);\n";
    if (trim(returnType) != "void") {
        body << "    return params.ReturnValue;\n";
    }

    return body.str();
}

void processFile(const fs::path& filePath, const fs::path& rootDir, const fs::path& outputRoot) {
    std::ifstream in(filePath);
    if (!in) {
        std::cerr << "failed to open: " << filePath << "\n";
        return;
    }

    std::ostringstream output;
    std::vector<std::string> includes;
    std::string line, currentClass, classBuffer;
    bool inClass = false;
    bool found = false;

    {
        std::ifstream scan(filePath);
        std::string scanLine;
        while (std::getline(scan, scanLine)) {
            std::string trimmed = trim(scanLine);
            if (trimmed.rfind("#include", 0) == 0) {
                includes.push_back(scanLine);
            }
        }
    }

    while (std::getline(in, line)) {
        std::string t = trim(line);

        if (!inClass && t.rfind("class ", 0) == 0) {
            size_t nameStart = 6;
            size_t nameEnd = t.find_first_of(" {:\t", nameStart);
            currentClass = t.substr(nameStart, nameEnd - nameStart);
            inClass = true;
            classBuffer.clear();
        }

        if (inClass) {
            classBuffer += line + "\n";

            if (t == "};" || t.find("};") != std::string::npos) {
                size_t pos = 0;
                while ((pos = classBuffer.find('(', pos)) != std::string::npos) {
                    size_t nameEnd = pos;
                    size_t nameStart = classBuffer.rfind(' ', nameEnd - 1);
                    if (nameStart == std::string::npos) { pos++; continue; }

                    std::string funcName = trim(classBuffer.substr(nameStart + 1, nameEnd - nameStart - 1));

                    if (funcName == currentClass || funcName == ("~" + currentClass)) {
                        pos++;
                        continue;
                    }

                    size_t retStart = classBuffer.rfind('\n', nameStart);
                    if (retStart == std::string::npos) retStart = 0;
                    else retStart++;
                    std::string returnType = trim(classBuffer.substr(retStart, nameStart - retStart));

                    size_t paramEnd = classBuffer.find(')', pos);
                    if (paramEnd == std::string::npos) break;
                    std::string params = trim(classBuffer.substr(pos + 1, paramEnd - pos - 1));

                    size_t afterParams = classBuffer.find_first_not_of(" \t", paramEnd + 1);
                    if (afterParams != std::string::npos && classBuffer[afterParams] == ';') {
                        found = true;
                        {
                            static std::mutex logMutex;
                            std::lock_guard<std::mutex> lock(logMutex);
                            std::cout << "generated function " << funcName << " in " << currentClass << "\n";
                        }
                        output << "// auto-generated function for " << funcName << "(by UE-SDK-Body-Generator)" << "\n";
                        output << returnType << " " << currentClass << "::" << funcName << "(" << params << ")\n";
                        output << "{\n";
                        output << "    " << generateFunctionBody(returnType, currentClass, funcName, params);
                        output << "}\n\n";
                    }
                    pos = paramEnd + 1;
                }
                inClass = false;
            }
        }
    }

    if (!found) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        std::cout << "mo matching functions in " << filePath.filename() << "\n";
        return;
    }

    fs::path relativePath = fs::relative(filePath, rootDir);
    fs::path outputPath = outputRoot / relativePath.parent_path();
    fs::create_directories(outputPath);

    fs::path outFile = outputPath / (filePath.stem().string() + "_generated.hpp");
    std::ofstream out(outFile);
    if (!out) {
        std::cerr << "failed to write file: " << outFile << "\n";
        return;
    }

    for (const auto& inc : includes) {
        out << inc << "\n";
    }
    if (!includes.empty()) {
        out << "\n";
    }

    out << output.str();

    {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        std::cout << "generated " << outFile << "\n";
    }
}

int main() {
    std::string folderPath;
    std::cout << "enter path to folder containing header files\n";
    std::getline(std::cin, folderPath);

    fs::path dirPath(folderPath);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "invalid folder path\n";
        return 1;
    }

    fs::path outputRoot = dirPath.parent_path() / "generated";
    fs::create_directories(outputRoot);

    std::vector<fs::path> files;
    for (auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".h" || ext == ".hpp" || ext == ".cpp") {
                files.push_back(entry.path());
            }
        }
    }

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4;

    std::vector<std::thread> threads;
    std::atomic<size_t> index(0);

    auto worker = [&]() {
        while (true) {
            size_t i = index.fetch_add(1);
            if (i >= files.size()) break;
            processFile(files[i], dirPath, outputRoot);
        }
        };

    for (unsigned int i = 0; i < threadCount; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    std::cout << "\nprocessing complete. output in " << outputRoot << "\n";
    return 0;
}
