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
					client->Inject();
					_server.AddClient(std::move(client));
					REPLPrint("[*] Injected client " + std::to_string(_PID) + " successfully");

				}
				catch (const std::exception& exception) {
					REPLPrint("[!] Failed to initialize client " + std::to_string(_PID));
					REPLPrint("[!] " + std::string(exception.what()));
				}
			};

			if (iss >> PID) {
				initialize_client(PID);
			}
			else { // inject all
				std::vector<DWORD> PIDs = GetProcessIds(L"RobloxPlayerBeta.exe");
				if (PIDs.empty()) {
					REPLPrint("[!] Could not find any roblox process");
					continue;
				}

				for (const auto& _PID : PIDs) {
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

				REPLPrint("[*] " + fs_path.filename().string() + " has been selected");

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
						try {
							client->Execute(buffer.str());
							REPLPrint("[*] Executed successfully for client " + PID);
						}
						catch (const std::exception& exception) {
							REPLPrint(exception.what());
						}
					}
				}
			}
			else {
				for (const auto& client : _server.GetClients()) {
					try {
						client->Execute(buffer.str());
						REPLPrint("[*] Executed successfully for client " + std::to_string(client->GetProcessId()));
					}
					catch (const std::exception& exception) {
						REPLPrint("[!] " + std::string(exception.what()));
					}
				}
			}
		}
	}

	std::cout << "\033[2K\r";
}