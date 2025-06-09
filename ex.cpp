#include <iostream>          // For std::cout, std::cerr, std::endl
#include <string>            // For std::string class
#include <vector>            // For std::vector container
#include <cstring>           // For memset(), strlen()
#include <cstdlib>           // For exit()
#include <unistd.h>          // For close(), write()
#include <arpa/inet.h>       // For socket functions, inet_pton()
#include <sys/socket.h>      // For socket(), sendto(), recvfrom()
#include <stdexcept>         // For std::runtime_error
#include <iomanip>           // For std::hex, std::setfill, std::setw

/**
 * TFTP Protocol Constants and Definitions
 * 
 * TFTP (Trivial File Transfer Protocol) is defined in RFC 1350
 * It's a simple protocol for transferring files over UDP
 */
namespace TFTP {
    // Standard TFTP server port as defined in RFC 1350
    constexpr uint16_t SERVER_PORT = 69;
    
    // Maximum TFTP packet size: 2 bytes opcode + 2 bytes block number + 512 bytes data
    constexpr size_t MAX_PACKET_SIZE = 516;
    
    // Maximum data payload per packet (TFTP specification limit)
    constexpr size_t MAX_DATA_SIZE = 512;
    
    // Timeout in seconds for network operations
    constexpr int TIMEOUT_SECONDS = 5;
    
    /**
     * TFTP Operation Codes (Opcodes) as defined in RFC 1350
     * These are 16-bit values sent in network byte order (big-endian)
     */
    enum class Opcode : uint16_t {
        RRQ   = 1,  // Read Request - client requests to read a file from server
        WRQ   = 2,  // Write Request - client requests to write a file to server
        DATA  = 3,  // Data packet - contains file data with block number
        ACK   = 4,  // Acknowledgment - confirms receipt of data block
        ERROR = 5   // Error packet - reports errors with error code and message
    };
    
    /**
     * TFTP Transfer Modes
     * These determine how data is interpreted during transfer
     */
    namespace Mode {
        constexpr const char* OCTET = "octet";        // Binary mode (8-bit bytes)
        constexpr const char* NETASCII = "netascii";  // Text mode with CRLF conversion
    }
}

/**
 * TFTPClient Class
 * 
 * Encapsulates TFTP client functionality using modern C++ practices
 * This class handles the low-level socket operations and TFTP protocol details
 */
class TFTPClient {
private:
    int socket_fd_;                    // UDP socket file descriptor
    struct sockaddr_in server_addr_;   // Server address structure
    std::string server_ip_;            // Server IP address as string
    uint16_t server_port_;             // Server port number
    
public:
    /**
     * Constructor: Initialize TFTP client with server details
     * 
     * @param server_ip   Server IP address (e.g., "127.0.0.1")
     * @param server_port Server port (default: 69 for TFTP)
     * 
     * The constructor sets up the client but doesn't create the socket yet.
     * This allows for error handling in the connect() method.
     */
    explicit TFTPClient(const std::string& server_ip, uint16_t server_port = TFTP::SERVER_PORT)
        : socket_fd_(-1), server_ip_(server_ip), server_port_(server_port) {
        
        std::cout << "TFTPClient initialized for server: " << server_ip_ 
                  << ":" << server_port_ << std::endl;
    }
    
    /**
     * Destructor: Cleanup resources
     * 
     * RAII (Resource Acquisition Is Initialization) principle:
     * Automatically cleanup when object goes out of scope
     */
    ~TFTPClient() {
        disconnect();
    }
    
    /**
     * Establish connection to TFTP server
     * 
     * Creates UDP socket and configures server address structure.
     * Note: UDP is connectionless, so "connect" here means preparing
     * the socket and address structures for communication.
     * 
     * @throws std::runtime_error if socket creation or address setup fails
     */
    void connect() {
        std::cout << "\n=== Establishing Connection ===\n";
        
        // Step 1: Create UDP socket
        // AF_INET = IPv4 address family
        // SOCK_DGRAM = UDP socket type (datagram, connectionless)
        // 0 = Let system choose appropriate protocol (UDP for SOCK_DGRAM)
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create UDP socket: " + std::string(strerror(errno)));
        }
        
