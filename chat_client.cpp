//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include "asio.hpp"
#include "chat_message.hpp"
#include <ncurses.h>

#include <string>
#include <fstream>
#include <sstream>

std::string user_list_filename = "user_list_file.txt";

using asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

WINDOW *create_newwin(int height, int width, int starty, int startx)
{ WINDOW *local_win;
 local_win = newwin(height, width, starty, startx);
 box(local_win, int('g') , int('a')); /* 0, 0 gives default characters
 * for the vertical and horizontal
 * lines */
 wrefresh(local_win); /* Show that box */
 return local_win;
}

void destroy_win(WINDOW *local_win)
{
 /* box(local_win, ' ', ' '); : This won't produce the desired
 * result of erasing the window. It will leave it's four corners
 * and so an ugly remnant of window.
 */
 wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
 /* The parameters taken are
 * 1. win: the window on which to operate
 * 2. ls: character to be used for the left side of the window
 * 3. rs: character to be used for the right side of the window
 * 4. ts: character to be used for the top side of the window
 * 5. bs: character to be used for the bottom side of the window
 * 6. tl: character to be used for the top left corner of the window
 * 7. tr: character to be used for the top right corner of the window
 * 8. bl: character to be used for the bottom left corner of the window
 * 9. br: character to be used for the bottom right corner of the window
 */
 wrefresh(local_win);
 delwin(local_win);
}

bool ParseLine(const std::string& CokeMachineLine, std::vector<std::string>& params)
	{
		int x = 0, z = 0, i = 0;

		if (CokeMachineLine.length() == 0)
		{
			return false;
		}
		else
		{
			while (z < (int) CokeMachineLine.length())
			{
				x = CokeMachineLine.find(" ", z) - z;
				params[i++] = CokeMachineLine.substr(z, x);
				z += params[i-1].length() + 1;
			}
		}
		return true;
	}

class chat_client
{
public:
  chat_client(asio::io_context& io_context,
      const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context)
  {
    do_connect(endpoints);
  }

