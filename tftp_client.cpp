#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h>  
#include <arpa/inet.h>   
#include <stdexcept>
#include <iomanip>
#include <cerrno>        /

namespace TFTP{
    
    constexpr std::uint16_t SERVER_PORT = 69;
    
    /*max packet size */ 
    constexpr std::size_t MAX_PACKET_SIZE = 516; 
    
    /*max packet data payload size */ 
    constexpr std::size_t MAX_DATA_SIZE = 512; 

    constexpr int TIMEOUT_SECONDS = 5; 
    
    /*TFTP operation opcode */ 
    enum class Opcode : std::uint16_t{
        RRQ = 1,    // read request 
        WRQ = 2,    // write request - FIXED: Added missing comma
        DATA = 3,   // data packets 
        ACK = 4,    // acknowledgment  
        ERROR = 5 
    }; 

    namespace Mode {
        constexpr const char* OCTET = "octet";      // binary mode 
        constexpr const char* NETASCII = "netascii"; 
    }
}

class TFTPClient{

private:
     int socket_fd; 
     struct sockaddr_in dest_addr; 
     struct sockaddr_in src_addr;
     socklen_t addr_len; 
     std::string server_ip;
     uint16_t server_port;
     int expected_block; 
     int total_bytes; 
     char buffer[TFTP::MAX_PACKET_SIZE];

public:

     explicit TFTPClient(const std::string& server_ip, uint16_t server_port = TFTP::SERVER_PORT) 
    : socket_fd(-1), server_ip(server_ip), server_port(server_port), expected_block(1), total_bytes(0){

         std::memset(&dest_addr, 0, sizeof(dest_addr)); 
         std::memset(&src_addr, 0, sizeof(src_addr)); 
         std::memset(buffer, 0, TFTP::MAX_PACKET_SIZE);
         addr_len = sizeof(src_addr);  // FIXED: Initialize addr_len
         
         std::cout << "TFTP initialized for server: " << server_ip << ":" << server_port << std::endl;
    }

     ~TFTPClient(){
        if(socket_fd >= 0){
             close(socket_fd); 
             std::cout << "Socket closed successfully" << std::endl;  // FIXED: Added std::endl
         }
    }

