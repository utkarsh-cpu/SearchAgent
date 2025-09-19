
# SampleAgent

A C++ research agent implementation using an agentic framework that can search the web and answer questions using AI capabilities.

## Overview

SampleAgent is a demonstration of an intelligent research agent built with C++20 that can:
- Decide autonomously whether to search for more information or provide an answer
- Search the web using DuckDuckGo for relevant information
- Generate comprehensive answers using Google's Gemini AI model
- Maintain context across multiple search iterations

## Features

- **Autonomous Decision Making**: Uses AI to decide when to search vs when to answer
- **Web Search Integration**: Searches the web using DuckDuckGo CLI tool (`ddgs`)
- **AI-Powered Responses**: Integrates with Google Gemini API for intelligent responses
- **Flow-Based Architecture**: Built on a flexible node-and-flow framework
- **Context Management**: Maintains search context across multiple iterations
- **SSL/TLS Support**: Secure HTTPS communication with AI APIs

## Architecture

The project uses an agentic framework with the following key components:

### Core Framework Classes

- **BaseNode**: Abstract base class for all processing nodes
- **Node**: Concrete implementation with Prep/Exec/Post pattern
- **Flow**: Orchestrates execution flow between nodes
- **ConditionalTransition**: Enables conditional routing between nodes

### Agent Implementation

The sample agent consists of three main nodes:

1. **DecideAction**: Analyzes the current context and decides whether to search or answer
2. **SearchWeb**: Performs web searches using DuckDuckGo
3. **AnswerQuestion**: Generates final answers using the collected information

### Flow Pattern
```
┌─────────────┐ search ┌───────────┐ decide
│ DecideAction├─────────────►│ SearchWeb ├──────────────┐ │ │ │ │ │ │ │ answer └───────────┘ ▼ │ ├─────────────────────────────────────────┘ └─────────────┘ ┌──────────────┐ │AnswerQuestion│ └──────────────┘
``` 

## Prerequisites

### Dependencies

- **C++20** compatible compiler
- **CMake** 3.15+
- **Boost Libraries**:
  - Boost.Beast (HTTP/HTTPS client)
  - Boost.JSON (JSON parsing)
  - Boost.Process (external process execution)
- **OpenSSL** (for HTTPS support)
- **DuckDuckGo CLI tool** (`ddgs`) - Install with `pip install duckduckgo-search[cli]`

### API Keys

- **Google Gemini API Key**: Set the `GEMINI_API_KEY` environment variable

## Installation

1. **Clone the repository**:
```bash 
git clone <repository-url> cd SampleAgent
``` 

2. **Install DuckDuckGo CLI**:
```bash
  pip install -U ddgs
```
1. **Set up environment variables**:
``` bash
export GEMINI_API_KEY="your-gemini-api-key-here"
```
1. **Build the project**:
``` bash
mkdir build
cd build
cmake ..
make
```
## Usage
### Basic Usage
Run the agent with a default question:
``` bash
./SampleAgent
```
### Custom Questions
Ask a specific question using the `--` prefix:
``` bash
./SampleAgent --"What are the latest developments in quantum computing?"
./SampleAgent --"Who won the 2024 Olympics marathon?"
```
### Example Output
``` 
🤔 Processing question: Who won the Nobel Prize in Physics 2024?
🤔 Agent deciding what to do next...
🔍 Agent decided to search for: Nobel Prize Physics 2024 winners
🌐 Searching the web for: Nobel Prize Physics 2024 winners
📚 Found information, analyzing results...
🤔 Agent deciding what to do next...
✍️ Crafting final answer...
✅ Answer generated successfully

🎯 Final Answer:
The 2024 Nobel Prize in Physics was awarded to John Hopfield and Geoffrey Hinton 
for their foundational discoveries and inventions that enable machine learning 
with artificial neural networks...
```
## Configuration
### Environment Variables
- `GEMINI_API_KEY`: Your Google Gemini API key (required)

### Customization
The agent behavior can be customized by modifying:
- **Prompts**: Edit the prompt templates in `DecideAction` and `AnswerQuestion` classes
- **Search Parameters**: Modify search queries and result processing in `SearchWeb`
- **Flow Logic**: Adjust the decision logic and node connections in `create_agent_flow()`

## Framework Details
### Node Execution Pattern
Each node follows a three-phase execution pattern:
1. **Prep**: Prepare data from shared state
2. **Exec**: Execute the main logic
3. **Post**: Update shared state and determine next action

### Shared State Management
The framework uses for flexible shared state management, allowing nodes to store and retrieve different data types as needed. `std::any`
### Error Handling
The implementation includes comprehensive error handling for:
- Network connectivity issues
- API failures
- Process execution errors
- JSON parsing errors

## Extending the Agent
### Adding New Nodes
1. Create a class inheriting from `Node`
2. Implement `Prep`, `Exec`, and `Post` methods
3. Add the node to the flow in `create_agent_flow()`

### Adding New Actions
1. Update the `DecideAction` node to recognize new actions
2. Create corresponding nodes for the new actions
3. Update the flow routing logic

### Example: Adding a Calculator Node
``` cpp
class Calculator : public Node {
public:
    std::any Exec(std::any& PrepRes) override {
        // Implement calculation logic
        return result;
    }
    // ... implement other methods
};
```
## Troubleshooting
### Common Issues
1. **"ddgs command not found"**: Install DuckDuckGo CLI with `pip install duckduckgo-search[cli]`
2. **SSL/TLS errors**: Ensure OpenSSL is properly installed and configured
3. **API timeout**: Check network connectivity and API key validity
4. **Build errors**: Verify all Boost libraries are installed

### Debug Mode
Enable verbose output by modifying the logging statements in the source code or adding debug flags.
## License
[License information - update based on your project's license]
## Contributing
Contributions are welcome! Please ensure:
- Code follows C++20 standards
- Proper error handling is implemented
- Documentation is updated for new features

## Acknowledgments
- Built on the Agentic Framework
- Uses Google Gemini AI for language processing
- Integrates DuckDuckGo for web search capabilities
- Powered by Boost libraries for networking and process management
