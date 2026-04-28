#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

class server{
    public: server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }
    private:
        tcp::acceptor acceptor_;

        void do_accept() {

        }

};

int main(){
    try{
    boost::asio::io_context io;
    tcp::acceptor acc(io,tcp::endpoint(tcp::v4(),1324));
    while(1){
    tcp::socket socket(io);
    acc.accept(socket);
    std::cout<<"sss\n";
    char msg[1201];
    size_t length = socket.read_some(boost::asio::buffer(msg));
    std::cout<< "Received: " << std::string(msg, length) << "\n";
    boost::asio::write(socket, boost::asio::buffer(msg, length));
    }
    }
    catch(std::exception& e){
       std::cout << "Error: " << e.what() << std::endl;
    };

    return 0;
}