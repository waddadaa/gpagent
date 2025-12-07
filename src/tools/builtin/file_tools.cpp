#include "gpagent/tools/tool_registry.hpp"
#include "gpagent/core/config.hpp"

#include <spdlog/spdlog.h>
#include <QImage>
#include <QBuffer>
#include <QByteArray>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

#ifdef HAVE_POPPLER
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#endif

namespace gpagent::tools::builtin {

namespace fs = std::filesystem;

#ifdef HAVE_POPPLER
// Read text content from a PDF file
std::string read_pdf_content(const fs::path& path, int max_pages = 100) {
    auto doc = poppler::document::load_from_file(path.string());
    if (!doc) {
        return "";
    }

    std::ostringstream result;
    int total_pages = doc->pages();
    int pages_to_read = std::min(total_pages, max_pages);

    result << "PDF Document: " << total_pages << " pages\n";
    result << std::string(50, '-') << "\n\n";

    for (int i = 0; i < pages_to_read; ++i) {
        auto page = doc->create_page(i);
        if (!page) continue;

        result << "[Page " << (i + 1) << "]\n";

        // Get text with proper layout
        auto text = page->text().to_latin1();
        if (!text.empty()) {
            result << std::string(text.data(), text.size());
        }
        result << "\n\n";
    }

    if (pages_to_read < total_pages) {
        result << "\n... (showing " << pages_to_read << " of " << total_pages << " pages)\n";
    }

    return result.str();
}
#endif

// Check if file is a PDF based on extension
bool is_pdf_file(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".pdf";
}

// Validate path is within allowed directories
bool validate_path(const fs::path& path, const ToolContext& ctx) {
    try {
        fs::path abs = fs::weakly_canonical(path);
        std::string abs_str = abs.string();

        for (const auto& allowed : ctx.allowed_paths) {
            if (abs_str.find(allowed) == 0) {
                return true;
            }
        }

        // Also allow paths under working directory
        fs::path cwd = fs::weakly_canonical(ctx.working_directory);
        if (abs_str.find(cwd.string()) == 0) {
            return true;
        }

        return false;
    } catch (...) {
        return false;
    }
}

ToolResult file_read_handler(const Json& args, const ToolContext& ctx) {
    std::string file_path = args.at("file_path").get<std::string>();
    int offset = args.value("offset", 0);
    int limit = args.value("limit", 2000);

    fs::path path(file_path);

    // Validate path
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + file_path
        };
    }

    // Check if file exists
    if (!fs::exists(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "File not found: " + file_path
        };
    }

    // Check if it's a file
    if (!fs::is_regular_file(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a regular file: " + file_path
        };
    }

    // Check if it's a PDF file
    if (is_pdf_file(path)) {
#ifdef HAVE_POPPLER
        try {
            std::string pdf_content = read_pdf_content(path, 100);
            if (pdf_content.empty()) {
                return ToolResult{
                    .success = false,
                    .content = "",
                    .error_message = "Failed to read PDF file: " + file_path
                };
            }
            return ToolResult{
                .success = true,
                .content = pdf_content
            };
        } catch (const std::exception& e) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = std::string("Error reading PDF: ") + e.what()
            };
        }
#else
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "PDF support not available. Install poppler-cpp library."
        };
#endif
    }

    // Read text file
    try {
        std::ifstream file(path);
        if (!file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to open file: " + file_path
            };
        }

        std::ostringstream result;
        std::string line;
        int line_num = 0;
        int lines_read = 0;

        while (std::getline(file, line) && lines_read < limit) {
            if (line_num >= offset) {
                // Truncate long lines
                if (line.length() > 2000) {
                    line = line.substr(0, 2000) + "... [truncated]";
                }
                result << std::setw(6) << (line_num + 1) << "\t" << line << "\n";
                lines_read++;
            }
            line_num++;
        }

        return ToolResult{
            .success = true,
            .content = result.str()
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error reading file: ") + e.what()
        };
    }
}

ToolResult file_write_handler(const Json& args, const ToolContext& ctx) {
    std::string file_path = args.at("file_path").get<std::string>();
    std::string content = args.at("content").get<std::string>();

    fs::path path(file_path);

    // Validate path
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + file_path
        };
    }

    try {
        // Create parent directories if needed
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (!file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to open file for writing: " + file_path
            };
        }

        file << content;

        return ToolResult{
            .success = true,
            .content = "File written successfully: " + file_path
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error writing file: ") + e.what()
        };
    }
}

