#include <array>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <frameworkd/classes/daemon/daemon.hpp>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sdbus-c++/Error.h>
#include <string>
#include <sys/stat.h>
#include <termio.h>
#include <unistd.h>

#include "uartd/function-handler.hpp"
#include "uartd/functions.hpp"

auto hex2string(const unsigned char i) -> std::string
{
    std::stringstream s;
    s << "0X" << std::uppercase << std::setw(3) << std::setfill('0') << std::hex << static_cast<int>(i);
    return s.str();
}

class UartdService : public RoutineService {
public:
    nlohmann::json m_actions;

    std::map<unsigned char, message_t (*)(const nlohmann::json&)> m_canMap;
    std::map<unsigned char, DBusHandler::Path> adresses;

    explicit UartdService(const std::string& id, const std::string& file, nlohmann::json actions)
        : RoutineService(id)
    {
        m_actions = actions;
        setupSerialFd(file);
    }

    void setup() override
    {
        std::map<std::string, message_t (*)(const nlohmann::json&)> functionMap = FunctionHandler::getFunctionMap();

        std::cout << "reading possible actions" << std::endl;

        for (const auto& [id, service] : m_actions.items()) {
            std::cout << std::string(service["who"]) << "." << std::string(service["method"]) << ": " << id << std::endl;

            auto idNum = static_cast<unsigned char>(std::stoul(id, nullptr, 16));
            std::string name = service["who"];
            m_canMap[idNum] = functionMap[service["method"]];
            adresses[idNum] = DBusHandler::Path {
                "zfkd.dbus." + name,
                "/zfkd/dbus/" + name,
                "zfkd.dbus." + name,
                service["method"]
            };
        }
    }

    void routine() override
    {
        unsigned char id;
        read(m_serialFd, &id, sizeof(char));

        std::cout << "received id: " << hex2string(id) << std::endl;

        nlohmann::json serviceJson = m_actions[hex2string(id)];

        if (adresses.count(id) == 0) {
            std::cout << "invalid id" << std::endl;
            return;
        }

        nlohmann::json recJson;
        try {
            nlohmann::json object;
            object["a"] = 0;
            recJson = DBusHandler::callMethod(adresses[id], object);
        } catch (sdbus::Error&) {
            std::cout << "invalid id" << std::endl;
            return;
        }

        if (recJson["data"].is_null()) {
            std::cout << "data is empty" << std::endl;
            return;
        }

        auto dataFrames = m_canMap[id](recJson);

        std::cout << "frames:" << std::endl;

        for (auto& frameData : dataFrames) {
            std::cout << "\t";
            for (auto& byte : frameData) {
                printf("%02hhx", byte);
                std::cout << ",";
            }
            std::cout << "\n";
        }

        std::cout << "\n";

        unsigned int startID = static_cast<unsigned int>(std::strtoul(static_cast<std::string>(serviceJson["answer"]["start_id"]).c_str(), nullptr, 16));
        unsigned int endID = static_cast<unsigned int>(std::strtoul(static_cast<std::string>(serviceJson["answer"]["end_id"]).c_str(), nullptr, 16));

        for (unsigned int i = startID; i <= endID && !dataFrames[i - startID].empty(); i++) {
            write(m_serialFd, dataFrames[i - startID].data(), dataFrames[i - startID].size());
        }
    }

    void destroy() override
    {
        close(m_serialFd);
    }

private:
    int m_serialFd;
    void setupSerialFd(const std::string& path)
    {

        if ((m_serialFd = open(path.c_str(), O_RDWR | O_NOCTTY)) < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        auto* termios_p = static_cast<struct termios*>(calloc(1, sizeof(struct termios)));

        cfsetispeed(termios_p, B115200);
        cfsetospeed(termios_p, B115200);
        cfmakeraw(termios_p);

        termios_p->c_cc[VMIN] = 1;
        termios_p->c_cc[VTIME] = 10;

        termios_p->c_cflag &= ~CSTOPB;
        termios_p->c_cflag &= ~CRTSCTS; /* no HW flow control? */
        termios_p->c_cflag |= CLOCAL | CREAD;

        if (tcsetattr(m_serialFd, TCSANOW, termios_p) != 0) {
            perror("tcsetattr");
            exit(EXIT_FAILURE);
        }
    }
};

auto main(int argc, char* argv[]) -> int
{
    Daemon daemon("uartd");
    std::string file;
    if (argc < 2) {
        file = daemon.getConfigHandler()["file"];
    } else {
        file = argv[1];
    }
    UartdService uartdService("uartd", file, daemon.getConfigHandler()["actions"]);

    daemon.deploy(uartdService);
    daemon.run();
}
