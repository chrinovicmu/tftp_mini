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
        ERROR - 5 
    }; 

    namespace Mode {

        constexpr const char* OCTET = "octet"; //binary mode 
        constexpr const char* NETASCII = "netascii"; 
    }
}

class TFTPClient{

private:
     int socket_fd; 
     struct sockaddr_in server_addr; 
     struct sockaddr_in from_addr;
     socklen_t addr_len; 
     std::string server_ip;
     uint16_t server_port;
     int expected_block; 
     int total_bytes; 
     char buffer[TFTP::MAX_PACKET_SIZE];


public:

     explicit TFTPClient(const std::string& server_ip, uint16_t server_port = TFTP::SERVER_PORT) 
    : socket_fd(-1), server_ip(server_ip), server_port(server_port), expected_block(1), total_bytes(0){
         std::memset(&server_addr, 0, sizeof(server_addr)); 
         std::memset(&from_addr, 0, sizeof(from_addr)); 
         std::memset(buffer, 0, TFTP::MAX_PACKET_SIZE);
         std::cout << "TFTP initilized for server: " << server_ip << " : " << server_port << std::endl; ;
    }

     ~TFTPClient(){
        if(socket_fd >=0){
             close(socket_fd); 
             std::cout << "socket closed successfully";
         }
    }

    void connect()
    {
        std::cout << "establishing connection\n";

        socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
        if(socket_fd < 0){
            throw std::runtime_error("failed to create udp socket: " + std::string(strerror(errno))); 
        }
        std::cout << "udp socket successfully created fd : " << socket_fd << "\n";
    
        server_addr.sin_family = AF_INET; 
        server_addr.sin_port = htons(server_port); 

        /*convsert ip address from string to binary format */ 
        int convert_result = inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr); 
        if(convert_result <= 0){
             close(socket_fd)
             socket_fd = -1; 
             throw std::runtime_error("invlaid serve ip address format: " + ":" << std::string(std::strerror(errno))); 
        }

         std::cout << "server address comfigured: " <, server_ip ":" << server_port << "\n";


         struct timeval tv; 
         tv.tv_sec = TFTP::TIMEOUT_SECONDS; 
         tv.tv_usec = 0; 
         if(setsockopt(socket_fd, SOL_SOCKET))


        











    }

}

int main (int argc, char *argv[])
{

    
    return 0;
}
