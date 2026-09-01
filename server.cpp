#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <unordered_map>
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>

#include <nlohmann/json.hpp>
#include "game.h"

using json = nlohmann::json;
using websocketpp::connection_hdl;

typedef websocketpp::server<websocketpp::config::asio> server;

enum class GameActionType {
    READY,
    PLAY,
    PASS,
    TIMEOUT
};

struct Player{
    Player(connection_hdl a,std::string b):hdl(a),token(b){}
    connection_hdl hdl;
    std::string token;
    std::string name="NONE";
    bool ready=0;
    int public_identity=0;
};

struct GameAction {
    GameActionType type;
    Player* player;
    std::vector<Card> cards;
};

struct Room {
    Room(int i, websocketpp::lib::asio::io_context& ios)
        : id(i), started(false), current_turn(0), pass_count(0), last_player(0), turn_timer(ios) {}

    int id;
    Game game;
    std::vector<Player*> players;
    std::unordered_map<Player*, std::shared_ptr<GamePlayer>> player_map;
    bool started;
    int current_turn;
    int pass_count;
    int last_player; // 1-based player_id of last successful play, 0 if table is clear
    std::vector<int> ranking;
    websocketpp::lib::asio::steady_timer turn_timer;
};

server ws_server;

std::string cha="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::vector<std::shared_ptr<Room>> rooms;
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
        try {
            if(p->hdl.expired()) continue;
            ws_server.send(p->hdl, msg, websocketpp::frame::opcode::text);
        } catch(...) {}
    }
}

void send_error(connection_hdl hdl, int code, const std::string& message) {
    json err{
        {"type","error"},
        {"code",code},
        {"message",message}
    };
    ws_server.send(hdl, err.dump(), websocketpp::frame::opcode::text);
}

int get_player_index(Room& room, Player* player) {
    auto it = std::find(room.players.begin(), room.players.end(), player);
    if (it == room.players.end()) return -1;
    return static_cast<int>(std::distance(room.players.begin(), it)) + 1;
}

bool parse_cards(const json& jcards, std::vector<Card>& out) {
    if(!jcards.is_array()) return false;
    for(const auto& item : jcards){
        if(!item.is_object() || !item.contains("num") || !item.contains("suit")) return false;
        out.emplace_back(item["num"].get<int>(), item["suit"].get<int>());
    }
    return true;
}

bool is_free_turn(const Room& room) {
    return room.game.card_in_table.first == CardsType::INVALID;
}

void sync_current_turn(std::shared_ptr<Room> room_ptr) {
    if(room_ptr->players.empty()) return;
    int size = static_cast<int>(room_ptr->players.size());
    int idx = room_ptr->game.nowplayer % size;
    int attempts = 0;
    while(attempts < size && room_ptr->game.players[idx]->over){
        idx = (idx + 1) % size;
        attempts++;
    }
    room_ptr->game.nowplayer = idx;
    room_ptr->current_turn = idx;
}

bool all_ready(Room& room) {
    if (room.players.empty()) return false;
    return std::all_of(room.players.begin(), room.players.end(), [](Player* p){ return p->ready; });
}

void send_your_turn(std::shared_ptr<Room> room_ptr) {
    if (room_ptr->players.empty()) return;
    sync_current_turn(room_ptr);
    Player* player = room_ptr->players[room_ptr->current_turn];
    json last_play = json::array();
    int last_player = 0;
    if(room_ptr->game.card_in_table.first != CardsType::INVALID){
        for(const auto &card : room_ptr->game.card_in_table.second){
            last_play.push_back({{"num", card.num}, {"suit", card.suit}});
        }
        last_player = room_ptr->last_player;
    }
    json msg{
        {"type","your_turn"},
        {"room_id", room_ptr->id},
        {"player_id", room_ptr->current_turn + 1},
        {"last_play", last_play},
        {"last_player", last_player},
        {"is_free", is_free_turn(*room_ptr)}
    };
    ws_server.send(player->hdl, msg.dump(), websocketpp::frame::opcode::text);
    json msgb{
        {"type","table_status"},
        {"room_id",room_ptr->id},
        {"player_status",json::array()},
        {"last_play", last_play},
        {"last_player", last_player},
        {"is_free", is_free_turn(*room_ptr)}
    };
    int now=0;
    for(auto p : room_ptr->players){
        auto gp_it = room_ptr->player_map.find(p);
        int hand_size = 0;
        bool is_over = false;
        if(gp_it != room_ptr->player_map.end() && gp_it->second){
            hand_size = (int)gp_it->second->GetHand().size();
            is_over = gp_it->second->over;
        }
        msgb["player_status"].push_back({
            {"player_id", ++now},
            {"playerhand_size", hand_size},
            {"player_public_identity", p->public_identity},
            {"is_over", is_over}
        });
    }
    broadcast(*room_ptr, msgb.dump());
}