        std::cout << "UDP socket created successfully (fd: " << socket_fd_ << ")\n";
        
        // Step 2: Configure server address structure
        // This structure tells the system where to send packets
        std::memset(&server_addr_, 0, sizeof(server_addr_));  // Zero out structure
        server_addr_.sin_family = AF_INET;                    // IPv4 address family
        server_addr_.sin_port = htons(server_port_);          // Convert port to network byte order
        
        // Step 3: Convert IP address from string to binary format
        // inet_pton() converts presentation format to network format
        // Returns: 1 on success, 0 if invalid format, -1 on error
        int result = inet_pton(AF_INET, server_ip_.c_str(), &server_addr_.sin_addr);
        if (result <= 0) {
    int sockfd;                 // Socket file descriptor for UDP communication
    struct sockaddr_in server_addr, from_addr;  // Server and sender address structures
    socklen_t addr_len;         // Address length for recvfrom()
    char buffer[MAX_BUFFER];    // Buffer to hold TFTP packets
    int n;                      // Number of bytes received
    int expected_block = 1;     // Expected block number (TFTP blocks start at 1)
    int total_bytes = 0;        // Total bytes received for statistics

    printf("=== TFTP Client Starting ===\n");

    /* Step 1: Create UDP Socket
     * AF_INET = IPv4 address family
     * SOCK_DGRAM = UDP socket type (connectionless)
     * 0 = Let system choose appropriate protocol (UDP for SOCK_DGRAM)
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP socket created successfully (fd: %d)\n", sockfd);

    /* Step 2: Configure Server Address Structure
     * This structure tells the system where to send our TFTP request
     */
    memset(&server_addr, 0, sizeof(server_addr));  // Zero out the structure
    server_addr.sin_family = AF_INET;              // IPv4 address family
    server_addr.sin_port = htons(SERVER_PORT);     // Convert port to network byte order
    
    /* Convert IP address from string to binary format
     * inet_pton() is preferred over deprecated inet_addr()
     * AF_INET specifies IPv4, "127.0.0.1" is localhost
     */
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Server address configured: 127.0.0.1:%d\n", SERVER_PORT);

    /* Step 3: Construct TFTP Read Request (RRQ) Packet
     * RRQ Format: [2 bytes opcode][filename]\0[mode]\0
     * 
     * The packet structure is:
     * - Bytes 0-1: Opcode (0x0001 for RRQ in network byte order)
     * - Bytes 2-n: Null-terminated filename string
     * - Bytes n+1-m: Null-terminated mode string
     */
    char *filename = "test.txt";    // File to request from server
    char *mode = "octet";           // Transfer mode: "octet" = binary, "netascii" = text
    
    /* Build the RRQ packet byte by byte */
    buffer[0] = 0x00;               // High byte of opcode
    buffer[1] = 0x01;               // Low byte of opcode (RRQ = 1)
    
    /* Copy filename and mode with null terminators
     * strcpy() automatically adds null terminator
     */
    strcpy(buffer + 2, filename);
    strcpy(buffer + 2 + strlen(filename) + 1, mode);
    
    /* Calculate total RRQ packet length:
     * 2 bytes (opcode) + filename length + 1 (null) + mode length + 1 (null)
     */
    int rrq_len = 2 + strlen(filename) + 1 + strlen(mode) + 1;
    
    printf("Constructed RRQ packet:\n");
    printf("  Filename: %s\n", filename);
    printf("  Mode: %s\n", mode);
    printf("  Packet size: %d bytes\n", rrq_len);

    /* Step 4: Send RRQ to TFTP Server
     * sendto() is used for UDP (connectionless) communication
     * Parameters: socket, buffer, length, flags, destination address, address size
     */
    ssize_t sent_bytes = sendto(sockfd, buffer, rrq_len, 0, 
                               (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent_bytes < 0) {
        perror("Failed to send RRQ");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("RRQ sent successfully (%zd bytes)\n", sent_bytes);

    /* Step 5: Receive and Process DATA Packets
     * TFTP DATA packet format: [2 bytes opcode][2 bytes block#][0-512 bytes data]
     * 
     * In a complete implementation, we would:
     * 1. Receive DATA packet
     * 2. Send ACK for received block
     * 3. Repeat until DATA packet < 512 bytes (indicates end of file)
     */
    printf("\n=== Receiving File Data ===\n");
    
    while (1) {
        /* Receive packet from server
         * recvfrom() fills in the sender's address in from_addr
         */
        addr_len = sizeof(from_addr);
        n = recvfrom(sockfd, buffer, MAX_BUFFER, 0, 
                    (struct sockaddr *)&from_addr, &addr_len);
        
        if (n < 0) {
            perror("Failed to receive data");
            break;
        }
        
        /* Verify minimum packet size (at least opcode) */
        if (n < 2) {
            printf("Received packet too small (%d bytes)\n", n);
            break;
        }
        
        /* Extract opcode from first 2 bytes (network byte order) */
        uint16_t opcode = (buffer[0] << 8) | buffer[1];
        
        printf("\nReceived packet: %d bytes, opcode: %d\n", n, opcode);
        
        if (opcode == TFTP_DATA) {
            /* Process DATA packet
             * DATA format: [opcode:2][block#:2][data:0-512]
             */
            if (n < 4) {
                printf("DATA packet too small (missing block number)\n");
                break;
            }
            
            /* Extract block number from bytes 2-3 */
            uint16_t block_num = (buffer[2] << 8) | buffer[3];
            int data_len = n - 4;  // Subtract 4 bytes for opcode and block number
            
            printf("DATA Block #%d: %d bytes of data\n", block_num, data_len);
            
            /* Verify this is the expected block (basic error checking) */
            if (block_num != expected_block) {
                printf("Warning: Expected block %d, got block %d\n", 
                       expected_block, block_num);
            }
            
            /* Display the file data (first 100 bytes for readability) */
            printf("Data content (first %d bytes):\n", 
                   data_len > 100 ? 100 : data_len);
            printf("--- START DATA ---\n");
            write(STDOUT_FILENO, buffer + 4, data_len > 100 ? 100 : data_len);
            if (data_len > 100) {
                printf("\n... (%d more bytes)", data_len - 100);
            }
            printf("\n--- END DATA ---\n");
            
            total_bytes += data_len;
            expected_block++;
            
            /* Check if this is the last packet
             * TFTP signals end-of-file when DATA packet contains < 512 bytes
             */
            if (data_len < DATA_SIZE) {
                printf("\nFile transfer complete! (last block was %d bytes)\n", data_len);
                break;
            }
            
            /* In a complete implementation, we would send an ACK here:
             * ACK format: [opcode:2 (0x0004)][block#:2]
             * 
             * char ack_packet[4];
             * ack_packet[0] = 0x00; ack_packet[1] = 0x04; // ACK opcode
             * ack_packet[2] = buffer[2]; ack_packet[3] = buffer[3]; // Echo block#
             * sendto(sockfd, ack_packet, 4, 0, (struct sockaddr *)&from_addr, addr_len);
             */
            
        } else if (opcode == TFTP_ERROR) {
            /* Handle ERROR packet
             * ERROR format: [opcode:2][error_code:2][error_msg]\0
             */
            if (n >= 4) {
                uint16_t error_code = (buffer[2] << 8) | buffer[3];
                printf("TFTP Error %d: %s\n", error_code, 
                       n > 4 ? buffer + 4 : "No error message");
            }
            break;
            
        } else {
            printf("Unexpected opcode: %d\n", opcode);
            break;
        }
    }
    
    /* Step 6: Cleanup and Statistics */
    close(sockfd);
    printf("\n=== Transfer Summary ===\n");
    printf("Total bytes received: %d\n", total_bytes);
    printf("Socket closed successfully\n");
    
    return 0;
}
