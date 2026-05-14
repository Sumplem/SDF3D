#include "sdf3d/app/App.h"

int main()
{
    sdf3d::App app;
    if (!app.init()) {
        return 1;
    }

    app.run();
    return 0;
}