void start_next_turn(std::shared_ptr<Room> room_ptr);
void process_action(std::shared_ptr<Room> room_ptr, GameAction action);

void apply_timeout(std::shared_ptr<Room> room_ptr) {
    if (room_ptr->players.empty()) return;
    int current = room_ptr->current_turn;
    room_ptr->pass_count++;
    int next_turn = (current + 1) % static_cast<int>(room_ptr->players.size());
    while(room_ptr->game.players[next_turn]->over && next_turn != current){
        next_turn = (next_turn + 1) % static_cast<int>(room_ptr->players.size());
    }
    room_ptr->game.nowplayer = next_turn;
    room_ptr->current_turn = next_turn;

    if(room_ptr->pass_count >= 3){
        room_ptr->game.table_clear();
        room_ptr->pass_count = 0;
        room_ptr->last_player = 0;
    }

    json msg{
        {"type","player_pass"},
        {"room_id", room_ptr->id},
        {"player_id", current + 1},
        {"next_turn", next_turn + 1},
        {"pass_count", room_ptr->pass_count}
    };
    broadcast(*room_ptr, msg.dump());
}

bool check_game_over(std::shared_ptr<Room> room_ptr) {
    return room_ptr->game.over_num >= 3;
}

void fill_remaining_ranking(std::shared_ptr<Room> room_ptr) {
    for(int i = 0; i < (int)room_ptr->players.size(); ++i){
        int pid = i + 1;
        if(std::find(room_ptr->ranking.begin(), room_ptr->ranking.end(), pid) == room_ptr->ranking.end()){
            room_ptr->ranking.push_back(pid);
        }
    }
}

void broadcast_game_over(std::shared_ptr<Room> room_ptr) {
    fill_remaining_ranking(room_ptr);
    json msg{
        {"type","game_over"},
        {"room_id", room_ptr->id},
        {"ranking", room_ptr->ranking}
    };
    broadcast(*room_ptr, msg.dump());
}

void apply_play(std::shared_ptr<Room> room_ptr, const std::vector<Card>& cards) {
    if (room_ptr->players.empty()) return;
    int player_idx = room_ptr->game.nowplayer;
    int result = room_ptr->game.round(cards);
    if(result == 0){
        send_error(room_ptr->players[player_idx]->hdl, 1004, "invalid_play");
        return;
    }
    for(const Card &d : cards){
        if(d.num==Card::TWO&&d.suit==Card::HERAT)room_ptr->players[player_idx]->public_identity+=2;
        if(d.num==Card::KING&&d.suit==Card::SPADE)room_ptr->players[player_idx]->public_identity+=1;
    }
    room_ptr->pass_count = 0;
    room_ptr->last_player = player_idx + 1;

    json result_msg{
        {"type","play_result"},
        {"room_id", room_ptr->id},
        {"player_id", player_idx + 1},
        {"cards", json::array()},
        {"cards_left", (int)room_ptr->player_map[room_ptr->players[player_idx]]->GetHand().size()},
        {"next_turn", room_ptr->game.nowplayer + 1}
    };
    for(const auto &card : cards){
        result_msg["cards"].push_back({{"num", card.num}, {"suit", card.suit}});
    }
    broadcast(*room_ptr, result_msg.dump());

    if(result == 2){
        room_ptr->ranking.push_back(player_idx + 1);
        json finish_msg{
            {"type","player_finish"},
            {"room_id", room_ptr->id},
            {"player_id", player_idx + 1},
            {"rank", room_ptr->game.over_num}
        };
        broadcast(*room_ptr, finish_msg.dump());
    }

    if(check_game_over(room_ptr)){
        broadcast_game_over(room_ptr);
    }
}

