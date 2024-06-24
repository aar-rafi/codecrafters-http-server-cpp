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
#include <fstream>
#include <sstream>
#include <algorithm>
#include <zlib.h>
using namespace std;

string dir;

std::string gzipCompress(const std::string &data)
{
  z_stream zs;
  memset(&zs, 0, sizeof(zs));

  if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
  {
    throw std::runtime_error("deflateInit2 failed");
  }

  zs.next_in = (Bytef *)data.data();
  zs.avail_in = data.size();

  int ret;
  char buffer[1024];
  std::string compressedData;

  do
  {
    zs.next_out = reinterpret_cast<Bytef *>(buffer);
    zs.avail_out = sizeof(buffer);

    ret = deflate(&zs, Z_FINISH);

    if (compressedData.size() < zs.total_out)
    {
      compressedData.append(buffer, zs.total_out - compressedData.size());
    }
  } while (ret == Z_OK);

  deflateEnd(&zs);

  if (ret != Z_STREAM_END)
  {
    throw std::runtime_error("deflate failed");
  }

  return compressedData;
}

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
  for (string s : requestparts)
    cout << "[" << s << "]";
  vector<string> tokens = string_split(requestparts[0], '/');
  string s;
  if (tokens[1] == " HTTP")
    s = "HTTP/1.1 200 OK\r\n\r\n";
  else if (tokens[1] == "echo")
  {
    int len = tokens[2].length() - 5;
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";
    if (requestparts.size() > 4)
    {
      auto it = find_if(requestparts.begin(), requestparts.end(), [](string s)
                        { return s.find("Accept-Encoding") != string::npos; });
      if (it != requestparts.end())
      {
        int pos = distance(requestparts.begin(), it);
        string temp = requestparts[pos].substr(pos + 2);
        vector<string> enc = string_split(temp, ' ');
        it = find_if(enc.begin(), enc.end(), [](string s)
                     { return s.find("gzip") != string::npos; });
        if (it != enc.end())
        {
          s += "Content-Encoding: gzip\r\n";
          tokens[2] = gzipCompress(tokens[2].substr(0, len));
          s += "Content-Length: " + to_string(tokens[2].size()) + "\r\n\r\n" + tokens[2];
        }
      }
      s += "Content-Length: " + to_string(len) + "\r\n\r\n" + tokens[2].substr(0, len);
    }
    else
      s += "Content-Length: " + to_string(len) + "\r\n\r\n" + tokens[2].substr(0, len);
  }
  else if (tokens[1] == "files")
  {
    tokens[2] = tokens[2].substr(0, tokens[2].length() - 5);
    if (tokens[0] == "GET ")
    {
      cout << dir + tokens[2] << endl;
      ifstream file(dir + tokens[2]);
      if (file.good())
      {
        stringstream ss;
        ss << file.rdbuf();
        string file_contents = ss.str();
        s = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + to_string(file_contents.size()) + "\r\n\r\n" + file_contents;
        file.close();
      }
      else
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    else if (tokens[0] == "POST ")
    {
      ofstream file(dir + tokens[2]);
      if (file.good())
      {
        file << requestparts[requestparts.size() - 1];
        s = "HTTP/1.1 201 Created\r\n\r\n";
        file.close();
      }
      else
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
  }
  else if (tokens[1] == "user-agent HTTP")
  {
    tokens.clear();
    tokens = string_split(requestparts[requestparts.size() - 3], ' '); // last 2 are empty, +1 for 0 index
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + to_string(tokens[1].length()) + "\r\n\r\n" + tokens[1];
  }
  else
    s = "HTTP/1.1 404 Not Found\r\n\r\n";

  // char write_buffer[512] = {0};
  // strcpy(write_buffer, s.c_str());
  // write(client, write_buffer, strlen(write_buffer));
  size_t totalsent = 0;
  while (totalsent < s.size())
  {
    size_t sent = write(client, s.c_str() + totalsent, s.size() - totalsent);
    if (sent < 0)
    {
      cerr << "write failed\n";
      break;
    }
    totalsent += sent;
  }
  close(client);
}

int main(int argc, char **argv)
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  if (argc > 2)
  {
    if (strcmp(argv[1], "--directory") == 0)
      dir = argv[2];
    cout << argv[0] << " " << argv[1] << " " << argv[2] << endl;
  }

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