ToolResult file_edit_handler(const Json& args, const ToolContext& ctx) {
    std::string file_path = args.at("file_path").get<std::string>();
    std::string old_string = args.at("old_string").get<std::string>();
    std::string new_string = args.at("new_string").get<std::string>();
    bool replace_all = args.value("replace_all", false);

    fs::path path(file_path);

    // Validate path
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + file_path
        };
    }

    // Check if file exists
    if (!fs::exists(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "File not found: " + file_path
        };
    }

    try {
        // Read current content
        std::ifstream in_file(path);
        if (!in_file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to open file for reading: " + file_path
            };
        }

        std::ostringstream ss;
        ss << in_file.rdbuf();
        std::string content = ss.str();
        in_file.close();

        // Find and replace
        size_t pos = content.find(old_string);
        if (pos == std::string::npos) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "old_string not found in file. Make sure it matches exactly."
            };
        }

        int replacements = 0;
        if (replace_all) {
            while ((pos = content.find(old_string, pos)) != std::string::npos) {
                content.replace(pos, old_string.length(), new_string);
                pos += new_string.length();
                replacements++;
            }
        } else {
            content.replace(pos, old_string.length(), new_string);
            replacements = 1;
        }

        // Write back
        std::ofstream out_file(path);
        if (!out_file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to open file for writing: " + file_path
            };
        }

        out_file << content;

        return ToolResult{
            .success = true,
            .content = "Made " + std::to_string(replacements) + " replacement(s) in " + file_path
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error editing file: ") + e.what()
        };
    }
}

ToolResult glob_handler(const Json& args, const ToolContext& ctx) {
    std::string pattern = args.at("pattern").get<std::string>();
    std::string base_path = args.value("path", ctx.working_directory);

    fs::path base(base_path);

    try {
        if (!fs::exists(base)) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Base path does not exist: " + base_path
            };
        }

        std::vector<std::string> matches;

        // Simple glob implementation (supports ** and *)
        std::regex glob_regex;
        try {
            // Convert glob to regex
            std::string regex_str = pattern;

            // Escape special regex characters except * and ?
            regex_str = std::regex_replace(regex_str, std::regex(R"([\.\+\^\$\|\(\)\[\]\{\}\\])"), R"(\$&)");

            // Convert ** to match any path
            regex_str = std::regex_replace(regex_str, std::regex(R"(\*\*)"), ".*");

            // Convert * to match any filename (not /)
            regex_str = std::regex_replace(regex_str, std::regex(R"(\*)"), "[^/]*");

            // Convert ? to match single character
            regex_str = std::regex_replace(regex_str, std::regex(R"(\?)"), ".");

            glob_regex = std::regex(regex_str);
        } catch (const std::regex_error&) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Invalid glob pattern: " + pattern
            };
        }

        // Walk directory
        for (const auto& entry : fs::recursive_directory_iterator(base,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::string rel_path = fs::relative(entry.path(), base).string();
            if (std::regex_match(rel_path, glob_regex)) {
                matches.push_back(entry.path().string());
            }

            // Limit results
            if (matches.size() >= 1000) break;
        }

        // Sort by modification time (newest first)
        std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
            try {
                return fs::last_write_time(a) > fs::last_write_time(b);
            } catch (...) {
                return false;
            }
        });

        std::ostringstream result;
        for (const auto& m : matches) {
            result << m << "\n";
        }

        return ToolResult{
            .success = true,
            .content = result.str().empty() ? "No files found matching pattern" : result.str()
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error in glob: ") + e.what()
        };
    }
}

ToolResult file_delete_handler(const Json& args, const ToolContext& ctx) {
    std::string file_path = args.at("file_path").get<std::string>();
    bool recursive = args.value("recursive", false);

    fs::path path(file_path);

    // Validate path
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + file_path
        };
    }

    // Check if path exists
    if (!fs::exists(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not found: " + file_path
        };
    }

    try {
        if (fs::is_directory(path)) {
            if (!recursive) {
                return ToolResult{
                    .success = false,
                    .content = "",
                    .error_message = "Path is a directory. Use recursive=true to delete directories."
                };
            }
            auto count = fs::remove_all(path);
            return ToolResult{
                .success = true,
                .content = "Deleted directory and " + std::to_string(count - 1) + " items: " + file_path
            };
        } else {
            fs::remove(path);
            return ToolResult{
                .success = true,
                .content = "Deleted file: " + file_path
            };
        }
    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error deleting: ") + e.what()
        };
    }
}

