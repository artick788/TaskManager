#include "src/TaskManagerLayer.hpp"
#include <iostream>
#include <VelyraAppFramework/Application.hpp>

int main(const int argc, const char* argv[]) {
    using namespace TaskManager;

    const App::ProgramArgs args(argv, argv + argc);
    try {
        App::ApplicationDesc desc;
        desc.applicationName = "TaskManager";
        App::Application app(desc, args);
        app.createAppLayer<TaskManagerLayer>();
        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Application failed to start: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
