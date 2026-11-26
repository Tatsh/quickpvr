#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <argparse/argparse.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "logging.h"
#include "png.h"
#include "pvr/pvr.h"

namespace Pvr2Png {

namespace {

constexpr int kUsageErrorStatus = 2;

struct Settings {
    std::optional<std::filesystem::path> output;
    std::optional<std::filesystem::path> palette;
    bool sheet{};
    bool info{};
    std::size_t inputCount{};
};

std::expected<std::vector<std::uint8_t>, std::string> readFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Cannot read " + path.string() + ".");
    }
    std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>()};
    if (file.bad()) {
        return std::unexpected("Cannot read " + path.string() + ".");
    }
    return data;
}

std::expected<void, std::string> writeFile(const std::filesystem::path &path,
                                           const std::vector<std::uint8_t> &data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (!file.flush()) {
        return std::unexpected("Cannot write " + path.string() + ".");
    }
    return {};
}

std::optional<std::filesystem::path> siblingPalette(const std::filesystem::path &texture) {
    for (const char *extension : {".pvp", ".PVP"}) {
        auto candidate = texture;
        candidate.replace_extension(extension);
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::expected<std::vector<Pvr::Color>, std::string> loadPalette(const std::filesystem::path &path) {
    const auto data = readFile(path);
    if (!data) {
        return std::unexpected(data.error());
    }
    auto palette = Pvr::decodePalette(*data);
    if (!palette) {
        return std::unexpected(path.string() + ": " + palette.error().message);
    }
    return std::move(*palette);
}

void printInfo(const std::filesystem::path &input, const Pvr::DecodeResult &result) {
    std::cout << input.string() << ": " << Pvr::containerName(result.container);
    if (result.container == Pvr::Container::DreamcastPack) {
        std::cout << ", " << result.images.size() << " textures";
    }
    std::cout << '\n';
    for (const auto &image : result.images) {
        std::cout << "  " << image.width << 'x' << image.height << ' ' << image.format << '\n';
    }
}

std::filesystem::path targetPath(const std::filesystem::path &directory,
                                 const std::string &stem,
                                 std::size_t index,
                                 std::size_t count) {
    if (count == 1) {
        return directory / (stem + ".png");
    }
    return directory / fmt::format("{}_{:03}.png", stem, index);
}

// Returns whether every image of the file was written.
bool convert(const std::filesystem::path &input, const Settings &settings) {
    const auto data = readFile(input);
    if (!data) {
        spdlog::error("{}", data.error());
        return false;
    }
    Pvr::Options options;
    if (const auto palettePath = settings.palette ? settings.palette : siblingPalette(input)) {
        auto palette = loadPalette(*palettePath);
        if (palette) {
            spdlog::debug("Using palette {}.", palettePath->string());
            options.palette = std::move(*palette);
        } else if (settings.palette) {
            spdlog::error("{}", palette.error());
            return false;
        } else {
            spdlog::warn("Ignoring {}. {}", palettePath->string(), palette.error());
        }
    }
    auto result = Pvr::decode(*data, options);
    if (!result) {
        spdlog::error("{}: {}", input.string(), result.error().message);
        return false;
    }
    for (const auto &failure : result->failures) {
        spdlog::error(
            "{}: texture {}: {}", input.string(), failure.position, failure.error.message);
    }
    if (settings.info) {
        printInfo(input, *result);
        return result->failures.empty();
    }

    std::vector<Pvr::Image> images;
    if (settings.sheet && result->images.size() > 1) {
        auto sheet = Pvr::contactSheet(result->images);
        if (!sheet) {
            spdlog::error("{}: {}", input.string(), sheet.error().message);
            return false;
        }
        images.push_back(std::move(*sheet));
    } else {
        images = std::move(result->images);
    }

    std::error_code error;
    const bool singleFileOutput = settings.output && settings.inputCount == 1 &&
                                  images.size() == 1 &&
                                  !std::filesystem::is_directory(*settings.output, error);
    auto directory = input.parent_path();
    if (settings.output && !singleFileOutput) {
        directory = *settings.output;
        std::filesystem::create_directories(directory, error);
        if (error) {
            spdlog::error("Cannot create {}. {}", directory.string(), error.message());
            return false;
        }
    }
    const auto stem = input.stem().string();
    bool ok = result->failures.empty();
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto target =
            singleFileOutput ? *settings.output : targetPath(directory, stem, i, images.size());
        const auto png = encodePng(images[i]);
        if (!png) {
            spdlog::error("{}", png.error());
            ok = false;
            continue;
        }
        if (const auto written = writeFile(target, *png); !written) {
            spdlog::error("{}", written.error());
            ok = false;
            continue;
        }
        spdlog::info("{} -> {} ({}x{} {})",
                     input.string(),
                     target.string(),
                     images[i].width,
                     images[i].height,
                     images[i].format);
    }
    return ok;
}

} // namespace

} // namespace Pvr2Png

int main(int argc, char *argv[]) {
    using namespace Pvr2Png;
    argparse::ArgumentParser parser("pvr2png", std::string(Pvr::version()));
    parser.add_description("Convert PowerVR and Sega Dreamcast PVR textures and texture packs to "
                           "PNG. Each FILE.pvr is written as FILE.png next to it. A pack becomes "
                           "FILE_000.png, FILE_001.png, and so on.");
    parser.add_argument("files")
        .help("Textures to convert.")
        .nargs(argparse::nargs_pattern::at_least_one);
    parser.add_argument("-o", "--output")
        .help("Where to write. With one input that yields one image, this is the PNG file. "
              "Otherwise it is a directory, created if missing.");
    parser.add_argument("-p", "--palette")
        .help("PVPL palette (.pvp) for palettized Dreamcast textures. By default a .pvp with the "
              "same name as the texture is used if present.");
    parser.add_argument("-s", "--sheet")
        .help("Write a pack as one contact sheet instead of one PNG per texture.")
        .flag();
    parser.add_argument("-i", "--info")
        .help("Print the container and the size and pixel format of each texture, and write "
              "nothing.")
        .flag();
    parser.add_argument("-q", "--quiet").help("Only log warnings and errors.").flag();
    parser.add_argument("--debug").help("Enable debug logging.").flag();
    Settings settings;
    std::vector<std::string> files;
    bool debug = false;
    bool quiet = false;
    // argparse reports usage errors only by throwing, some of them when a value is read.
    try {
        parser.parse_args(argc, argv);
        files = parser.get<std::vector<std::string>>("files");
        if (const auto output = parser.present("--output")) {
            settings.output = *output;
        }
        if (const auto palette = parser.present("--palette")) {
            settings.palette = *palette;
        }
        settings.sheet = parser.get<bool>("--sheet");
        settings.info = parser.get<bool>("--info");
        debug = parser.get<bool>("--debug");
        quiet = parser.get<bool>("--quiet");
    } catch (const std::exception &e) {
        std::cerr << parser.usage() << "\npvr2png: error: " << e.what() << '\n';
        return kUsageErrorStatus;
    }
    setupLogging(debug);
    if (quiet && !debug) {
        spdlog::set_level(spdlog::level::warn);
    }
    settings.inputCount = files.size();
    bool ok = true;
    for (const auto &file : files) {
        ok = convert(file, settings) && ok;
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
