#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <unordered_map>
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <memory>

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
    Room(int i):id(i), started(false){}
    int id;
    Game game;
    std::vector<Player*> players;
    std::unordered_map<Player*, std::shared_ptr<GamePlayer>> player_map;
    bool started;
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
std::unordered_map<std::string,int> ty=
{
{"auth",0},
{"change_name",1},
{"create_room",2},
{"join_room",3},
{"leave_room",4},
{"ready",5},
};

Room* find_room_by_player(Player* player) {
    for (auto &room : rooms) {
        if (std::find(room.players.begin(), room.players.end(), player) != room.players.end()) {
            return &room;
        }
    }
    return nullptr;
}

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
                resp["name"] = it->second.name;
                ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
            } else {
                auto it = auth_players.find(token);
                if(it != auth_players.end()){
                    it->second.hdl = hdl;
                    hdl_to_players[hdl] = &it->second;
                    json resp;
                    resp["type"] = "auth_ok";
                    resp["token"] = token;
                    resp["name"] = it->second.name;
                    ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
                } else {
                    auto [it2, inserted] = auth_players.emplace(token, Player{hdl, token});
                    hdl_to_players[hdl] = &it2->second;
                    json resp;
                    resp["type"] = "auth_ok";
                    resp["token"] = token;
                    resp["name"] = it2->second.name;
                    ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
                }
            }
            break;
        }
        case 1:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                json err{
                    {"type","error"},
                    {"msg","not_authed"},
                    {"code",101}
                };
                ws_server.send(hdl, err.dump(),websocketpp::frame::opcode::text);
                break;
            }
            itp->second->name = j.value("name", std::string("NONE"));
            json resp;
            resp["type"] = "change_name_ok";
            resp["name"] = itp->second->name;
            ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
            break;
        }
        case 2:{
            auto itp = hdl_to_players.find(hdl);
            json msgt;
            if(itp == hdl_to_players.end()){
                msgt["type"] = "error";
                msgt["code"] = 201;
                msgt["message"] = "not_authed";
                ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
                break;
            }
            if((int)rooms.size() >= 10){
                msgt["type"] = "error";
                msgt["code"] = 202 ;
                msgt["message"] = "max_rooms";
                ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
                break;
            }
            rooms.emplace_back(next_room_id);
            Room &room = rooms.back();
            room.players.push_back(itp->second);
            room.player_map[itp->second] = room.game.AddPlayer();
            msgt["type"] = "room_created";
            msgt["room_id"] = next_room_id;
            msgt["player_count"] = 1;
            msgt["max_players"] = 4;
            msgt["started"] = false;
            ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
            next_room_id++;
            break;
        }
        case 3:{
            if(!j.contains("room_id")){
                json err{
                    {"type","error"},
                    {"code",300},
                    {"msg","none_content"}
                };
                ws_server.send(hdl,err.dump(),websocketpp::frame::opcode::text);
                break;
            }
            auto itp = hdl_to_players.find(hdl);
            json msgt;
            if(itp == hdl_to_players.end()){
                msgt["type"] = "error";
                msgt["code"] = 301;
                msgt["message"] = "not_authed";
                ws_server.send(hdl, msgt.dump(), websocketpp::frame::opcode::text);
                break;
            }
            int roomid = j.value("room_id", 0);
            if(roomid <= 0 || roomid > (int)rooms.size()){
                json err{
                    {"type","error"},
                    {"code",302},
                    {"msg","no_found_room"}
                };
                ws_server.send(hdl,err.dump(),websocketpp::frame::opcode::text);
                break;
            }
            Room &room = rooms[roomid - 1];
            if(room.players.size() == 4){
                json err{
                    {"type","error"},
                    {"code",303},
                    {"msg","rooms_player_enough"}
                };
                ws_server.send(hdl,err.dump(),websocketpp::frame::opcode::text);
                break;
            }
            if(std::find(room.players.begin(), room.players.end(), itp->second) != room.players.end()){
                json err{
                    {"type","error"},
                    {"code",304},
                    {"msg","already_in_room"}
                };
                ws_server.send(hdl,err.dump(),websocketpp::frame::opcode::text);
                break;
            }
            room.players.push_back(itp->second);
            room.player_map[itp->second] = room.game.AddPlayer();
            json msgs{
                {"type","room_joined"},
                {"room_id",roomid},
                {"player_count",(int)room.players.size()},
                {"max_players",4},
                {"started",false}
            };
            ws_server.send(hdl,msgs.dump(),websocketpp::frame::opcode::text);
            json bmsg{
                {"type","player_joined"},
                {"player_id",(int)room.players.size()},
                {"player_name",room.players.back()->name},
                {"room_id",roomid},
                {"player_count",(int)room.players.size()}
            };
            broadcast(room,bmsg.dump());
            break;
        }
        case 4:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                json err{
                    {"type","error"},
                    {"code",101},
                    {"message","not_authed"}
                };
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            Player* player = itp->second;
            Room* room_ptr = find_room_by_player(player);
            if(!room_ptr){
                json err{
                    {"type","error"},
                    {"code",305},
                    {"message","not_in_room"}
                };
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            Room &room = *room_ptr;
            auto it = std::find(room.players.begin(), room.players.end(), player);
            int player_id = (int)(std::distance(room.players.begin(), it) + 1);
            room.players.erase(it);
            auto gp_it = room.player_map.find(player);
            if(gp_it != room.player_map.end()){
                room.game.RemovePlayer(gp_it->second);
                room.player_map.erase(gp_it);
            }
            player->ready = false;
            json resp{
                {"type","leave_room_ok"},
                {"room_id", room.id},
                {"player_count", (int)room.players.size()}
            };
                ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
                json bmsg{
                    {"type","player_left"},
                    {"player_id", player_id},
                    {"room_id", room.id},
                    {"player_count", (int)room.players.size()}
                };
                broadcast(room, bmsg.dump());
                break;
        }
        case 5:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                json err{
                    {"type","error"},
                    {"code",101},
                    {"message","not_authed"}
                };
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            Player* player = itp->second;
            Room* room_ptr = find_room_by_player(player);
            if(!room_ptr){
                json err{
                    {"type","error"},
                    {"code",305},
                    {"message","not_in_room"}
                };
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            Room &room = *room_ptr;
            if(room.started){
                json err{
                    {"type","error"},
                    {"code",306},
                    {"message","game_already_started"}
                };
                ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
                break;
            }
            player->ready = true;
            int ready_count = std::count_if(room.players.begin(), room.players.end(), [](Player* p){ return p->ready; });
            auto it = std::find(room.players.begin(), room.players.end(), player);
            int player_id = (int)(std::distance(room.players.begin(), it) + 1);
            json ready_msg{
                {"type","player_ready"},
                {"player_id", player_id},
                {"room_id", room.id},
                {"ready_count", ready_count},
                {"player_count", (int)room.players.size()}
            };
            broadcast(room, ready_msg.dump());
            if(ready_count == (int)room.players.size() && room.players.size() == 4){
                if(room.game.gamestart()){
                    room.started = true;
                    for(auto &p : room.players){
                        auto gp_it2 = room.player_map.find(p);
                        if(gp_it2 == room.player_map.end()) continue;
                        json start_msg{
                            {"type","game_start"},
                            {"room_id", room.id},
                            {"hand", json::array()},
                            {"first_turn", room.game.nowplayer}
                        };
                        for(const auto &card : gp_it2->second->GetHand()){
                            start_msg["hand"].push_back({
                                {"num", card.num},
                                {"suit", card.suit}
                            });
                        }
                        ws_server.send(p->hdl, start_msg.dump(), websocketpp::frame::opcode::text);
                    }
                }
            }
            break;
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