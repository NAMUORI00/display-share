#include "monitoring/PerformanceMonitor.h"
#include <iostream>

int main() {
    try {
        PerformanceMonitor app;
        
        if (!app.Initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }
        
        app.Run();
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return -1;
    }
}
