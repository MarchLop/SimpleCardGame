#pragma once

#include <iostream>
#include <vector>
#include <utility>
#include <random>
#include <memory>
#include <algorithm>
#include <set>

inline std::random_device rd;
inline std::mt19937 tt(rd()); 
class Card{
    public:
    Card(int n,int s):num(n),suit(s){}
    bool operator<(const Card & other)const{
        return (num==other.num)?suit<other.suit:num<other.num ;
    }
    bool operator==(const Card &other)const{
        return (num==other.num)&&(suit==other.suit);
    }
    public:
    int num,suit;// spade=3,heart=2;diamond=1;clove=0
    enum Num{
        THREE,FOUR,FIVE,SIX,SEVEN,EIGHT,NINE,TEN,JACK,QUE,KING,ACE,TWO
    };
    enum Suit{
        CLOVE,DIAMOND,SPADE,HERAT
    };
};

enum class CardsType{
    INVALID,
    SINGLE,       // 单张 A
    PAIR,         // 对子 AA
    THREE,        // 三条 AAA
    THREE_TWO,    // 三带二 AAABB
    STRAIGHT,     // 顺子(5张起) ABCDE...
    FOUR,         // 炸弹(4张) AAAA
    FOUR_TWO,     // 四带二 AAAABB
};

CardsType tell_type(const std::vector<Card>& play){
    if(play.empty()) return CardsType::INVALID;
    size_t n = play.size();
    // 检查顺子（需要至少5张，且不能包含2，且点数严格连续且无重复点数）
    if(n >= 5){
        bool is_straight = true;
        if(play.back().num == Card::TWO) is_straight = false;
        for(size_t i = 1; i < n; ++i){
            if(play[i].num == play[i-1].num){ is_straight = false; break; } // 重复点
            if(play[i].num != play[0].num + int(i)) { is_straight = false; break; }
        }
        if(is_straight) return CardsType::STRAIGHT;
    }
    switch(n){
        case 1:{return CardsType::SINGLE;}
        case 2:{
            if(play[0].num!=play[1].num)return CardsType::INVALID;
            return CardsType::PAIR;
        }
        case 3:{
            if(play[0].num!=play[1].num||play[2].num!=play[1].num)return CardsType::INVALID;
            return CardsType::THREE;
        }
        case 4:{
            for(const Card& temp:play){
                if(temp.num!=play.begin()->num)return CardsType::INVALID;
            }
            return CardsType::FOUR;
        }
        case 5:{
            if(play[1].num!=play[0].num||
                play[3].num!=play[4].num||
                (play[2].num!=play[3].num&&play[1].num!=play[2].num))return CardsType::INVALID;
            return CardsType::THREE_TWO; 
        }
        case 6:{
            if(play[1].num!=play[0].num||
                play[5].num!=play[4].num||
                play[2].num!=play[3].num||
                (play[2].num!=play[1].num&&play[4].num!=play[3].num))return CardsType::INVALID;
            return CardsType::FOUR_TWO;    
        }
        default :{
            return CardsType::INVALID;
        }
    }
}

class Deck{
    public:
    Deck(){
        for(int i=Card::THREE;i<=Card::TWO;i++){
            for(int j=Card::CLOVE;j<=Card::HERAT;j++){
                deck.push_back((Card){i,j});
            }
        }
        std::cerr << "Deck constructed, size=" << deck.size() << "\n";
    }
    Card Drawcard(){
        std::cerr << "Deck::Drawcard called, size=" << deck.size() << "\n";
        if(deck.empty()) throw std::runtime_error("Deck is empty");
        Card temp=deck.back();
        deck.pop_back();
        return temp;
    }
    void shuffle(){
        std::cerr << "Deck::shuffle before size=" << deck.size() << "\n";
        std::shuffle(deck.begin(), deck.end(), tt);
        std::cerr << "Deck::shuffle after size=" << deck.size() << "\n";
    }
    private:
    std::vector<Card> deck;
};

