#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include "Node/Node.h"
#include "Flow/Flow.h"
#include "BaseNode/ConditionalTransition.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace json = boost::json;
using tcp = boost::asio::ip::tcp;
using any = std::any;
namespace bp = boost::process;
// Utility functions (you'll need to implement these based on your requirements)
std::string call_llm(const std::string& prompt, const std::string& api_key, const std::string& model, const std::string& api_host) {
    try {
        // Asio I/O context for event-driven networking
        net::io_context ioc;

        // SSL/TLS setup with better configuration
        ssl::context ctx{ssl::context::tls_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_options(ssl::context::default_workarounds |
                       ssl::context::no_sslv2 |
                       ssl::context::no_sslv3);

        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(api_host, "443");

        // Create SSL stream with timeout
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        // Set timeout for connection
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

        // Set SNI Hostname
        if(!SSL_set_tlsext_host_name(stream.native_handle(), api_host.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream).connect(results);

        // Perform the SSL handshake with timeout
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
        stream.handshake(ssl::stream_base::client);

        // Create JSON payload for Gemini API
        json::object request_body;
        json::array contents;
        json::object content;
        json::array parts;
        json::object part;
        part["text"] = prompt;
        parts.push_back(part);
        content["parts"] = parts;
        contents.push_back(content);
        request_body["contents"] = contents;

        std::string json_payload = json::serialize(request_body);

        // Set up an HTTP POST request message
        http::request<http::string_body> req{http::verb::post, "/v1beta/models/" + model + ":generateContent?key=" + api_key, 11};
        req.set(http::field::host, api_host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::connection, "close"); // Add this to prevent keep-alive issues
        req.body() = json_payload;
        req.prepare_payload();

        // Set timeout for write operation
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
        http::write(stream, req);

        // Declare a container to hold the response
        beast::flat_buffer buffer;
        http::response<http::string_body> res;

        // Set timeout for read operation
        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
        http::read(stream, buffer, res);

        // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        if(ec == net::error::eof || ec == ssl::error::stream_truncated) {
            // These are expected when server closes connection
            ec = {};
        }
        if(ec && ec != beast::errc::not_connected) {
            std::cerr << "Warning: SSL shutdown error: " << ec.message() << std::endl;
            // Don't throw here, we might still have valid response
        }

        // Check HTTP status
        if(res.result() != http::status::ok) {
            std::cerr << "HTTP Error: " << res.result_int() << " " << res.reason() << std::endl;
            return "Error: HTTP " + std::to_string(res.result_int());
        }

        // Parse the JSON response
        try {
            if (json::value response_json = json::parse(res.body()); response_json.is_object() && response_json.as_object().contains("candidates")) {
                auto candidates = response_json.at("candidates").as_array();
                if (!candidates.empty()) {
                    auto candidate = candidates[0].as_object();
                    if (candidate.contains("content")) {
                        auto key_value_pairs = candidate.at("content").as_object();
                        if (key_value_pairs.contains("parts")) {
                            auto values = key_value_pairs.at("parts").as_array();
                            if (!values.empty()) {
                                auto value = values[0].as_object();
                                if (value.contains("text")) {
                                    return value.at("text").as_string().c_str();
                                }
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& parse_error) {
            std::cerr << "JSON parsing error: " << parse_error.what() << std::endl;
            std::cerr << "Response body: " << res.body() << std::endl;
            return "Error: Failed to parse API response";
        }

        return "No response generated";

    } catch (const beast::system_error& e) {
        std::cerr << "Error calling LLM: " << e.what() << std::endl;
        return "Error: Network connection failed";
    } catch (const std::exception& e) {
        std::cerr << "Error calling LLM: " << e.what() << std::endl;
        return "Error: Failed to call LLM API";
    }
}

std::string search_web_duckduckgo(const std::string& query) {
    std::cout << "Searching web for: " << query << std::endl;

    try {
        std::string output;
        std::string error_output;
        bp::ipstream out_stream;
        bp::ipstream err_stream;

        // Create the command with proper argument handling
        // ddgs text -q "query"
        bp::child c(
            bp::search_path("ddgs"),  // Use search_path to find ddgs in PATH
            "text",
            "-q", query,              // Separate -q and query as individual arguments
            bp::std_out > out_stream,
            bp::std_err > err_stream,
            bp::std_in.close()        // Close stdin to prevent hanging
        );

        // Read output with timeout
        bool finished = c.wait_for(std::chrono::seconds(30));

        if (!finished) {
            std::cerr << "Search timeout - terminating process" << std::endl;
            c.terminate();
            c.wait();  // Wait for termination to complete
            return "Search timeout - command took too long";
        }

        // Read stdout
        std::stringstream ss;
        std::string line;
        while (std::getline(out_stream, line)) {
            ss << line << "\n";
        }
        output = ss.str();

        // Read stderr for error information
        std::stringstream err_ss;
        while (std::getline(err_stream, line)) {
            err_ss << line << "\n";
        }
        error_output = err_ss.str();

        // Check exit code
        int exit_code = c.exit_code();
        if (exit_code != 0) {
            std::cerr << "ddgs command failed with exit code: " << exit_code << std::endl;
            if (!error_output.empty()) {
                std::cerr << "Error output: " << error_output << std::endl;
            }
            return "Search command failed with exit code: " + std::to_string(exit_code) +
                   (error_output.empty() ? "" : "\nError: " + error_output);
        }

        if (output.empty()) {
            return "No search results found for query: " + query;
        }

        // Trim excessive whitespace and limit output size
        if (output.length() > 2000) {
            output = output.substr(0, 2000) + "... [truncated]";
        }

        return output;

    } catch (const bp::process_error& e) {
        std::cerr << "Process error: " << e.what() << std::endl;
        return "Error: Process execution failed - " + std::string(e.what());
    } catch (const std::exception& e) {
        std::cerr << "Search error: " << e.what() << std::endl;
        return "Error: Search failed - " + std::string(e.what());
    }
}



// Simple YAML parser for demo (you might want to use a proper YAML library)
std::map<std::string, std::string> parse_simple_yaml(const std::string& yaml_str) {
    std::map<std::string, std::string> result;
    // This is a very basic parser - consider using a proper YAML library like yaml-cpp
    // For demo purposes, we'll handle basic key-value pairs

    size_t action_pos = yaml_str.find("action:");
    if (action_pos != std::string::npos) {
        size_t start = action_pos + 7; // length of "action:"
        size_t end = yaml_str.find('\n', start);
        if (end == std::string::npos) end = yaml_str.length();
        std::string action = yaml_str.substr(start, end - start);
        // Trim whitespace
        action.erase(0, action.find_first_not_of(" \t\r\n"));
        action.erase(action.find_last_not_of(" \t\r\n") + 1);
        result["action"] = action;
    }

    size_t query_pos = yaml_str.find("search_query:");
    if (query_pos != std::string::npos) {
        size_t start = query_pos + 13; // length of "search_query:"
        size_t end = yaml_str.find('\n', start);
        if (end == std::string::npos) end = yaml_str.length();
        std::string query = yaml_str.substr(start, end - start);
        // Trim whitespace
        query.erase(0, query.find_first_not_of(" \t\r\n"));
        query.erase(query.find_last_not_of(" \t\r\n") + 1);
        result["search_query"] = query;
    }

    return result;
}

class DecideAction final : public Node {
public:
    std::any Prep(any& shared) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);

        // Get the current context (default to "No previous search" if none exists)
        std::string context = "No previous search";
        if (shared_map.find("context") != shared_map.end()) {
            context = shared_map["context"];
        }

        // Get the question from the shared store
        std::string question = shared_map["question"];

        // Return both for the exec step
        return std::make_pair(question, context);
    }

    std::any Exec(any& PrepRes) override {
        auto [question, context] = std::any_cast<std::pair<std::string, std::string>>(PrepRes);

        std::cout << "🤔 Agent deciding what to do next..." << std::endl;

        // Create a prompt to help the LLM decide what to do next
        std::string prompt = R"(
### CONTEXT
You are a research assistant that can search the web.
Question: )" + question + R"(
Previous Research: )" + context + R"(

### ACTION SPACE
[1] search
  Description: Look up more information on the web
  Parameters:
    - query (str): What to search for

[2] answer
  Description: Answer the question with current knowledge
  Parameters:
    - answer (str): Final answer to the question

## NEXT ACTION
Decide the next action based on the context and available actions.
Return your response in this format:

```yaml
thinking: |
    <your step-by-step reasoning process>
action: search OR answer
reason: <why you chose this action>
answer: <if action is answer>
search_query: <specific search query if action is search>
```
IMPORTANT: Make sure to:
1. Use proper indentation (4 spaces) for all multi-line fields
2. Use the | character for multi-line text fields
3. Keep single-line fields without the | character )";
        const char* api_key_env = std::getenv("GEMINI_API_KEY");
        if (!api_key_env) {
            std::cerr << "Error: GEMINI_API_KEY environment variable not set" << std::endl;
            // Return fallback decision
            std::map<std::string, std::string> fallback;
            fallback["action"] = "search";
            fallback["search_query"] = question;
            return fallback;
        }

        std::string api_key = api_key_env;
        std::string model = "gemini-2.0-flash";
        std::string api_host = "generativelanguage.googleapis.com";

        std::string response = call_llm(prompt, api_key, model, api_host);

 // Call the LLM to make a decision

 // Parse the response to get the decision
        size_t yaml_start = response.find("```yaml");
        size_t yaml_end = response.find("```", yaml_start + 7);
        if (yaml_start != std::string::npos && yaml_end != std::string::npos) {
            std::string yaml_str = response.substr(yaml_start + 7, yaml_end - yaml_start - 7);
            auto decision = parse_simple_yaml(yaml_str);
            return decision;
        }

        // Fallback decision
        std::map<std::string, std::string> fallback;
        fallback["action"] = "search";
        fallback["search_query"] = question;
        return fallback;
    }

    std::any Post(any& shared, any& PrepRes, any& ExecRes) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);
        auto decision = std::any_cast<std::map<std::string, std::string>>(ExecRes);

        // If LLM decided to search, save the search query
        if (decision["action"] == "search") {
            shared_map["search_query"] = decision["search_query"];
            std::cout << "🔍 Agent decided to search for: " << decision["search_query"] << std::endl;
        } else {
            if (decision.find("answer") != decision.end()) {
                shared_map["context"] = decision["answer"]; // save the context if LLM gives the answer without searching.
            }
            std::cout << "💡 Agent decided to answer the question" << std::endl;
        }

        // Update the shared state
        shared = shared_map;

        // Return the action to determine the next node in the flow
        return decision["action"];
    }
};

class SearchWeb final : public Node {
public:
    std::any Prep(any& shared) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);
        return shared_map["search_query"];
    }

    std::any Exec(any& PrepRes) override {
        const auto search_query = std::any_cast<std::string>(PrepRes);

        // Call the search utility function
        std::cout << "🌐 Searching the web for: " << search_query << std::endl;
        std::string results = search_web_duckduckgo(search_query);
        return results;
    }

    std::any Post(any& shared, any& PrepRes, any& ExecRes) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);
        const auto results = std::any_cast<std::string>(ExecRes);

        // Add the search results to the context in the shared store
        std::string previous;
        if (shared_map.contains("context")) {
            previous = shared_map["context"];
        }

        shared_map["context"] = previous + "\n\nSEARCH: " + shared_map["search_query"] + "\nRESULTS: " + results;

        std::cout << "📚 Found information, analyzing results..." << std::endl;

        // Update the shared state
        shared = shared_map;

        // Always go back to the decision node after searching
        return std::string("decide");
    }
};

