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
    int num,suit;// spade=3,heart=2;diamond=1;clove=0
    bool operator<(const Card & other)const{
        return (suit==other.suit)? num<other.num : suit<other.suit;
    }
    bool operator==(const Card &other)const{
        return (num==other.num)&&(suit==other.suit);
    }
};

class Deck{
    public:
    Deck(){
        for(int i=1;i<=13;i++){
            for(int j=0;j<=3;j++){
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

        if(hand.empty()) return 1;
        return 0;
    }
    private:
    int id;
    std::vector<Card> hand;
};

class Game{
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
            return 1;
        }

        int round(const std::vector<Card>& play){// 0:error 1:continue 2:win
            if(!check(play))return 0;
            //card_in_table=
            if(players[nowplayer]->PlayCard(play))return 2;
            nowplayer++;
            nowplayer%=4;
            return 1;
        }

        bool check(const std::vector<Card>& play){
            //...
            return 1;
        }
    public:
        std::vector<std::shared_ptr<GamePlayer>> players;
        bool status=0;
        int nowplayer=tt()%4;
        //??? card_in_table
    private:
        Deck deck;
};

