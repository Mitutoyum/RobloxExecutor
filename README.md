
# Roblox Executor

This is an executor base that I made to learn more about reverse engineering as also for people that wants a working base to work with. It works by injecting luau bytecode into a modulescript which then you spoof it with PlayerListManager so it gets required whenever you press escape.

It is detected so don't use it on your main account\
currently working for `version-d34359a5577645e2` | `10/11/2025`
## Installation

Simply clone this repository and load it into Visual Studio to build
    
## Acknowledgements

This is made for educational purposes, I am not responsible for anything that happens to your account
## Usage

This executor uses a REPL instead of a GUI\
There are 3 commands:
- `inject [PID]`
- `select <file_path>` file_path = the file that will get executed
- `execute [PID]`

## Roadmap

- Improve the REPL

## Finding/Updating Offsets
Offset Providers:
- [imtheo.lol](https://imtheo.lol/Offsets/Offsets.hpp)
- [offsets.ntgetwritewatch.workers.dev](https://offsets.ntgetwritewatch.workers.dev/offsets.hpp)

Research servers:
- https://discord.gg/rPc56a3J
- https://discord.gg/r4Jtc5FN

## 🚀 About Me
Discord: `mitutoyum`