class GamePlayer{
    public:
    void DrawCard(const Card &d,int &now,int &i){
        hand.push_back(d);
        if(d.num==Card::THREE&&d.suit==Card::CLOVE)now=i;
        if(d.num==Card::TWO&&d.suit==Card::HERAT)identity+=2;
        if(d.num==Card::KING&&d.suit==Card::SPADE)identity+=1;
        std::sort(hand.begin(),hand.end());
    }
    bool PlayCard(const std::vector<Card>& play){
        std::multiset<Card> tmp(hand.begin(), hand.end());
        for(const Card &c : play){
            auto it = tmp.find(c);
            if(it == tmp.end()) return false; // 非法出牌
            tmp.erase(it);
        }
        // 验证通过，真正从 hand 中移除
        for(const Card &c : play){
            auto it = std::find(hand.begin(), hand.end(), c);
            if(it != hand.end()) hand.erase(it);
        }

        if(hand.empty()){
            over = 1;
            return true;
        }
        return false;
    }
    const std::vector<Card>& GetHand() const {
        return hand;
    }
    bool over=0;
    int identity=0;
    private:
    std::vector<Card> hand;
    
};

class Game{// note who host outside
    public:
        std::shared_ptr<GamePlayer> AddPlayer(){
            auto newplayer = std::make_shared<GamePlayer>();
            players.push_back(newplayer);
            return newplayer;
        }

        bool RemovePlayer(const std::shared_ptr<GamePlayer>& player){
            auto it = std::find(players.begin(), players.end(), player);
            if(it == players.end()) return false;
            players.erase(it);
            return true;
        }

        bool gamestart(){
            std::cerr << "Game::gamestart entered players=" << players.size() << " status=" << status << "\n";
            if(players.size()!=4) {
                std::cerr << "Game::gamestart failed: wrong player count=" << players.size() << "\n";
                return 0;
            }
            if(status){
                std::cerr << "Game::gamestart called but status already true\n";
                return 0;
            }
            std::cerr << "Game::gamestart executing shuffle/deal\n";
            deck.shuffle();
            try{
                for(int j=0;j<13;++j)
                for(int i=0;i<4;++i){
                    players[i]->DrawCard(deck.Drawcard(),nowplayer,i);
                }
            } catch(const std::exception &e){
                std::cerr << "Exception during gamestart dealing: " << e.what() << "\n";
                return 0;
            }
            status=1;
            turn=0;
            table_clear();
            return 1;
        }

        int round(const std::vector<Card>& play){// 0:error 1:continue 2:player_over 确保牌按格式传进来
            if(!check(play))return 0;
            size_t before = players[nowplayer]->GetHand().size();
            bool finished = players[nowplayer]->PlayCard(play);
            if(players[nowplayer]->GetHand().size() == before) return 0;

            card_in_table={tell_type(play),play};
            if(finished){
                over_num++;
            }
            do{
                nowplayer++;
                nowplayer%=4;
            }while(players[nowplayer]->over);
            return finished ? 2 : 1;
        }

        bool check(const std::vector<Card>& play){
            CardsType temp = tell_type(play);
            if(temp == CardsType::INVALID) return false;
            if(card_in_table.first == CardsType::INVALID) return true;
            if(temp != card_in_table.first) return false;

            auto min_suit = [](const std::vector<Card>& cards){
                Card m = cards[0];
                for(size_t i = 1; i < cards.size(); ++i){
                    if(cards[i]<m) m = cards[i];
                }
                return m;
            };

            switch(temp){
                case CardsType::SINGLE:
                    if(play[0].num > card_in_table.second[0].num) return true;
                    if(play[0].num == card_in_table.second[0].num && play[0].suit > card_in_table.second[0].suit) return true;
                    return false;
                case CardsType::PAIR: {
                    if(play[0].num != card_in_table.second[0].num) return play[0].num > card_in_table.second[0].num;
                    return  min_suit(card_in_table.second) < min_suit(play);
                }
                case CardsType::THREE:
                case CardsType::FOUR:
                case CardsType::THREE_TWO:
                case CardsType::FOUR_TWO:
                    return play[0].num > card_in_table.second[0].num;
                case CardsType::STRAIGHT: {
                    if(play.back().num != card_in_table.second.back().num) return play.back().num > card_in_table.second.back().num;
                    return min_suit(card_in_table.second) < min_suit(play);
                }
                default:
                    return false;
            }
        }

        void table_clear(){//when host==nowplayer
            card_in_table.first=CardsType::INVALID;
            turn++;
        }

        void reset(){
            players.clear();
            status = 0;
            over_num = 0;
            nowplayer = tt() % 4;
            turn = 0;
            table_clear();
            deck = Deck();
        }
    public:
        std::vector<std::shared_ptr<GamePlayer>> players;
        bool status=0;
        int nowplayer=tt()%4;
        int turn=0;
        int over_num=0;
        std::pair<CardsType,std::vector<Card>> card_in_table;
    private:
        Deck deck;
};

