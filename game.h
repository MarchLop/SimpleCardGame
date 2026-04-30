#pragma once

#include <vector>
#include <utility>
#include <random>
#include <memory>
#include <algorithm>

std::random_device rd;
std::mt19937 tt(rd()); 
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
    bool f=1;//is_straight
    int now=1;
    if(play.rbegin()->num==Card::TWO)f=0;
    for(auto i=play.begin()+1;i!=play.end();i++){
        if(i->num!=play.begin()->num+now){
            f=0;break;
        }
        now++;
    }
    if(f)return CardsType::STRAIGHT;
    switch(play.size()){
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
                (play[2].num!=play[3].num&&play[1].num!=play[3].num))return CardsType::INVALID;
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
            for(int j=Card::CLOVE;j<=Card::SPADE;j++){
                deck.push_back((Card){i,j});
            }
        }
    }
    Card Drawcard(){
        Card temp=deck.back();
        deck.pop_back();
        return temp;
    }
    void shuffer(){
        std::shuffle(deck.begin(), deck.end(), tt);
    }
    private:
    std::vector<Card> deck;
};

class GamePlayer{
    public:
    void DarwCard(const Card &d){
        hand.push_back(d);
        if(d.num==Card::TWO&&d.suit==Card::HERAT)identity=1;
        if(d.num==Card::KING&&d.suit==Card::SPADE)identity=1;
        std::sort(hand.begin(),hand.end());
    }
    bool PlayCard(const std::vector<Card>& play){
        for(const Card &now : play){
            for(auto it = hand.begin(); it != hand.end(); ++it){
                if(now == *it){
                    hand.erase(it);
                    break;  
                }
            }
        }

        if(hand.empty()){
            over=1;
            return 1;
        }
        return 0;
    }
    bool over=0;
    private:
    int identity=0;
    std::vector<Card> hand;
    
};

class Game{// note who host outside
    public:
        std::shared_ptr<GamePlayer> AddPlayer(){
            auto newplayer = std::make_shared<GamePlayer>();
            players.push_back(newplayer);
            return newplayer;
        }

        bool gamestart(){
            if(players.size()!=4)return 0;
            deck.shuffer();status=1;
            for(int j=0;j<13;++j)
            for(int i=0;i<4;++i){
                players[i]->DarwCard(deck.Drawcard());
            }
            turn=0;
            table_clear();
            return 1;
        }

        int round(const std::vector<Card>& play){// 0:error 1:continue 2:player_over 3game_over确保牌按格式传进来
            if(!check(play))return 0;
            
            if(players[nowplayer]->PlayCard(play)){
                if(card_in_table.first!=CardsType::INVALID){
                    card_in_table={tell_type(play),play};
                }
                over_num++;
                do{
                nowplayer++;
                nowplayer%=4;
                }while(!players[nowplayer]->over);
                return 2;
            }
            card_in_table={tell_type(play),play};
            do{
                nowplayer++;
                nowplayer%=4;
            }while(!players[nowplayer]->over);
            return 1;
        }

        bool check(const std::vector<Card>& play){
            CardsType temp=tell_type(play);
            if(temp==CardsType::INVALID)return 0;
            if(card_in_table.first==CardsType::INVALID) return 1;
            if(temp!=card_in_table.first)return 0;
            if(card_in_table.second[0]<play[0])return 1;
            return 0;
        }

        void table_clear(){//when host==nowplayer
            card_in_table.first=CardsType::INVALID;
            turn++;
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

