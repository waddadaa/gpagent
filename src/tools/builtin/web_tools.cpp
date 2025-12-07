#include "gpagent/tools/tool_registry.hpp"
#include "gpagent/core/config.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

namespace gpagent::tools::builtin {

// Simple HTML to text converter using string operations (avoid regex for stability)
std::string html_to_text(const std::string& html) {
    std::string text;
    text.reserve(html.size());

    bool in_tag = false;
    bool in_script = false;
    bool in_style = false;

    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];

        if (c == '<') {
            in_tag = true;
            // Check for script/style tags
            if (i + 7 < html.size()) {
                std::string tag = html.substr(i, 7);
                if (tag == "<script" || tag == "<SCRIPT") in_script = true;
                if (i + 6 < html.size() && (html.substr(i, 6) == "<style" || html.substr(i, 6) == "<STYLE")) in_style = true;
            }
            // Check for closing script/style
            if (i + 9 < html.size() && (html.substr(i, 9) == "</script>" || html.substr(i, 9) == "</SCRIPT>")) in_script = false;
            if (i + 8 < html.size() && (html.substr(i, 8) == "</style>" || html.substr(i, 8) == "</STYLE>")) in_style = false;

            // Add newlines for block elements
            if (i + 3 < html.size()) {
                std::string tag3 = html.substr(i, 3);
                if (tag3 == "<br" || tag3 == "<BR" || tag3 == "<p>" || tag3 == "<P>" ||
                    tag3 == "<di" || tag3 == "<DI" || tag3 == "<li" || tag3 == "<LI") {
                    text += '\n';
                }
            }
            continue;
        }

        if (c == '>') {
            in_tag = false;
            continue;
        }

        // Skip content inside tags, scripts, and styles
        if (in_tag || in_script || in_style) continue;

        // Handle HTML entities
        if (c == '&' && i + 1 < html.size()) {
            if (html.substr(i, 6) == "&nbsp;") { text += ' '; i += 5; continue; }
            if (html.substr(i, 5) == "&amp;") { text += '&'; i += 4; continue; }
            if (html.substr(i, 4) == "&lt;") { text += '<'; i += 3; continue; }
            if (html.substr(i, 4) == "&gt;") { text += '>'; i += 3; continue; }
            if (html.substr(i, 6) == "&quot;") { text += '"'; i += 5; continue; }
            if (html.substr(i, 5) == "&#39;") { text += '\''; i += 4; continue; }
        }

        text += c;
    }

    // Collapse whitespace
    std::string result;
    result.reserve(text.size());
    bool last_was_space = false;
    bool last_was_newline = false;

    for (char c : text) {
        if (c == '\n' || c == '\r') {
            if (!last_was_newline) {
                result += '\n';
                last_was_newline = true;
            }
            last_was_space = false;
        } else if (c == ' ' || c == '\t') {
            if (!last_was_space && !last_was_newline) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
            last_was_newline = false;
        }
    }

    // Trim
    size_t start = result.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t\n\r");
    return result.substr(start, end - start + 1);
}

// URL encode a string
std::string url_encode(const std::string& str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else if (c == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return encoded.str();
}

// Parse URL into components
struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string path;
    int port = 443;
    bool valid = false;
};

ParsedUrl parse_url(const std::string& url) {
    ParsedUrl result;

    std::regex url_regex(R"(^(https?):\/\/([^\/:\s]+)(?::(\d+))?(\/.*)?$)", std::regex::icase);
    std::smatch match;

    if (std::regex_match(url, match, url_regex)) {
        result.scheme = match[1].str();
        result.host = match[2].str();
        if (match[3].matched) {
            result.port = std::stoi(match[3].str());
        } else {
            result.port = (result.scheme == "https") ? 443 : 80;
        }
        result.path = match[4].matched ? match[4].str() : "/";
        result.valid = true;
    }

    return result;
}

// Search result structure
struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

// Format search results as string
std::string format_results(const std::vector<SearchResult>& results) {
    std::ostringstream output;

    if (results.empty()) {
        return "No results found.";
    }

    for (size_t i = 0; i < results.size(); ++i) {
        output << "### " << (i + 1) << ". " << results[i].title << "\n";
        output << results[i].url << "\n";
        if (!results[i].snippet.empty()) {
            output << results[i].snippet << "\n";
        }
        output << "\n";
    }

    return output.str();
}

