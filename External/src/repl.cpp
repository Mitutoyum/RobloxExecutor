#include <iostream>
#include <string>
#include <sstream>
#include <Windows.h>
#include <memory>
#include <fstream>
#include <filesystem>

#include "executor/repl.h"
#include "executor/client.h"
#include "executor/utils.h"
#include "executor/process.h"
#include "executor/websocket.h"


REPL::REPL(Websocket& server) : _server(server) {}


void REPL::Run() {
	std::string input;
	std::string path;

	std::cout << "Executor\'s REPL, type exit to quit\n";

	while (input != "exit") {
		std::cout << "\033[2K\r";
		std::cout << "> ";
		std::getline(std::cin, input);
		std::istringstream iss(input);

		std::string command;
		iss >> command;

		if (command == "inject") {
			DWORD PID;

			auto initialize_client = [&](DWORD _PID) {
				try {
					auto client = std::make_unique<Client>(_PID, _server);
					Result result = client->Initialize();
					if (!result.success) {
						REPLPrint("[!] Failed to initialize client " + PID);
						REPLPrint(result.message);
						return;
					}
					_server.AddClient(std::move(client));
				}
				catch (const std::exception& e) {
					REPLPrint("[!]" + std::string(e.what()));
				}
			};

			if (iss >> PID) {
				initialize_client(PID);
			}
			else { // initialize all
				std::vector<DWORD> PIDs = GetProcessIds(L"RobloxPlayerBeta.exe");
				if (PIDs.empty()) {
					REPLPrint("[!] Could not find any roblox process");
					continue;
				}

				for (auto const& _PID : PIDs) {
					initialize_client(_PID);
				}
			}
		}
		else if (command == "select") {
			if (iss >> path) {
				std::filesystem::path fs_path(path);

				if (!std::filesystem::exists(fs_path) && !std::filesystem::is_regular_file(fs_path)) {
					REPLPrint("[!] Invalid path");
					continue;
				}
			}
			else {
				REPLPrint("[!] No path was specified");
			}
		}
		else if (command == "execute") {
			std::ifstream file(path);

			if (!file.is_open()) {
				REPLPrint("[!] Failed to open file");
				continue;

			};


			std::stringstream buffer;
			buffer << file.rdbuf();

			DWORD PID;

			if (iss >> PID) {
				for (const auto& client : _server.GetClients()) {
					if (client->GetProcessId() == PID) {
						Result execute_result = client->Execute(buffer.str());

						if (!execute_result.success) {
							REPLPrint(execute_result.message);
							continue;
						}
						REPLPrint("[*] Executed successfully for client" + PID);
					}
				}
			}
			else {
				for (const auto& client : _server.GetClients()) {
					//if (client->GetProcessId() == PID) {
						Result execute_result = client->Execute(buffer.str());

						if (!execute_result.success) {
							REPLPrint(execute_result.message);
							continue;
						}
						REPLPrint("[*] Executed successfully for client " + std::to_string(client->GetProcessId()));
					//}
				}
			}
		}
	}

	std::cout << "\033[2K\r";
}