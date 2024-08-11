#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <unordered_map>
#include <zlib.h>  // For CRC32 checksum

using boost::asio::ip::udp;

const int PORT = 12345;
const std::string FILENAME = "example.txt";
const size_t PACKET_SIZE = 1024;

// Function to calculate CRC32 checksum
uint32_t calculateChecksum(const char* data, std::size_t length) {
    return crc32(0L, reinterpret_cast<const unsigned char*>(data), length);
}

void sendFileToClients(udp::socket& socket, const std::unordered_map<udp::endpoint, bool>& clients) {
    std::ifstream file(FILENAME, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file " << FILENAME << std::endl;
        return;
    }

    boost::array<char, PACKET_SIZE> buffer;
    std::size_t bytesSent = 0;

    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        std::size_t length = file.gcount();

        // Calculate checksum
        uint32_t checksum = calculateChecksum(buffer.data(), length);

        for (auto& client : clients) {
            // Send packet
            socket.send_to(boost::asio::buffer(buffer.data(), length), client.first);

            // Send checksum
            socket.send_to(boost::asio::buffer(&checksum, sizeof(checksum)), client.first);

            // Wait for acknowledgment
            char ack[4];
            udp::endpoint senderEndpoint;
            socket.receive_from(boost::asio::buffer(ack), senderEndpoint);

            if (std::string(ack) == "ACK") {
                std::cout << "Packet acknowledged by " << senderEndpoint << std::endl;
            } else {
                std::cerr << "Failed to receive proper acknowledgment from " << senderEndpoint << std::endl;
            }
        }
    }

    std::cout << "File sent to all clients successfully!" << std::endl;
}

int main() {
    try {
        boost::asio::io_context ioContext;
        udp::socket socket(ioContext, udp::endpoint(udp::v4(), PORT));

        std::cout << "Server is running and waiting for clients..." << std::endl;

        std::unordered_map<udp::endpoint, bool> clients;
        udp::endpoint clientEndpoint;
        boost::array<char, 5> clientRequest;

        // Wait for clients to connect
        while (true) {
            socket.receive_from(boost::asio::buffer(clientRequest), clientEndpoint);
            if (std::string(clientRequest.data(), 5) == "START") {
                clients[clientEndpoint] = true;
                std::cout << "Client connected: " << clientEndpoint << std::endl;

                // Optionally, break after receiving a certain number of clients
                if (clients.size() >= 5) { 
                    break;
                }
            }
        }

        sendFileToClients(socket, clients);

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