void apply_pass(std::shared_ptr<Room> room_ptr) {
    if (room_ptr->players.empty()) return;
    int current = room_ptr->game.nowplayer;
    room_ptr->pass_count++;
    int next_turn = (current + 1) % static_cast<int>(room_ptr->players.size());
    while(room_ptr->game.players[next_turn]->over && next_turn != current){
        next_turn = (next_turn + 1) % static_cast<int>(room_ptr->players.size());
    }
    room_ptr->game.nowplayer = next_turn;
    room_ptr->current_turn = next_turn;

    if(room_ptr->pass_count >= 3){
        room_ptr->game.table_clear();
        room_ptr->pass_count = 0;
        room_ptr->last_player = 0;
    }

    json msg{
        {"type","player_pass"},
        {"room_id", room_ptr->id},
        {"player_id", current + 1},
        {"next_turn", next_turn + 1},
        {"pass_count", room_ptr->pass_count}
    };
    broadcast(*room_ptr, msg.dump());
}

void process_action(std::shared_ptr<Room> room_ptr, GameAction action) {
    room_ptr->turn_timer.cancel();
    if (!room_ptr->started || room_ptr->players.empty()) return;

    switch (action.type) {
        case GameActionType::TIMEOUT:
            apply_timeout(room_ptr);
            break;
        case GameActionType::PLAY:
            apply_play(room_ptr, action.cards);
            break;
        case GameActionType::PASS:
            apply_pass(room_ptr);
            break;
        default:
            break;
    }

    if(!check_game_over(room_ptr)){
        start_next_turn(room_ptr);
    }
}

void start_next_turn(std::shared_ptr<Room> room_ptr) {
    if (!room_ptr->started || room_ptr->players.empty()) return;
    send_your_turn(room_ptr);
    room_ptr->turn_timer.expires_after(std::chrono::seconds(30));
    room_ptr->turn_timer.async_wait([room_ptr](const websocketpp::lib::error_code& ec) {
        if (ec) return;
        process_action(room_ptr, GameAction{GameActionType::TIMEOUT, room_ptr->players[room_ptr->current_turn], {}});
    });
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
{"play",6},
{"pass",7},
{"get_rooms",8},
{"player_list",9},
{"chat",10}
};

std::shared_ptr<Room> find_room_by_player(Player* player) {
    for (auto &room_ptr : rooms) {
        if (std::find(room_ptr->players.begin(), room_ptr->players.end(), player) != room_ptr->players.end()) {
            return room_ptr;
        }
    }
    return nullptr;
}

std::shared_ptr<Room> find_room_by_id(int room_id) {
    for (auto &room_ptr : rooms) {
        if (room_ptr->id == room_id) return room_ptr;
    }
    return nullptr;
}

void send_player_list(connection_hdl hdl, Room& room) {
    json msgs{
        {"type","player_list"},
        {"room_id", room.id},
        {"players", json::array()}
    };
    for(size_t idx = 0; idx < room.players.size(); ++idx){
        Player* player = room.players[idx];
        msgs["players"].push_back({
            {"player_id", (int)idx + 1},
            {"player_name", player->name},
            {"ready", player->ready}
        });
    }
    ws_server.send(hdl, msgs.dump(), websocketpp::frame::opcode::text);
}

void send_room_joined(connection_hdl hdl, Room& room, Player* player) {
    json msgs{
        {"type","room_joined"},
        {"room_id", room.id},
        {"player_id", get_player_index(room, player)},
        {"player_count", (int)room.players.size()},
        {"max_players", 4},
        {"started", room.started}
    };
    ws_server.send(hdl, msgs.dump(), websocketpp::frame::opcode::text);
}

