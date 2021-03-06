#include "stdafx.h"
#include <SFML/Network.hpp>
#include "Tools.h"
#include "Globals.h"
#include "Game.h"
#include "PathFinder.h"
#include "Command_Enforcer.h"
#include "GameLoop.h"

const unsigned short PORT = 53000;

void Client_Disconnecting(unsigned int i) {
	std::cout << std::endl << CurrentTime();
	std::cout << "Client disconnected: " << players[i].socket->getRemoteAddress() << ' ' << players[i].username << std::endl;

	globalMutex.lock();
	//Drop 'just grabbed' item
	for (Item &item : items) {
		if (item.owner == players[i].id && item.grabbed && !item.equipped && !item.inInventory) {
			item.owner = -1;
			item.grabbed = false;
			break;
		}
	}

	//Broadcast deleted player id
	broadcast_Delete_Online_Player(players[i]);

	//Remove player from target list
	Enemies_Target_List_Remove(players[i]);

	//Delete player
	selector.remove(*players[i].socket);
	players[i].socket->disconnect();
	delete players[i].socket;
	players.erase(players.begin() + i);
	globalMutex.unlock();

	std::cout << CurrentTime() << "Clients: " << players.size() << std::endl << "Me: ";
}

#include "Commands.h"

bool command_interpretation(Player &player, sf::Packet &packet, float deltaTime) {
	unsigned short command = 65535;
	packet >> command;

	if (command == Login) {
		char username[20], password[20];
		packet >> username >> password;
		login(player, username, password);
	}
	//Client Logged in?
	else if (!player.logged_in) {
		return 0;
	}
	if (command == Move_Player) {
		move_Player(player, packet, deltaTime);
	}
	else if (command == Attack_Player) {
		player.attack();
		broadcast_Attack_Player(player);
	}
	else if (command == Running) {
		Running_Player(player);
	}
	else if (command == Walking) {
		Walking_Player(player);
	}
	//Item:
	else if (command == Send_Items) {
		sendItems(player);
	}
	else if (command == Item_Grabbed) {
		item_Grabbed(player, packet);
	}
	else if (command == Item_Released) {
		item_Released(player, packet);
	}
	else if (command == Item_Equipped) {
		//item_Equipped(player, packet);
	}
	else if (command == Item_inInventory) {
		item_Insert(player, packet);
	}
	else if (command == Item_Auto_Insert) {
		item_AutoInsert(player, packet);
	}
	else if (command == Item_Take_Out) {
		item_Take_Out(player, packet);
	}
	else if (command == Item_Repositioning) {
		item_Repositioned(player, packet);
	}
	//Chat:
	else if (command == Chat_Message) {
		std::string msg;
		packet >> msg;
		std::cout << std::endl << CurrentTime() << player.username << ": " << msg << std::endl << "Me: ";
		broadcast_Message(player, msg);
	}
	//Sends
	else if (command == Send_Player) {
		sendPlayer(player);
	}
	else if (command == Send_Walls) {
		sendWalls(player);
	}
	else if (command == Send_Online_Players) {
		sendOnlinePlayersTo(player);
	}
	else if (command == Send_Daemons) {
		send_Daemons(player);
	}
	else if (command == Send_Safezones) {
		send_Safezones(player);
	}
	else if (command == Send_Regenerators) {
		send_Regenerators(player);
	}
	else if (command == Send_Respawn) {
		if (player.dead)
			respawn_Player(player);
	}
	player.time = 0;
	return 1;
}

void Search_for_Clients(void)
{
	if (selector.isReady(listener))
	{
		//Add New Client
		Player player(ClientID++, 0, 0);
		if (listener.accept(*player.socket) == sf::Socket::Done)
		{
			players.push_back(player);
			selector.add(*player.socket);
			std::cout << std::endl << CurrentTime() << "New Client: " << player.socket->getRemoteAddress() << std::endl << "Me: ";
		}
		//Delete Disconnected Client
		else
		{
			std::cout << std::endl << CurrentTime() << "Client disconnected: " << player.socket->getRemoteAddress() << std::endl;
			player.socket->disconnect();
			std::cout << CurrentTime() << "Clients: " << players.size() << std::endl << "Me: ";
		}
	}
}

void Receive_Packets()
{
	sf::Clock clock;
	unsigned int i = 0;
	while (!quit)
	{
		if (selector.wait())
		{
			Search_for_Clients();

			for (i = 0; i < players.size(); ++i) {
				players[i].time += clock.getElapsedTime().asSeconds();
				if (selector.isReady(*players[i].socket))
				{
					//Receive Messages
					sf::Packet packetReceive;
					if (players[i].socket->receive(packetReceive) == sf::Socket::Done) {
						command_interpretation(players[i], packetReceive, clock.getElapsedTime().asSeconds());
					}
					//Delete Disconnected Client
					else if (players[i].socket->receive(packetReceive) == sf::Socket::Disconnected) {
						Client_Disconnecting(i);
						continue;
					}
				}
				if (players[i].time > 300) {
					std::cout << players[i].socket->getRemoteAddress() << ' ' << players[i].username << " Time out" << std::endl;
					Client_Disconnecting(i);
				}
			}
			clock.restart();
		}
	}
}

void SendMessage()
{
	sf::Packet packet;
	std::cout << CurrentTime() << "Me: ";
	std::string s;
	getline(std::cin, s);

	if (!Server_Command(s) && s != "\n") {
		packet << Chat_Message << "Server: " + s;
		for (Player &player : players)
			if (selector.isReady(*player.socket))
				player.socket->send(packet);
	}
}

int main()
{
	std::cout << "Loading database ..." << std::endl;
	users.push_back(User(1, "guest", "asd"));
	users.push_back(User(2, "guest2", "asd"));
	users.push_back(User(3, "guest3", "asd"));
	users.push_back(User(4, "guest4", "asd"));

	std::cout << "Loading server ..." << std::endl;
	CreateMap();

	if (listener.listen(PORT) != sf::Socket::Done) {
		std::cout << "Can not use " << PORT << " port" << std::endl;
	}
	selector.add(listener);

	sf::Thread *thread_Receive = 0;
	sf::Thread *thread_gameLoop = 0;

	thread_Receive = new sf::Thread(&Receive_Packets);
	thread_gameLoop = new sf::Thread(&gameLoop);

	thread_Receive->launch();
	thread_gameLoop->launch();

	std::cout << "Server is ready." << std::endl;

	while (!quit) {
		SendMessage();
	}

	DeleteGame(thread_Receive, thread_gameLoop);
	listener.close();
	std::cout << "Server shutdown..." << std::endl;
}
