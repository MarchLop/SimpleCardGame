#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <unordered_map>
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>
#include "game.h"

using json = nlohmann::json;
using websocketpp::connection_hdl;

typedef websocketpp::server<websocketpp::config::asio> server;

struct Player{
    Player(connection_hdl a,std::string b):hdl(a),token(b){}
    connection_hdl hdl;
    std::string token;
    std::string name="NONE";
    bool ready=0;
};

struct Room {
    Room(int i):id(i){game.AddPlayer();}
    int id;
    Game game;
    std::vector<Player*> players;
};

server ws_server;

std::string cha="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::vector<Room> rooms;
std::unordered_map<std::string,Player> auth_players;
std::map<connection_hdl,Player*, std::owner_less<connection_hdl>> hdl_to_players;
int next_player_id = 1;
int next_room_id = 1;

std::string romdom_token(){
    std::string s;
    std::uniform_int_distribution<int> dist(0, (int)cha.size() - 1);
    for(int i=0;i<48;i++){
        s += cha[dist(tt)];
    }
    return s;
} 

// 广播给房间所有人
void broadcast(Room& room, const std::string& msg) {
    for (auto& p : room.players) {
        ws_server.send(p->hdl, msg, websocketpp::frame::opcode::text);
    }
}


// 新连接
// void on_open(connection_hdl hdl) {
//     int now=next_player_id++;
//     playerid[hdl]=now;
//     json j={{"type","connected"},{"date",{"player_id",now}}};
//     ws_server.send(hdl,j.dump(),websocketpp::frame::opcode::text);
//     std::cout<<"Player"<<now<<" connected\n";
// }

// 收到消息
std::unordered_map<std::string,int> ty={{"auth",0},{"change_name",1},{"create_room",2}}; 

void on_message(connection_hdl hdl, server::message_ptr msg) {
    json j=json::parse(msg->get_payload());
    if(!j.contains("type"))return;
    std::string type=j.at("type").get<std::string>();
    auto it_type=ty.find(type);
    if(it_type==ty.end()){

        return;
    }
    switch(it_type->second){
        case 0:{
            std::string token = j.value("token", std::string());
            if(token.empty()){
                token = romdom_token();
                auto [it, inserted] = auth_players.emplace(token, Player{hdl, token});
                hdl_to_players[hdl] = &it->second;
                json resp;
                resp["type"] = "auth_ok";
                resp["token"] = token;
                ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
            } else {
                auto it = auth_players.find(token);
                if(it != auth_players.end()){
                    it->second.hdl = hdl;
                    hdl_to_players[hdl] = &it->second;
                    json msgs;
                    msgs["name"] = it->second.name;
                    msgs["type"] = "player_name";
                    msgs["token"] = token;
                    ws_server.send(hdl, msgs.dump(), websocketpp::frame::opcode::text);
                } else {
                    auto [it2, inserted] = auth_players.emplace(token, Player{hdl, token});
                    hdl_to_players[hdl] = &it2->second;
                    json msgs;
                    msgs["name"] = it2->second.name;
                    msgs["type"] = "player_name";
                    msgs["token"] = token;
                    ws_server.send(hdl, msgs.dump(), websocketpp::frame::opcode::text);
                }
            }
            break;
        }
        case 1:{
            {
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                json err{{"type","error"},{"msg","not_authed"}};
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            itp->second->name = j.value("name", std::string("NONE"));
            }
            break;
        }
        case 2:{
            {
            auto itp = hdl_to_players.find(hdl);
            json msgt;
            if(itp == hdl_to_players.end()){
                msgt["type"] = "room_created";
                msgt["res"] = "failed";
                msgt["reason"] = "not_authed";
                ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
                break;
            }
            if(rooms.size() >= 10){
                msgt["type"] = "room_created";
                msgt["res"] = "failed";
                msgt["reason"] = "max_rooms";
                ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
                break;
            }
            rooms.emplace_back(next_room_id);
            rooms.back().players.push_back(itp->second);
            msgt["type"] = "room_created";
            msgt["room_id"] = next_room_id;
            msgt["player_id"] = 1;
            msgt["res"] = "success";
            ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
            next_room_id++;
            }
        }

    }
}

// 断开连接
void on_close(connection_hdl hdl) {
    auto it = hdl_to_players.find(hdl);
    if(it != hdl_to_players.end()){
        Player* p = it->second;
        // 从所有房间移除该玩家指针
        for(auto &room : rooms){
            room.players.erase(std::remove(room.players.begin(), room.players.end(), p), room.players.end());
        }
        // 重置 auth_players 中对应玩家的 hdl（保留 token）
        for(auto &kv : auth_players){
            if(&kv.second == p){
                kv.second.hdl = connection_hdl();
                break;
            }
        }
        hdl_to_players.erase(it);
    }
}

int main() {
    ws_server.init_asio();

   // ws_server.set_open_handler(&on_open);
    ws_server.set_message_handler(&on_message);
    ws_server.set_close_handler(&on_close);

    ws_server.listen(9002);
    ws_server.start_accept();

    std::cout << "Server started on ws://localhost:9002\n";

    ws_server.run();
}