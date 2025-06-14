#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h> 
#include <sys/socket.h>
#include <stdexcept>
#include <iomanip> 

namespace TFTP{
    
    constexpr std::uint16_t SERVER_PORT = 69;
    
    /*max packet size */ 
    constexpr std::size_t MAX_PACKET_SIZE = 516; 
    
    /*max packet data pyaload size */ 
    constexpr std::size_t MAX_DATA_SIZE = 512; 

    constexpr int TIMEOUT_SECONDS = 5; 
    
    /*TFTP operation opcode */ 

    enum class Opcode : std::uint16_t{
        RRQ = 1, //read request 
        WRQ = 2; //write request
        DATA = 3, //data packets 
        ACK = 4, // acknowledgment  
        ERROR = 5 
    }; 

    namespace Mode {

        constexpr const char* OCTET = "octet"; //binary mode 
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
         std::cout << "TFTP initilized for server: " << server_ip << " : " << server_port << std::endl; ;
    }

     /*destructor */ 

     ~TFTPClient(){
        if(socket_fd >=0){
             close(socket_fd); 
             std::cout << "socket closed successfully";
         }
    }

    void connect()
    {
        std::cout << "=== establishing connection === \n";

        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

        if(socket_fd < 0){
            throw std::runtime_error("failed to create udp socket: " + std::string(strerror(errno))); 
        }
        std::cout << "udp socket successfully created fd : " << socket_fd << "\n";
    
        dest_addr.sin_family = AF_INET; 
        dest_addr.sin_port = htons(server_port); 

        /*convsert ip address from string to binary format */ 
        int convert_result = inet_pton(AF_INET, server_ip.c_str(), &dest_addr.sin_addr); 

        if(convert_result <= 0){
             close(socket_fd)
             socket_fd = -1; 
             throw std::runtime_error("invlaid serve ip address format: " + ":" << std::string(std::strerror(errno))); 
        }

         std::cout << "server address comfigured: " <, server_ip ":" << server_port << "\n";


         struct timeval tv; 
         tv.tv_sec = TFTP::TIMEOUT_SECONDS; 
         tv.tv_usec = 0; 

         /*set time outs for socket */ 
         if(setsockopt(socket_fd, SOL_SOCKET, &tv, sizeof(tv)) < 0)
         {
             close(socket_fd); 
             socket_fd = -1;
             throw std::runtime_error("failed to set socket timout: " + std::string(std::strerror(errno)));
         }

         std::cout << "socket timeouts set to " << TFTP::TIMEOUT_SECONDS << "seconds \n"; 
    }

     void send_rrq(const char *filename, const char *mode = TFTP::Mode::OCTET)
     {
        std::size_t file_len = std::strlen(filename); 
        std::size_t mode_len = std::strlen(mode); 
        
        /*opcode + filename + null + mode */ 
        std::size_t rrq_len = 2 + file_len + 1 + mode_len + 1;

        if(rrq_len > TFTP::MAX_PACKET_SIZE)
        {
            close(socket_fd); 
            socket_fd = -1;
            throw std::runtime_error("rrq packet too large:" + std:to_string(rrq_len) + " bytes (max:  "
                                     + std::to_string(TFTP::MAX_PACKET_SIZE) + " )\n"); 

        }

        buffer[0] = 0x00; 
        buffer[1] = static_cast<uint8_t>(TFTP::Opcode::RRQ); 

        std::strcpy(buffer + 2, filename);
        std::strcpy(buffer + 2 + filename + 1, mode);

        std::cout << "RRQ packet: \n"; 
        std::cout << "Filname : " << filename << "\n";
        std::cout << "mode: " << mode << "\n";
        std::cout << "packet size: " << rrq_len << "bytes\n";

        ssize_t sent_bytes = sendto(socket_fd, buffer, rrq_len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)); 
        
        if(sent_bytes < 0)
        {
            close(socket_fd);
            socket_fd = -1; 
            throw std::runtime_error("filed to send RRQ: " + std::string(std::strerror(errno))); 
        }
        
        std::cout << "rrq sent successfully (" << sent_bytes << " bytes\n)"; 
    }

    
    void recieve_file(void)
    {
        std::cout << "=== receiving file data === \n";
        
        while (true) 
        {
            ssize_t rec = recvfrom(socket_fd, buffer, TFTP::MAX_PACKET_SIZE, 0, (struct sockaddr*)&src_addr);
            
            if(rec < 0)
            {
                close(socket_fd); 
                socket_fd = -1; 
                throw std::runtime_error("failed to recieve data: " + std::string(std::strerror(errno))); 
            }

            if(rec < 4)
            {
                std::cerr << "recieved packet is too small (" << rec << "bytes\n"; 
                break; 
            }

            uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) |  static_cast<uint8_t>(buffer[1]);
           
            std::cout << "\nReceived packet: " << rec << "bytes, opcode: " << opcode << "\n";

            if(opcode == static_cast<uint16_t>(TFTP::Opcode::DATA))
            {
                uint16_t block_num = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]); 

                int data_len = rec - 4;

                std::cout << "DATA block #" << block_num << " : " << data_len << " bytes of data";

                if(block_num != expected_block)
                {
                    std::cerr << "Warning, expected block" << expected_block << ", recieved block " << block_num << "\n";
                }

                std::cout << "Data content (first " << (data_len > 100 ? 100 : data_len) << " bytes):\n";
                std::cout << "--- START DATA ---\n";

                write(STDOUT_FILENO, buffer + 4, data_len > 100 ? 100 :data_len);

                if(data_len > 100)
                {
                    std::cout << "\n... (" << (data_len - 100) << " more bytes)";
                }
                std::cout << "\n--- END DATA ---\n";
            
                total_bytes += data_len;

                char ack_packet[4]; 
                ack_packet[0] = 0x00; 
                ack_packet[1] = static_cast<uint8_t>(TFTP::Opcode::ACK);
                ack_packet[2] = buffer[2]; 
                ack_packet[3] = buffer[3];

                ssize_t ack_send = sendto(socket_fd, ack_packet, 4, 0, (struct sockaddr*)&src_addr, addr_len);

                if(ack_packet < 0)
                {
                    throw std::runtime_error("Failed to send ack: " + std::string(std::strerror(errno)));
                }

                std::cout << "Ack sent block #" << block_num << "\n";
            }
            
        }

    }





}

int main (int argc, char *argv[])
{

    
    return 0;
}