// =============================================================================
// Google Custom Search API
// =============================================================================
std::vector<SearchResult> search_google(const std::string& query, int num_results,
                                         const std::string& api_key, const std::string& cx) {
    std::vector<SearchResult> results;

    if (api_key.empty() || cx.empty()) {
        spdlog::error("Google search requires api_key and cx (Search Engine ID)");
        return results;
    }

    try {
        httplib::Client client("https://www.googleapis.com");
        client.set_read_timeout(30);
        client.set_connection_timeout(10);

        std::string path = "/customsearch/v1?key=" + url_encode(api_key) +
                           "&cx=" + url_encode(cx) +
                           "&q=" + url_encode(query) +
                           "&num=" + std::to_string(std::min(num_results, 10));

        auto res = client.Get(path);

        if (!res || res->status != 200) {
            spdlog::error("Google search failed: {}", res ? res->body : "connection error");
            return results;
        }

        Json response = Json::parse(res->body);

        if (response.contains("items")) {
            for (const auto& item : response["items"]) {
                SearchResult sr;
                sr.title = item.value("title", "");
                sr.url = item.value("link", "");
                sr.snippet = item.value("snippet", "");

                if (!sr.url.empty()) {
                    results.push_back(sr);
                }
            }
        }

        spdlog::info("Google search found {} results", results.size());

    } catch (const std::exception& e) {
        spdlog::error("Google search exception: {}", e.what());
    }

    return results;
}

// =============================================================================
// Tavily API
// =============================================================================
std::vector<SearchResult> search_tavily(const std::string& query, int num_results,
                                         const std::string& api_key) {
    std::vector<SearchResult> results;

    if (api_key.empty()) {
        spdlog::error("Tavily search requires API key");
        return results;
    }

    try {
        httplib::Client client("https://api.tavily.com");
        client.set_read_timeout(30);
        client.set_connection_timeout(10);

        Json body;
        body["api_key"] = api_key;
        body["query"] = query;
        body["max_results"] = num_results;
        body["include_answer"] = false;
        body["include_raw_content"] = false;

        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };

        auto res = client.Post("/search", headers, body.dump(), "application/json");

        if (!res || res->status != 200) {
            spdlog::error("Tavily search failed: {}", res ? res->body : "connection error");
            return results;
        }

        Json response = Json::parse(res->body);

        if (response.contains("results")) {
            for (const auto& item : response["results"]) {
                SearchResult sr;
                sr.title = item.value("title", "");
                sr.url = item.value("url", "");
                sr.snippet = item.value("content", "");

                // Truncate long snippets
                if (sr.snippet.length() > 300) {
                    sr.snippet = sr.snippet.substr(0, 300) + "...";
                }

                if (!sr.url.empty()) {
                    results.push_back(sr);
                }
            }
        }

        spdlog::info("Tavily search found {} results", results.size());

    } catch (const std::exception& e) {
        spdlog::error("Tavily search exception: {}", e.what());
    }

    return results;
}

// =============================================================================
// Perplexity API (uses their Sonar model for search with citations)
// =============================================================================
std::vector<SearchResult> search_perplexity(const std::string& query, int num_results,
                                             const std::string& api_key) {
    std::vector<SearchResult> results;

    if (api_key.empty()) {
        spdlog::error("Perplexity search requires API key");
        return results;
    }

    try {
        httplib::Client client("https://api.perplexity.ai");
        client.set_read_timeout(60);  // Perplexity can be slow
        client.set_connection_timeout(10);

        // Perplexity uses a chat-completion style API with built-in search
        // The sonar model automatically searches and returns citations
        Json body;
        body["model"] = "sonar";  // sonar model has online search with citations
        body["messages"] = Json::array();
        body["messages"].push_back({
            {"role", "user"},
            {"content", query}
        });

        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + api_key}
        };

        auto res = client.Post("/chat/completions", headers, body.dump(), "application/json");

        if (!res || res->status != 200) {
            spdlog::error("Perplexity search failed: {}", res ? res->body : "connection error");
            return results;
        }

        Json response = Json::parse(res->body);

        // Get the response content (summary)
        std::string content;
        if (response.contains("choices") && !response["choices"].empty()) {
            content = response["choices"][0]["message"]["content"].get<std::string>();
        }

        // Extract citations from the response - Perplexity returns these in a citations array
        if (response.contains("citations") && response["citations"].is_array()) {
            int count = 0;
            for (const auto& citation : response["citations"]) {
                if (count >= num_results) break;

                SearchResult sr;
                sr.url = citation.get<std::string>();
                // Extract domain as title since citations are just URLs
                size_t start = sr.url.find("://");
                if (start != std::string::npos && start + 3 < sr.url.length()) {
                    start += 3;
                    size_t end = sr.url.find("/", start);
                    if (end == std::string::npos) {
                        end = sr.url.length();
                    }
                    sr.title = sr.url.substr(start, end - start);
                } else {
                    sr.title = sr.url;
                }
                sr.snippet = "";  // Perplexity citations are just URLs

                if (!sr.url.empty()) {
                    results.push_back(sr);
                    ++count;
                }
            }
        }

        // If we got a response but no citations, return the content as context
        if (results.empty() && !content.empty()) {
            SearchResult sr;
            sr.title = "Perplexity Search Summary";
            sr.url = "";
            sr.snippet = content;
            if (sr.snippet.length() > 500) {
                sr.snippet = sr.snippet.substr(0, 500) + "...";
            }
            results.push_back(sr);
        }

        spdlog::info("Perplexity search found {} results", results.size());

    } catch (const std::exception& e) {
        spdlog::error("Perplexity search exception: {}", e.what());
    }

    return results;
}