void send_game_snapshot(connection_hdl hdl, Room& room, Player* player) {
    auto gp_it = room.player_map.find(player);
    if(gp_it == room.player_map.end()) return;
    json start_msg{
        {"type","game_start"},
        {"room_id", room.id},
        {"hand", json::array()},
        {"identity", gp_it->second->identity},
        {"first_turn", room.game.nowplayer + 1}
    };
    for(const auto &card : gp_it->second->GetHand()){
        start_msg["hand"].push_back({{"num", card.num}, {"suit", card.suit}});
    }
    ws_server.send(hdl, start_msg.dump(), websocketpp::frame::opcode::text);
}

void on_message(connection_hdl hdl, server::message_ptr msg) {
    json j;
    try {
        j = json::parse(msg->get_payload());
    } catch(...) {
        send_error(hdl, 300, "invalid_json");
        return;
    }
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
            if(itp == hdl_to_players.end()){send_error(hdl,101,"not_authed");break;}

            itp->second->name = j.value("name", std::string("NONE"));
            json resp{
                {"type", "change_name_ok"},
                {"name", itp->second->name}
            };
            ws_server.send(hdl, resp.dump(), websocketpp::frame::opcode::text);
            break;
        }
        case 2:{
            auto itp = hdl_to_players.find(hdl);
            
            if(itp == hdl_to_players.end()){send_error(hdl,201,"not_authed");break;}

            if(find_room_by_player(itp->second)){send_error(hdl,304,"already_in_room");break;}

            if((int)rooms.size() >= 10){send_error(hdl,202,"max_rooms");break;}

            rooms.emplace_back(std::make_shared<Room>(next_room_id, ws_server.get_io_context()));
            auto room_ptr = rooms.back();
            Room &room = *room_ptr;
            room.players.push_back(itp->second);
            // ensure ready flag is cleared when a player is added (token reuse shouldn't carry over ready)
            itp->second->ready = false;
            //room.player_map[itp->second] = room.game.AddPlayer();
            json msgs{
                {"type","room_created"},
                {"room_id",next_room_id},
                {"player_id",1},
                {"player_count",1},
                {"max_players", 4},
                {"started", false}
            };
            
            ws_server.send(hdl, msgs.dump(), websocketpp::frame::opcode::text);
            next_room_id++;
            break;
        }
        case 3:{
            if(!j.contains("room_id")){send_error(hdl,300,"none_content");break;}

            auto itp = hdl_to_players.find(hdl);
            json msgt;
            if(itp == hdl_to_players.end()){send_error(hdl,301,"not_authed");break;}

            int roomid = j.value("room_id", 0);
            auto room_ptr = find_room_by_id(roomid);
            if(!room_ptr){send_error(hdl,302,"no_found_room");break;}

            Room &room = *room_ptr;
            Player* player = itp->second;
            auto existing = find_room_by_player(player);
            if(existing){
                if(existing->id != roomid){
                    send_error(hdl,304,"already_in_room");
                    break;
                }
                send_room_joined(hdl, room, player);
                send_player_list(hdl, room);
                if(room.started) send_game_snapshot(hdl, room, player);
                break;
            }
            if(room.started){send_error(hdl,306,"game_already_started");break;}
            if(room.players.size() == 4){send_error(hdl,303,"rooms_player_enough");break;}

            room.players.push_back(player);
            player->ready = false;
            send_room_joined(hdl, room, player);
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
            if(itp == hdl_to_players.end()){send_error(hdl,101,"not_authed");break;}

            Player* player = itp->second;
            auto room_ptr = find_room_by_player(player);
            if(!room_ptr){send_error(hdl,305,"not_in_room");break;}

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
            if(itp == hdl_to_players.end()){send_error(hdl, 101, "not_authed");break;}

            Player* player = itp->second;
            auto room_ptr = find_room_by_player(player);
            if(!room_ptr){send_error(hdl, 305, "not_in_room");break;}

            Room &room = *room_ptr;
            if(room.started){
                send_error(hdl, 306, "game_already_started");
                break;
            }
            player->ready = true;
            int ready_count = std::count_if(room.players.begin(), room.players.end(), [](Player* p){ return p->ready; });
            int player_id = get_player_index(room, player);
            json ready_msg{
                {"type","player_ready"},
                {"player_id", player_id},
                {"room_id", room.id},
                {"ready_count", ready_count},
                {"player_count", (int)room.players.size()}
            };
            // send ready_ok ack to the requesting client
            json ack{
                {"type","ready_ok"},
                {"room_id", room.id},
                {"player_id", player_id},
                {"ready_count", ready_count}
            };
            ws_server.send(hdl, ack.dump(), websocketpp::frame::opcode::text);
            broadcast(room, ready_msg.dump());
            if(ready_count == (int)room.players.size() && room.players.size() == 4){
                auto room_shared = room_ptr;
                room_ptr->game.reset();
                room_ptr->player_map.clear();
                room_ptr->ranking.clear();
                room_ptr->last_player = 0;
                room_ptr->pass_count = 0;
                for(auto p : room_ptr->players){
                    p->public_identity = 0;
                    room_ptr->player_map[p] = room_ptr->game.AddPlayer();
                }
                websocketpp::lib::asio::post(ws_server.get_io_context(), [room_shared](){
                    Room &r = *room_shared;
                    if(!all_ready(r)) return;
                    if((int)r.players.size() != 4) return;
                    if(r.started) return;
                    if(!r.game.status && r.game.gamestart()){
                        r.started = true;
                        r.current_turn = r.game.nowplayer;
                        for(auto &p : r.players){
                            auto gp_it2 = r.player_map.find(p);
                            if(gp_it2 == r.player_map.end()) continue;
                            json start_msg{
                                {"type","game_start"},
                                {"room_id", r.id},
                                {"hand", json::array()},
                                {"identity",r.player_map[p]->identity},
                                {"first_turn", r.game.nowplayer + 1}
                            };
                            for(const auto &card : gp_it2->second->GetHand()){
                                start_msg["hand"].push_back({
                                    {"num", card.num},
                                    {"suit", card.suit}
                                });
                            }
                            ws_server.send(p->hdl, start_msg.dump(), websocketpp::frame::opcode::text);
                        }
                        start_next_turn(room_shared);
                    }
                });
            }
            break;
        }
        case 6:{
            if(!j.contains("cards")){
                send_error(hdl, 300, "missing_cards");
                break;
            }
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                send_error(hdl, 101, "not_authed");
                break;
            }
            Player* player = itp->second;
            auto room_ptr = find_room_by_player(player);
            if(!room_ptr){
                send_error(hdl, 305, "not_in_room");
                break;
            }
            if(!room_ptr->started){
                send_error(hdl, 307, "game_not_started");
                break;
            }
            if(room_ptr->game.nowplayer != room_ptr->current_turn){
                room_ptr->current_turn = room_ptr->game.nowplayer;
            }
            if(room_ptr->current_turn < 0 || room_ptr->current_turn >= (int)room_ptr->players.size()){
                send_error(hdl, 307, "invalid_turn");
                break;
            }
            if(room_ptr->players[room_ptr->current_turn] != player){
                send_error(hdl, 308, "not_your_turn");
                break;
            }
            std::vector<Card> cards;
            if(!parse_cards(j["cards"], cards)){
                send_error(hdl, 300, "invalid_cards");
                break;
            }
            Player* current_player = player;
            websocketpp::lib::asio::post(ws_server.get_io_context(), [room_ptr, current_player, cards = std::move(cards)](){
                process_action(room_ptr, GameAction{GameActionType::PLAY, current_player, cards});
            });
            break;
        }
        case 7:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                send_error(hdl, 101, "not_authed");
                break;
            }
            Player* player = itp->second;
            auto room_ptr = find_room_by_player(player);
            if(!room_ptr){
                send_error(hdl, 305, "not_in_room");
                break;
            }
            if(!room_ptr->started){
                send_error(hdl, 307, "game_not_started");
                break;
            }
            if(room_ptr->game.nowplayer != room_ptr->current_turn){
                room_ptr->current_turn = room_ptr->game.nowplayer;
            }
            if(room_ptr->current_turn < 0 || room_ptr->current_turn >= (int)room_ptr->players.size()){
                send_error(hdl, 307, "invalid_turn");
                break;
            }
            if(room_ptr->players[room_ptr->current_turn] != player){
                send_error(hdl, 308, "not_your_turn");
                break;
            }
            Player* current_player = player;
            websocketpp::lib::asio::post(ws_server.get_io_context(), [room_ptr, current_player](){
                process_action(room_ptr, GameAction{GameActionType::PASS, current_player, {}});
            });
            break;
        }
        case 8:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                send_error(hdl, 101, "not_authed");
                break;
            }
            json msgs{
                {"type","room_list"},
                {"rooms",json::array()}
            };
            for(auto &room: rooms){
                msgs["rooms"].push_back({
                    {"room_id",room->id},
                    {"player_count",(int)room->players.size()},
                    {"max_players",4},
                    {"started",room->started}
                });
            }
            ws_server.send(hdl,msgs.dump(),websocketpp::frame::opcode::text);
            break;
        }
        case 9:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                send_error(hdl, 101, "not_authed");
                break;
            }
            auto room = find_room_by_player(itp->second);
            if(!room){
                send_error(hdl, 305, "not_in_room");
                break;
            }
            json msgs{
                {"type","player_list"},
                {"room_id", room->id},
                {"players",json::array()}
            };
            for(size_t idx = 0; idx < room->players.size(); ++idx){
                Player* player = room->players[idx];
                msgs["players"].push_back({
                    {"player_id", (int)idx + 1},
                    {"player_name", player->name},
                    {"ready", player->ready}
                });
            }
            ws_server.send(hdl,msgs.dump(),websocketpp::frame::opcode::text);
            break;
        }
        case 10:{
            auto itp = hdl_to_players.find(hdl);
            if(itp == hdl_to_players.end()){
                send_error(hdl, 101, "not_authed");
                break;
            }
            Player* player = itp->second;
            auto room_ptr = find_room_by_player(player);
            if(!room_ptr){
                send_error(hdl, 305, "not_in_room");
                break;
            }
            std::string text = j.value("text", std::string());
            if(text.empty()){
                send_error(hdl, 300, "none_content");
                break;
            }
            int player_id = get_player_index(*room_ptr, player);
            if(text.empty() || text.size() > 200) break;
            json chat_msg{
                {"type","chat"},
                {"room_id", room_ptr->id},
                {"player_id", player_id},
                {"player_name", player->name},
                {"text", text}
            };
            broadcast(*room_ptr, chat_msg.dump());
            break;
        }
    }
}

// 断开连接
void on_close(connection_hdl hdl) {
    auto it = hdl_to_players.find(hdl);
    if (it == hdl_to_players.end()) return;
    Player* p = it->second;
    // A reconnect may have already replaced this player's connection. Do not let
    // the old socket's close event disconnect the new socket.
    std::owner_less<connection_hdl> less;
    if (p->hdl.lock() && (less(p->hdl, hdl) || less(hdl, p->hdl))) {
        hdl_to_players.erase(it);
        return;
    }
    // Keep the player in the room. The saved token can reconnect to the same seat.
    p->hdl = connection_hdl();
    for (auto& room : rooms) {
        if (!room->started && std::find(room->players.begin(), room->players.end(), p) != room->players.end())
            p->ready = false;
    }
    hdl_to_players.erase(it);
    for (auto& room : rooms) {
        if (std::find(room->players.begin(), room->players.end(), p) != room->players.end()) {
            json msg{{"type","player_connection"},{"room_id",room->id},{"player_id",get_player_index(*room,p)},{"connected",false}};
            broadcast(*room,msg.dump());
        }
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