  void write(const chat_message& msg)
  {
    asio::post(io_context_,
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    asio::post(io_context_, [this]() { socket_.close(); });
  }



  void login()
	{

		int start_y, start_x;
		char temp[50];

		//vector <WINDOW*> WB;

		//getmaxyx(stdscr, height, width);

		start_y = 1;
		start_x = 1;


		WINDOW* win = newwin(20,80, start_y, start_x);
		//WB.push_back(win);
		refresh(); // refreshes the whole screen

		box(win, int('|'), int('-'));

		wrefresh(win); //refreshes the window only
		echo();

		int proceed = 1;
		std::string user_id, user_pw;
		while(proceed)
		{
			std::string user_id, user_pw;

			mvwprintw(win,2,1,"ID: ");
			wrefresh(win);

			wgetnstr(win,temp,50);
			user_id = temp;

			wrefresh(win);

			mvwprintw(win,4,1,"Password: ");
			wrefresh(win);

			wgetnstr(win,temp,50);
			user_pw = temp;
			wrefresh(win);

			//vector to hold user_id and user_pw as parameters
			std::vector<std::string> params(2);
			//taking input from file
			std::ifstream inputFile{user_list_filename};
			std::string temp;

			while(inputFile.good())
			{
				std::getline(inputFile,temp);
				if(ParseLine(temp,params))
				{
					if(user_id == params[0] && user_pw == params[1])
					{
						proceed = 0;
						break;
					}
				}
			}
			//close file
			inputFile.close();

			if(proceed == 0)
			{
				break;

			}else
			{

				mvwprintw(win,6,1, "\nThe account mentioned above is not in our database\n");
				wrefresh(win);

				char yn;
				mvwprintw(win,8,1, "Enter 'y' if you want to try again.\n");
				wrefresh(win);
				mvwprintw(win,9,1, "Enter 'n' if you want to create a new account: ");
				wrefresh(win);

				yn = getch();

				if(yn == 'y' || yn == 'Y')
				{
					proceed = 1;
					continue;
				}else
				{
					//check if file exists already
					//if not create one
					//just append
					std::ofstream outfile;

					//open file
					outfile.open(user_list_filename, std::ios::app);

					//write to file
					//add user id and pw to file with a space in between
					std::stringstream ss;
					ss << user_id << ' ' << user_pw << '\n';

					outfile << ss.str();
					//close file;
					outfile.close();

					chat_client_user_id = user_id;
					chat_client_pw = user_pw;
					proceed = 0;
				}
			}

		}
		chat_client_user_id = user_id;
		chat_client_pw = user_pw;
		noecho();
		delwin(win);
	}


private:

  void do_connect(const tcp::resolver::results_type& endpoints)
  {
    asio::async_connect(socket_, endpoints,
        [this](std::error_code ec, tcp::endpoint)
        {
          if (!ec)
          {
            do_read_header();
          }
        });
  }

  void do_read_header()
  {
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), chat_message::header_length),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_read_body()
  {
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "\n";
            do_read_header();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write()
  {
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

private:
  asio::io_context& io_context_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;

  //private variables for login screen
  std::string chat_client_name;
  std::string chat_client_user_id;
  std::string chat_client_pw;

};
/*
class chatroom
{
	private:
		int maxUsers = 10;
		int maxUsersOnline = 5;
		std::string chatroomName;
		std::string admin;

	public:
	void renewAdmin();
	void addUser();
	void removeUser();
	void deleteChatroom();
	void renameChatroom();
	std::string displayAdmin();
	void countUsers();
	int getMaxUsers();
	int getMaxUsersOnline();
	std::string getChatroomName();


};
*/


int main(int argc, char* argv[])
{
	initscr();
	cbreak();
	noecho();

	WINDOW* roomBox;
	WINDOW* displayBox;
	WINDOW* msgBox;

  try
  {
	//need to provide 3 arguments
    if (argc != 3)
    {
      std::cerr << "Usage: chat_client <host> <port>\n";
      return 1;
    }

	//initialize the screen

	//y-> no. of rows
	//x-> no. of columns
	int height, width;
	height = 44;
	width = 85;
	//getmaxyx(stdscr,height,width);


    asio::io_context io_context;

    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);
    chat_client c(io_context, endpoints);

	c.login(); // enter login procedure


	roomBox = create_newwin(height-1, width/4, 1,1);
	displayBox = create_newwin(height - height/4 -1, width - width/4 -1, 1, width/4 +1);
	msgBox = create_newwin(height/4-1, width - width/4 -1, height - height/4 +1, width/4 +1);
	refresh();


	box(roomBox, int('|'), int('-'));
	wrefresh(roomBox);

	box(msgBox,int('|'), int('n'));
	wrefresh(msgBox);

	box(displayBox,int('|'), int('c'));
	wrefresh(displayBox);


	endwin();
    std::thread t([&io_context](){ io_context.run(); });



    char line[chat_message::max_body_length + 1];
	echo();
    while (1)
    {
		wgetnstr(msgBox,line, chat_message::max_body_length + 1);
	  wrefresh(msgBox);
      chat_message msg;
      msg.body_length(std::strlen(line));
      std::memcpy(msg.body(), line, msg.body_length());
      msg.encode_header();
      c.write(msg);
    }
	noecho();

    c.close(); //close chat_client
    t.join(); //join thread
	getch();
	destroy_win(displayBox);
	destroy_win(msgBox);
	destroy_win(roomBox);
  }
  catch (std::exception& e)
  {
	std::stringstream ess;
    ess << "Exception: " << e.what() << "\n";
	wprintw(msgBox, (ess.str()).c_str());
	wrefresh(msgBox);

	destroy_win(displayBox);
	destroy_win(msgBox);
	destroy_win(roomBox);
  }

	endwin();

  return 0;
}
