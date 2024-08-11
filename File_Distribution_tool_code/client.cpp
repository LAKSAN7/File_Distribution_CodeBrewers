#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <zlib.h>  // For CRC32 checksum

using boost::asio::ip::udp;

const int PORT = 12345;
const std::string SERVER_ADDRESS = "127.0.0.1";
const std::string OUTPUT_FILENAME = "received_example.txt";
const size_t PACKET_SIZE = 1024;

// Function to calculate CRC32 checksum
uint32_t calculateChecksum(const char* data, std::size_t length) {
    return crc32(0L, reinterpret_cast<const unsigned char*>(data), length);
}

void receiveFile(udp::socket& socket, udp::endpoint& serverEndpoint) {
    std::ofstream outputFile(OUTPUT_FILENAME, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Could not create output file " << OUTPUT_FILENAME << std::endl;
        return;
    }

    boost::array<char, PACKET_SIZE> buffer;
    std::size_t bytesReceived = 0;

    for (;;) {
        std::size_t length = socket.receive_from(boost::asio::buffer(buffer), serverEndpoint);

        if (length > 0) {
            // Receive checksum
            uint32_t receivedChecksum;
            socket.receive_from(boost::asio::buffer(&receivedChecksum, sizeof(receivedChecksum)), serverEndpoint);

            // Verify checksum
            uint32_t calculatedChecksum = calculateChecksum(buffer.data(), length);
            if (calculatedChecksum == receivedChecksum) {
                outputFile.write(buffer.data(), length);
                bytesReceived += length;

                // Send acknowledgment
                char ack[4] = {'A', 'C', 'K', '\0'};
                socket.send_to(boost::asio::buffer(ack), serverEndpoint);
            } else {
                std::cerr << "Checksum mismatch, packet discarded" << std::endl;
            }
        } else {
            break;
        }
    }

    std::cout << "File received and saved as " << OUTPUT_FILENAME << std::endl;
}

int main() {
    try {
        boost::asio::io_context ioContext;
        udp::socket socket(ioContext, udp::endpoint(udp::v4(), 0));

        udp::resolver resolver(ioContext);
        udp::endpoint serverEndpoint = *resolver.resolve(udp::v4(), SERVER_ADDRESS, std::to_string(PORT)).begin();

        // Send initial request to server to start file transfer
        char request[5] = {'S', 'T', 'A', 'R', 'T'};
        socket.send_to(boost::asio::buffer(request), serverEndpoint);

        std::cout << "Connected to the server" << std::endl;
        receiveFile(socket, serverEndpoint);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