    void connect()
    {
        std::cout << "=== Establishing connection ===" << std::endl;

        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

        if(socket_fd < 0){
            throw std::runtime_error("Failed to create UDP socket: " + std::string(strerror(errno))); 
        }
        std::cout << "UDP socket successfully created, fd: " << socket_fd << std::endl;
    
        dest_addr.sin_family = AF_INET;                    // IPv4 address family
        dest_addr.sin_port = htons(server_port);          // Convert port to network byte order

        int convert_result = inet_pton(AF_INET, server_ip.c_str(), &dest_addr.sin_addr); 

        if(convert_result <= 0){
             close(socket_fd);  
             socket_fd = -1; 
             throw std::runtime_error("Invalid server IP address format: " + server_ip + " - " + std::string(strerror(errno))); 
        }

         std::cout << "Server address configured: " << server_ip << ":" << server_port << std::endl;

         struct timeval tv; 
         tv.tv_sec = TFTP::TIMEOUT_SECONDS;    // Timeout in seconds
         tv.tv_usec = 0;                       // Additional microseconds (0 for whole seconds)

         if(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
         {
             close(socket_fd); 
             socket_fd = -1;
             throw std::runtime_error("Failed to set socket timeout: " + std::string(strerror(errno)));
         }

         std::cout << "Socket timeout set to " << TFTP::TIMEOUT_SECONDS << " seconds" << std::endl; 
    }

     void send_rrq(const char *filename, const char *mode = TFTP::Mode::OCTET)
     {
        std::size_t file_len = std::strlen(filename); 
        std::size_t mode_len = std::strlen(mode); 
        
        // Calculate total RRQ packet size: opcode(2) + filename + null(1) + mode + null(1)
        std::size_t rrq_len = 2 + file_len + 1 + mode_len + 1;

        if(rrq_len > TFTP::MAX_PACKET_SIZE)
        {
            close(socket_fd); 
            socket_fd = -1;
            // FIXED: Corrected std::to_string syntax
            throw std::runtime_error("RRQ packet too large: " + std::to_string(rrq_len) + " bytes (max: "
                                     + std::to_string(TFTP::MAX_PACKET_SIZE) + ")"); 
        }

        // Build RRQ packet according to TFTP protocol specification
        buffer[0] = 0x00;                                          // High byte of opcode
        buffer[1] = static_cast<uint8_t>(TFTP::Opcode::RRQ);      // Low byte of RRQ opcode (1)

        // Copy filename followed by null terminator
        std::strcpy(buffer + 2, filename);
        // Copy mode string after filename+null, FIXED: corrected buffer offset calculation
        std::strcpy(buffer + 2 + file_len + 1, mode);

        // Display packet information for debugging
        std::cout << "RRQ packet:" << std::endl; 
        std::cout << "Filename: " << filename << std::endl;
        std::cout << "Mode: " << mode << std::endl;
        std::cout << "Packet size: " << rrq_len << " bytes" << std::endl;

        // Send RRQ packet to server using UDP
        ssize_t sent_bytes = sendto(socket_fd, buffer, rrq_len, 0, 
                                   (struct sockaddr*)&dest_addr, sizeof(dest_addr)); 
        
        if(sent_bytes < 0)
        {
            close(socket_fd);
            socket_fd = -1; 
            throw std::runtime_error("Failed to send RRQ: " + std::string(strerror(errno))); 
        }
        
        std::cout << "RRQ sent successfully (" << sent_bytes << " bytes)" << std::endl; 
    }

    // Main file reception loop - handles DATA packets and sends ACK responses
    void receive_file(void)  // FIXED: Corrected function name spelling
    {
        std::cout << "=== Receiving file data ===" << std::endl;
        
        while (true) 
        {
            // Receive packet from server (blocking call with timeout)
            ssize_t rec = recvfrom(socket_fd, buffer, TFTP::MAX_PACKET_SIZE, 0, 
                                  (struct sockaddr*)&src_addr, &addr_len);
            
            if(rec < 0)
            {
                close(socket_fd); 
                socket_fd = -1; 
                throw std::runtime_error("Failed to receive data: " + std::string(strerror(errno))); 
            }

            // Validate minimum packet size (opcode + block number = 4 bytes minimum)
            if(rec < 4)
            {
                std::cerr << "Received packet is too small (" << rec << " bytes)" << std::endl; 
                break; 
            }

            // Extract opcode from first 2 bytes (network byte order - big endian)
            uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
           
            std::cout << "\nReceived packet: " << rec << " bytes, opcode: " << opcode << std::endl;

            // Handle DATA packet (opcode 3)
            if(opcode == static_cast<uint16_t>(TFTP::Opcode::DATA))
            {
                // Extract block number from bytes 2-3 (network byte order)
                uint16_t block_num = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]); 

                // Calculate actual data length (total packet - 4 byte header)
                int data_len = rec - 4;

                std::cout << "DATA block #" << block_num << ": " << data_len << " bytes of data" << std::endl;

                // Check for out-of-sequence blocks (basic error detection)
                if(block_num != expected_block)
                {
                    std::cerr << "Warning: expected block " << expected_block 
                             << ", received block " << block_num << std::endl;
                }

                // Display data content preview (first 100 bytes for debugging)
                std::cout << "Data content (first " << (data_len > 100 ? 100 : data_len) << " bytes):" << std::endl;
                std::cout << "--- START DATA ---" << std::endl;

                // Write data directly to stdout for demonstration
                write(STDOUT_FILENO, buffer + 4, data_len > 100 ? 100 : data_len);

                if(data_len > 100)
                {
                    std::cout << "\n... (" << (data_len - 100) << " more bytes)" << std::endl;
                }
                std::cout << "\n--- END DATA ---" << std::endl;
            
                // Update statistics
                total_bytes += data_len;
                expected_block++;  // FIXED: Added missing increment for expected block

                // Construct ACK packet: opcode(4) + block_number(2 bytes)
                char ack_packet[4]; 
                ack_packet[0] = 0x00;                                    // High byte of ACK opcode
                ack_packet[1] = static_cast<uint8_t>(TFTP::Opcode::ACK); // Low byte of ACK opcode (4)
                ack_packet[2] = buffer[2];                               // Copy block number high byte
                ack_packet[3] = buffer[3];                               // Copy block number low byte

                // Send ACK back to server (using src_addr as server may use different port)
                ssize_t ack_sent = sendto(socket_fd, ack_packet, 4, 0, 
                                         (struct sockaddr*)&src_addr, addr_len);

                // FIXED: Check correct variable name for ACK send result
                if(ack_sent < 0)
                {
                    throw std::runtime_error("Failed to send ACK: " + std::string(strerror(errno)));
                }

                std::cout << "ACK sent for block #" << block_num << std::endl;

                // Check for end of transfer (data packet smaller than maximum indicates last packet)
                if(data_len < TFTP::MAX_DATA_SIZE)
                {
                    std::cout << "File transfer complete! (Last block was " << data_len << " bytes)" << std::endl;
                    break; 
                }
            }
            // Handle ERROR packet (opcode 5)
            else if (opcode == static_cast<uint16_t>(TFTP::Opcode::ERROR))
            {
                if (rec >= 4)
                {
                    // Extract error code from bytes 2-3
                    // FIXED: Added missing parentheses in array access
                    uint16_t error_code = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]); 
                    // Extract error message string (if present)
                    // FIXED: Corrected variable name from 'n' to 'rec'
                    std::string error_msg = (rec > 4) ? std::string(buffer + 4) : "No error message"; 
                    std::cerr << "TFTP Error " << error_code << ": " << error_msg << std::endl;  // FIXED: typo
                }
                break; 
            }
            else{
                std::cerr << "Unexpected opcode: " << opcode << std::endl; 
                break; 
            }
        }
    }

    // Display transfer statistics summary
    void print_summary()  // FIXED: Corrected function name spelling
    {
        std::cout << "\n=== Transfer Summary ===" << std::endl;
        std::cout << "Total bytes received: " << total_bytes << std::endl;
    }
}; 

int main (int argc, char *argv[])
{
    try{
        // Create TFTP client instance targeting localhost
        // FIXED: Corrected IP address format (was "127.0.1", should be "127.0.0.1")
        TFTPClient client("127.0.0.1"); 
        std::cout << "=== TFTP Client Starting ===" << std::endl; 
        
        // Establish connection and configure socket
        client.connect();
        
        // Request file "test.txt" from server using binary mode
        client.send_rrq("test.txt"); 
        
        // Enter main reception loop to receive file data
        client.receive_file();  // FIXED: Corrected function name
        
        // Display transfer completion summary
        client.print_summary(); // FIXED: Corrected function name
        
    }catch (const std::runtime_error& e){
        std::cerr << "Error: " << e.what() << std::endl; 
        return 1; 
    }
    
    return 0;
}