ToolResult move_file_handler(const Json& args, const ToolContext& ctx) {
    std::string source = args.at("source").get<std::string>();
    std::string destination = args.at("destination").get<std::string>();
    bool overwrite = args.value("overwrite", false);

    fs::path src_path(source);
    fs::path dst_path(destination);

    // Validate paths
    if (ctx.sandbox_enabled) {
        if (!validate_path(src_path, ctx)) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Source path not allowed: " + source
            };
        }
        if (!validate_path(dst_path, ctx)) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Destination path not allowed: " + destination
            };
        }
    }

    // Check source exists
    if (!fs::exists(src_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Source not found: " + source
        };
    }

    // Check if destination exists
    if (fs::exists(dst_path) && !overwrite) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Destination already exists. Use overwrite=true to replace."
        };
    }

    try {
        // Create parent directories if needed
        if (dst_path.has_parent_path()) {
            fs::create_directories(dst_path.parent_path());
        }

        fs::rename(src_path, dst_path);
        return ToolResult{
            .success = true,
            .content = "Moved " + source + " to " + destination
        };
    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error moving file: ") + e.what()
        };
    }
}

ToolResult list_directory_handler(const Json& args, const ToolContext& ctx) {
    std::string dir_path = args.value("path", ctx.working_directory);
    bool show_hidden = args.value("show_hidden", false);
    bool recursive = args.value("recursive", false);
    int max_depth = args.value("max_depth", 3);

    fs::path path(dir_path);

    // Validate path
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + dir_path
        };
    }

    if (!fs::exists(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Directory not found: " + dir_path
        };
    }

    if (!fs::is_directory(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a directory: " + dir_path
        };
    }

    try {
        std::ostringstream result;
        int count = 0;
        const int max_entries = 500;

        std::function<void(const fs::path&, int)> list_dir = [&](const fs::path& p, int depth) {
            if (count >= max_entries) return;
            if (recursive && depth > max_depth) return;

            std::string indent(depth * 2, ' ');

            for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied)) {
                if (count >= max_entries) break;

                std::string name = entry.path().filename().string();

                // Skip hidden files unless requested
                if (!show_hidden && !name.empty() && name[0] == '.') {
                    continue;
                }

                if (entry.is_directory()) {
                    result << indent << "[DIR]  " << name << "/\n";
                    count++;
                    if (recursive) {
                        list_dir(entry.path(), depth + 1);
                    }
                } else if (entry.is_regular_file()) {
                    auto size = fs::file_size(entry.path());
                    std::string size_str;
                    if (size < 1024) {
                        size_str = std::to_string(size) + " B";
                    } else if (size < 1024 * 1024) {
                        size_str = std::to_string(size / 1024) + " KB";
                    } else {
                        size_str = std::to_string(size / (1024 * 1024)) + " MB";
                    }
                    result << indent << "[FILE] " << name << " (" << size_str << ")\n";
                    count++;
                } else if (entry.is_symlink()) {
                    result << indent << "[LINK] " << name << "\n";
                    count++;
                }
            }
        };

        list_dir(path, 0);

        if (count >= max_entries) {
            result << "\n... (truncated, " << max_entries << " entries shown)\n";
        }

        return ToolResult{
            .success = true,
            .content = result.str().empty() ? "Directory is empty" : result.str()
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error listing directory: ") + e.what()
        };
    }
}

ToolResult get_working_dir_handler(const Json& args, const ToolContext& ctx) {
    (void)args;  // Unused

    try {
        fs::path cwd = fs::current_path();
        return ToolResult{
            .success = true,
            .content = cwd.string()
        };
    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error getting working directory: ") + e.what()
        };
    }
}

