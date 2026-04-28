#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using websocketpp::connection_hdl;

typedef websocketpp::server<websocketpp::config::asio> server;

struct Player {
    connection_hdl hdl;
    int id;
};

struct Room {
    int id;
    std::vector<Player> players;
};

server ws_server;

std::vector<Room> rooms;
std::map<connection_hdl, int, std::owner_less<connection_hdl>> player_room;
int next_player_id = 1;
int next_room_id = 1;

// 广播给房间所有人
void broadcast(Room& room, const std::string& msg) {
    for (auto& p : room.players) {
        ws_server.send(p.hdl, msg, websocketpp::frame::opcode::text);
    }
}


// 新连接
void on_open(connection_hdl hdl) {

}

// 收到消息
void on_message(connection_hdl hdl, server::message_ptr msg) {
    
}

// 断开连接
void on_close(connection_hdl hdl) {
    
}

int main() {
    ws_server.init_asio();

    ws_server.set_open_handler(&on_open);
    ws_server.set_message_handler(&on_message);
    ws_server.set_close_handler(&on_close);

    ws_server.listen(9002);
    ws_server.start_accept();

    std::cout << "Server started on ws://localhost:9002\n";

    ws_server.run();
}