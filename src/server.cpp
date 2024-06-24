#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <thread>
using namespace std;

vector<string> string_split(string &s, char delim, int len = 1)
{
  vector<string> tokens;
  size_t start = 0;
  size_t end = s.find(delim);
  while (end != string::npos)
  {
    tokens.push_back(s.substr(start, end - start));
    start = end + len;
    end = s.find(delim, start);
  }
  tokens.push_back(s.substr(start));
  return tokens;
}

void handle_client(int client)
{
  char read_buffer[512] = {0};
  int readbytes = read(client, read_buffer, 512);
  if (readbytes < 0)
  {
    cerr << "read failed\n";
    return;
  }
  cout << "read" << readbytes << " bytes " << endl;

  string temp(read_buffer);
  vector<string> requestparts = string_split(temp, '\r', 2);
  vector<string> tokens = string_split(requestparts[0], '/');
  string s;
  if (tokens[1] == " HTTP")
    s = "HTTP/1.1 200 OK\r\n\r\n";
  else if (tokens[1] == "echo")
  {
    int len = tokens[2].length() - 5;
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(len) + "\r\n\r\n" + tokens[2].substr(0, len);
  }
  else if (tokens[1] == "user-agent HTTP")
  {
    tokens.clear();
    tokens = string_split(requestparts[requestparts.size() - 3], ' '); // last 2 are empty, +1 for 0 index
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(tokens[1].length()) + "\r\n\r\n" + tokens[1];
  }
  else
    s = "HTTP/1.1 404 Not Found\r\n\r\n";

  char write_buffer[512] = {0};
  strcpy(write_buffer, s.c_str());
  write(client, write_buffer, strlen(write_buffer));
}

int main(int argc, char **argv)
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";
  while (true)
  {
    int client = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    std::cout << "Client connected\n";
    thread th(handle_client, client);
    th.detach();
  }

  close(server_fd);
  return 0;
}