// =============================================================================
// Web Fetch Handler
// =============================================================================
ToolResult web_fetch_handler(const Json& args, const ToolContext& ctx) {
    (void)ctx;

    std::string url = args.at("url").get<std::string>();
    bool raw_html = args.value("raw", false);
    int max_length = args.value("max_length", 50000);

    auto parsed = parse_url(url);
    if (!parsed.valid) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Invalid URL: " + url
        };
    }

    try {
        std::unique_ptr<httplib::Client> client;

        if (parsed.scheme == "https") {
            client = std::make_unique<httplib::Client>(
                "https://" + parsed.host + ":" + std::to_string(parsed.port));
        } else {
            client = std::make_unique<httplib::Client>(
                "http://" + parsed.host + ":" + std::to_string(parsed.port));
        }

        client->set_follow_location(true);
        client->set_read_timeout(30);
        client->set_connection_timeout(10);

        // Set a browser-like user agent
        httplib::Headers headers = {
            {"User-Agent", "Mozilla/5.0 (compatible; GPAgent/1.0)"},
            {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"}
        };

        auto res = client->Get(parsed.path, headers);

        if (!res) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to fetch URL: connection error"
            };
        }

        if (res->status >= 400) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "HTTP error: " + std::to_string(res->status)
            };
        }

        std::string content = res->body;

        // Convert HTML to text unless raw requested
        if (!raw_html && content.find("<html") != std::string::npos) {
            content = html_to_text(content);
        }

        // Truncate if too long
        if (static_cast<int>(content.length()) > max_length) {
            content = content.substr(0, max_length) + "\n\n... [truncated]";
        }

        return ToolResult{
            .success = true,
            .content = content
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error fetching URL: ") + e.what()
        };
    }
}

// =============================================================================
// Web Search Handler - Uses configured provider
// =============================================================================
ToolResult web_search_handler(const Json& args, const ToolContext& ctx) {
    std::string query = args.at("query").get<std::string>();
    int num_results = args.value("num_results", 5);

    // Get search configuration
    std::string provider = "perplexity";  // Default
    std::string tavily_key, perplexity_key, google_key, google_cx;

    if (ctx.config) {
        provider = ctx.config->search.provider;
        tavily_key = ctx.config->api_keys.tavily;
        perplexity_key = ctx.config->api_keys.perplexity;
        google_key = ctx.config->api_keys.google_search;
        google_cx = ctx.config->api_keys.google_cx;

        if (ctx.config->search.max_results > 0) {
            num_results = std::min(num_results, ctx.config->search.max_results);
        }
    }

    spdlog::info("Web search using provider: {} for query: {}", provider, query);

    std::vector<SearchResult> results;

    // Select provider and execute search
    if (provider == "tavily") {
        if (tavily_key.empty()) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Tavily API key not configured. Set TAVILY_API_KEY in Settings or environment."
            };
        }
        results = search_tavily(query, num_results, tavily_key);

    } else if (provider == "google") {
        if (google_key.empty() || google_cx.empty()) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Google Search API key or CX not configured. Set GOOGLE_SEARCH_API_KEY and GOOGLE_CX in Settings or environment."
            };
        }
        results = search_google(query, num_results, google_key, google_cx);

    } else {
        // Default: Perplexity
        if (perplexity_key.empty()) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Perplexity API key not configured. Set PERPLEXITY_API_KEY in Settings or environment."
            };
        }
        results = search_perplexity(query, num_results, perplexity_key);
    }

    if (results.empty()) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "No search results found for: " + query
        };
    }

    return ToolResult{
        .success = true,
        .content = format_results(results)
    };
}

// Register web tools
void register_web_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "web_fetch",
            .description = "Fetch and read a web page. Returns text content extracted from HTML.",
            .parameters = {
                {"url", "The URL to fetch (must start with http:// or https://)", ParamType::String, true},
                {"raw", "Return raw HTML instead of extracted text (default: false)", ParamType::Boolean, false},
                {"max_length", "Maximum content length to return (default: 50000)", ParamType::Integer, false}
            },
            .keywords = {"web", "fetch", "url", "http", "page", "download", "read"}
        },
        web_fetch_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "web_search",
            .description = "Search the web for information. Supports Perplexity (default), Google Custom Search, and Tavily providers. Requires API key configuration.",
            .parameters = {
                {"query", "The search query", ParamType::String, true},
                {"num_results", "Number of results to return (default: 5)", ParamType::Integer, false}
            },
            .keywords = {"search", "web", "google", "find", "query", "internet", "perplexity", "tavily"}
        },
        web_search_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