// Base64 encoding helper
static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::vector<unsigned char>& data) {
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t data_len = data.size();
    const unsigned char* bytes_to_encode = data.data();

    while (data_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

// Check if file is an image based on extension
bool is_image_file(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
           ext == ".gif" || ext == ".webp" || ext == ".bmp";
}

// Get MIME type from extension
std::string get_image_mime_type(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".bmp") return "image/bmp";
    return "application/octet-stream";
}

// Compress and resize image to reduce latency
// Target: ~1.15 megapixels (1092x1092 for 1:1 aspect ratio)
struct CompressedImage {
    std::vector<unsigned char> data;
    std::string mime_type;
    int width;
    int height;
    bool was_resized;
};

CompressedImage compress_image(const fs::path& path, int max_dimension = 1568) {
    CompressedImage result;
    result.was_resized = false;

    QImage image(QString::fromStdString(path.string()));
    if (image.isNull()) {
        spdlog::warn("Failed to load image with Qt: {}", path.string());
        return result;
    }

    result.width = image.width();
    result.height = image.height();

    // Calculate megapixels
    double megapixels = (image.width() * image.height()) / 1000000.0;
    spdlog::info("Original image: {}x{} ({:.2f} MP)", image.width(), image.height(), megapixels);

    // Resize if larger than target (1.15 MP or long edge > 1568)
    if (megapixels > 1.15 || image.width() > max_dimension || image.height() > max_dimension) {
        // Calculate new dimensions maintaining aspect ratio
        int new_width = image.width();
        int new_height = image.height();

        if (image.width() > image.height()) {
            if (image.width() > max_dimension) {
                new_width = max_dimension;
                new_height = static_cast<int>(image.height() * (static_cast<double>(max_dimension) / image.width()));
            }
        } else {
            if (image.height() > max_dimension) {
                new_height = max_dimension;
                new_width = static_cast<int>(image.width() * (static_cast<double>(max_dimension) / image.height()));
            }
        }

        // Additional check for megapixels
        double new_megapixels = (new_width * new_height) / 1000000.0;
        if (new_megapixels > 1.15) {
            double scale = std::sqrt(1.15 / new_megapixels);
            new_width = static_cast<int>(new_width * scale);
            new_height = static_cast<int>(new_height * scale);
        }

        image = image.scaled(new_width, new_height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        result.width = new_width;
        result.height = new_height;
        result.was_resized = true;

        spdlog::info("Resized image to: {}x{} ({:.2f} MP)", new_width, new_height,
                     (new_width * new_height) / 1000000.0);
    }

    // Convert to JPEG with quality 85 for good balance of quality/size
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);

    // Use JPEG for compression (except for PNG with transparency)
    const char* format = "JPEG";
    result.mime_type = "image/jpeg";

    // Check if original was PNG and might have transparency
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if ((ext == ".png" || ext == ".gif") && image.hasAlphaChannel()) {
        format = "PNG";
        result.mime_type = "image/png";
        image.save(&buffer, format);
    } else {
        // Convert to RGB if needed (remove alpha for JPEG)
        if (image.hasAlphaChannel()) {
            image = image.convertToFormat(QImage::Format_RGB888);
        }
        image.save(&buffer, format, 85);  // 85% quality
    }

    buffer.close();

    result.data.resize(ba.size());
    std::memcpy(result.data.data(), ba.constData(), ba.size());

    spdlog::info("Compressed image size: {} bytes", result.data.size());

    return result;
}

ToolResult image_read_handler(const Json& args, const ToolContext& ctx) {
    std::string file_path = args.at("file_path").get<std::string>();
    spdlog::info("image_read_handler called for: {}", file_path);

    fs::path path(file_path);

    // Validate path
    spdlog::info("Sandbox enabled: {}, validating path...", ctx.sandbox_enabled);
    if (ctx.sandbox_enabled && !validate_path(path, ctx)) {
        spdlog::warn("Path validation failed for: {}", file_path);
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Path not allowed: " + file_path
        };
    }

    // Check if file exists
    if (!fs::exists(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "File not found: " + file_path
        };
    }

    // Check if it's an image file
    if (!is_image_file(path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a supported image file. Supported: jpg, jpeg, png, gif, webp, bmp"
        };
    }

    // Check file size (max 20MB)
    auto file_size = fs::file_size(path);
    if (file_size > 20 * 1024 * 1024) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Image file too large. Maximum size is 20MB."
        };
    }

    try {
        // Compress and resize image for optimal API performance
        auto compressed = compress_image(path);

        if (compressed.data.empty()) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to process image file: " + file_path
            };
        }

        // Encode to base64
        std::string base64_data = base64_encode(compressed.data);

        // Return as JSON with image data
        Json result = {
            {"type", "image"},
            {"media_type", compressed.mime_type},
            {"data", base64_data},
            {"file_path", file_path},
            {"original_size", file_size},
            {"compressed_size", compressed.data.size()},
            {"width", compressed.width},
            {"height", compressed.height},
            {"was_resized", compressed.was_resized}
        };

        spdlog::info("Image ready: {}x{}, {} bytes base64",
                     compressed.width, compressed.height, base64_data.size());

        return ToolResult{
            .success = true,
            .content = result.dump(),
            .is_image = true  // Flag to indicate this contains image data
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error reading image: ") + e.what()
        };
    }
}

