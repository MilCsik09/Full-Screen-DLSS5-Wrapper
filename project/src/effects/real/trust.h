#pragma once
#include "effects/real/com.h"
#include "infrastructure/bounded_string.h"
#include "interior/units.h"

namespace real {

// What a file's version resource calls its product, which is the one thing about a file its name does not say.
using ProductName = infra::BoundedString<wchar_t, 64>;

// A loadable file held open so it stays the same file for the life of the session. Most callers require
// NVIDIA signature verification; the explicit modified-neural-model path deliberately does not.
struct HeldFile
{
    UniqueHandle handle;
    ProductName product; // what the file calls its product, or nothing when it says: read once the file is held, so it is this file's
};

// Which NVIDIA-related file is being handled. Verified paths use this to name a refusal; the explicit
// modified-neural-model path is restricted to NeuralRendering by its dedicated function.
enum class ModelKind : std::uint8_t { NeuralRendering, SuperResolution, OpticalFlow, Runtime };

// Verifies every Authenticode signature the file carries, the first and each one after it, and fails
// unless each is trusted and one of their signers is NVIDIA Corporation by name, with a chain that, built
// again from Microsoft's own trusted root list and the certificates the signature carries, is clean: a root
// anyone put into the ordinary Windows stores does not count. Only then is the product name read, which is
// for the caller to judge. Revocation is not chased, which would mean a network call on a path that has to
// work offline; a root on Microsoft's list that the machine does not hold yet is fetched for that chain,
// which is the one call over the network this check can make, and it is bounded.
[[nodiscard]] infra::Result<HeldFile, Error> OpenTrusted(const interior::FilePath& path, ModelKind kind) noexcept;

// Opens only the neural-rendering model without Authenticode verification. The caller must expose this as
// an explicit opt-in compatibility mode; no other NVIDIA library may use this path.
[[nodiscard]] infra::Result<HeldFile, Error> OpenModifiedNeuralModel(const interior::FilePath& path) noexcept;

// Asks Windows, for every library the process loads by name from here on, its own and those loaded inside
// the libraries it uses, to take the one in the system folder whenever one of that name is there, so a
// file put beside the program cannot stand in for it. The executable's own imports are resolved before
// main runs; the linker's dependent load flag confines those to the system folder.
[[nodiscard]] infra::Status<Error> PreferSystemLibraries() noexcept;

} // namespace real
