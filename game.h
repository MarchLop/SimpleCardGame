#include <vector>
#include <utility>
#include <random>
#include <algorithm>

std::random_device rd;
std::mt19937 tt(rd()); 
class card{
    public:
    card(int n,int s):num(n),suit(s){}
    int num,suit;// spade=3,heart=2;diamond=1;clove=0
    bool operator<(const card & other)const{
        return (suit==other.suit)? num<other.num : suit<other.suit;
    }
    bool operator==(const card &other)const{
        return (num==other.num)&&(suit==other.suit);
    }
};

class Deck{
    public:
    Deck(){
        for(int i=1;i<=13;i++){
            for(int j=0;j<=3;j++){
                deck.push_back((card){i,j});
            }
        }
    }
    void shuffer(){
        std::shuffle(deck.begin(), deck.end(), tt);
    }
    private:
    std::vector<card> deck;
};

class GamePlayer{
    public:
    void DarwCard(card &d){
        hand.push_back(d);
        std::sort(hand.begin(),hand.end());
    }
    void PlayCard(card &d){
        
    }
    private:
    int id;
    std::vector<card> hand;
};



