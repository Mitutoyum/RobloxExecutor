#pragma once

#include <cstdint>

namespace Offsets {

    namespace FakeDataModel {
        inline constexpr uintptr_t Pointer = 0x71cbe48;
        inline constexpr uintptr_t RealDataModel = 0x1c0;
    }

    namespace DataModel {
        inline constexpr uintptr_t GameLoaded = 0x688;
    }

    namespace Instance {
        inline constexpr uintptr_t ChildrenEnd = 0x8;
        inline constexpr uintptr_t ChildrenStart = 0x68;
        inline constexpr uintptr_t ClassDescriptor = 0x18;
        inline constexpr uintptr_t ClassName = 0x8;
        inline constexpr uintptr_t Name = 0x88;
        inline constexpr uintptr_t Parent = 0x58;
    }

    namespace Player {
        inline constexpr uintptr_t LocalPlayer = 0x128;
    }

	namespace ModuleScript {
		inline constexpr uintptr_t ByteCode = 0x158;
        inline constexpr uintptr_t IsCoreScript = 0x190;
        inline constexpr uintptr_t ModuleFlags = (IsCoreScript - 0x4);
	}

	namespace LocalScript {
        inline constexpr uintptr_t ByteCode = 0x1b0;
    }

    namespace ByteCode {
        inline constexpr uintptr_t Pointer = 0x10;
        inline constexpr uintptr_t Size = 0x20;
    }

    namespace FFlags {
        inline constexpr uintptr_t WebSocketServiceEnableClientCreation = 0x607cf20;
    }
}