class AnswerQuestion final : public Node {
public:
    std::any Prep(any& shared) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);
        std::string question = shared_map["question"];
        std::string context;
        if (shared_map.contains("context")) {
            context = shared_map["context"];
        }
        return std::make_pair(question, context);
    }

    std::any Exec(any& PrepRes) override {
        auto [question, context] = std::any_cast<std::pair<std::string, std::string>>(PrepRes);

        std::cout << "✍️ Crafting final answer..." << std::endl;

        // Create a prompt for the LLM to answer the question
        std::string prompt = R"(
### CONTEXT
Based on the following information, answer the question.
Question: )" + question + R"(
Research: )" + context + R"(

## YOUR ANSWER:
Provide a comprehensive answer using the research results.
)";

        // Call the LLM to generate an answer
        const char* api_key_env = std::getenv("GEMINI_API_KEY");
        if (!api_key_env) {
            std::cerr << "Error: GEMINI_API_KEY environment variable not set" << std::endl;
            // Return fallback decision
            std::map<std::string, std::string> fallback;
            fallback["action"] = "search";
            fallback["search_query"] = question;
            return fallback;
        }

        std::string api_key = api_key_env;
        std::string model = "gemini-2.0-flash";
        std::string api_host = "generativelanguage.googleapis.com";

        std::string response = call_llm(prompt, api_key, model, api_host);
        return response;
    }

    std::any Post(any& shared, any& PrepRes, any& ExecRes) override {
        auto shared_map = std::any_cast<std::map<std::string, std::string>>(shared);
        const auto answer = std::any_cast<std::string>(ExecRes);

        // Save the answer in the shared store
        shared_map["answer"] = answer;

        std::cout << "✅ Answer generated successfully" << std::endl;

        // Update the shared state
        shared = shared_map;

        // We're done - no need to continue the flow
        return std::string("done");
    }
};
Flow create_agent_flow() {
    /**
    Create and connect the nodes to form a complete agent flow.

    The flow works like this:
    1. DecideAction node decides whether to search or answer
    2. If search, go to SearchWeb node
    3. If answer, go to AnswerQuestion node
    4. After SearchWeb completes, go back to DecideAction

    Returns:
        Flow: A complete research agent flow
    */

    const auto decide = std::make_shared<DecideAction>();
    const auto search = std::make_shared<SearchWeb>();
    const auto answer = std::make_shared<AnswerQuestion>();


    *decide - "search" >> search;
    *decide - "answer" >> answer;
    *search - "decide" >> decide;


    return Flow{decide};
}

int main(const int argc, char* argv[]) {
    // Default question
    const std::string default_question = "Who won the Nobel Prize in Physics 2024?";
    std::string question = default_question;

    // Get question from command line if provided with --
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg.substr(0, 2) == "--" && arg.length() > 2) {
            question = arg.substr(2);
            break;
        }
    }

    try {
        // Create the agent flow
        Flow agent_flow = create_agent_flow();

        // Process the question
        std::map<std::string, std::string> shared;
        shared["question"] = question;

        std::cout << "🤔 Processing question: " << question << std::endl;

        std::any shared_any = shared;
        agent_flow.Run(shared_any);

        // Get the final answer
        auto final_shared = std::any_cast<std::map<std::string, std::string>>(shared_any);

        std::cout << "\n🎯 Final Answer:" << std::endl;
        auto answer_it = final_shared.find("answer");
        if (answer_it != final_shared.end()) {
            std::cout << answer_it->second << std::endl;
        } else {
            std::cout << "No answer found" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