// Register file tools with the registry
void register_file_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "file_read",
            .description = "Read the contents of a file. Supports text files (returns lines with line numbers) and PDF files (extracts text content).",
            .parameters = {
                {"file_path", "The absolute path to the file to read (supports .txt, .pdf, and other text files)", ParamType::String, true},
                {"offset", "Line number to start reading from (0-indexed, text files only)", ParamType::Integer, false},
                {"limit", "Maximum number of lines to read (default: 2000, text files only)", ParamType::Integer, false}
            },
            .keywords = {"read", "file", "content", "view", "cat", "open", "pdf"}
        },
        file_read_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "image_read",
            .description = "Read an image file and return it as base64 encoded data for visual analysis. Supports JPEG, PNG, GIF, WebP, and BMP formats.",
            .parameters = {
                {"file_path", "The absolute path to the image file", ParamType::String, true}
            },
            .keywords = {"image", "picture", "photo", "read", "view", "analyze", "vision", "screenshot"}
        },
        image_read_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "file_write",
            .description = "Write content to a file. Creates the file if it doesn't exist, overwrites if it does.",
            .parameters = {
                {"file_path", "The absolute path to the file to write", ParamType::String, true},
                {"content", "The content to write to the file", ParamType::String, true}
            },
            .keywords = {"write", "file", "create", "save", "output"},
            .requires_confirmation = true
        },
        file_write_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "file_edit",
            .description = "Edit a file by replacing exact text. The old_string must match exactly.",
            .parameters = {
                {"file_path", "The absolute path to the file to edit", ParamType::String, true},
                {"old_string", "The exact string to replace (must be unique or use replace_all)", ParamType::String, true},
                {"new_string", "The replacement string", ParamType::String, true},
                {"replace_all", "Replace all occurrences (default: false)", ParamType::Boolean, false}
            },
            .keywords = {"edit", "file", "modify", "replace", "change", "update"},
            .requires_confirmation = true
        },
        file_edit_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "glob",
            .description = "Find files matching a glob pattern. Supports ** for recursive matching.",
            .parameters = {
                {"pattern", "The glob pattern to match (e.g., **/*.cpp, src/**/*.hpp)", ParamType::String, true},
                {"path", "Base directory to search in (default: working directory)", ParamType::String, false}
            },
            .keywords = {"find", "file", "glob", "pattern", "search", "list"}
        },
        glob_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "file_delete",
            .description = "Delete a file or directory. Use recursive=true for directories.",
            .parameters = {
                {"file_path", "The absolute path to delete", ParamType::String, true},
                {"recursive", "Delete directories recursively (default: false)", ParamType::Boolean, false}
            },
            .keywords = {"delete", "remove", "rm", "file", "directory"},
            .requires_confirmation = true
        },
        file_delete_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "move_file",
            .description = "Move or rename a file or directory.",
            .parameters = {
                {"source", "The source path", ParamType::String, true},
                {"destination", "The destination path", ParamType::String, true},
                {"overwrite", "Overwrite if destination exists (default: false)", ParamType::Boolean, false}
            },
            .keywords = {"move", "rename", "mv", "file"},
            .requires_confirmation = true
        },
        move_file_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "list_directory",
            .description = "List contents of a directory with file sizes.",
            .parameters = {
                {"path", "Directory path (default: working directory)", ParamType::String, false},
                {"show_hidden", "Show hidden files (default: false)", ParamType::Boolean, false},
                {"recursive", "List recursively (default: false)", ParamType::Boolean, false},
                {"max_depth", "Max recursion depth (default: 3)", ParamType::Integer, false}
            },
            .keywords = {"list", "ls", "directory", "folder", "files"}
        },
        list_directory_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "get_working_dir",
            .description = "Get the current working directory.",
            .parameters = {},
            .keywords = {"pwd", "cwd", "directory", "path"}
        },
        get_working_dir_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
