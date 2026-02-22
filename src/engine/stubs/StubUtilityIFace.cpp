// StubUtilityIFace.cpp — Compile unit for the stub interface implementations
// The actual implementations live in StubInterfaces.h (header-only for simplicity).
// This .cpp ensures the engine static library has an object file to link.

#include "CvGameCoreDLL.h"
#include "StubInterfaces.h"

// Force instantiation of the OpenCiv4 stub classes so they're available at link time.
// (They're used directly in main.cpp via #include, but having this translation unit
//  in the engine library ensures the linker sees them.)
namespace OpenCiv4 {
    // Explicit template instantiation not needed since these are regular classes,
    // but this file anchors the engine library.
    static int stub_anchor = 0;
}
