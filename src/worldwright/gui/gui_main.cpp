#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("Worldwright");
    QCoreApplication::setOrganizationName("Worldwright");

    worldwright::gui::MainWindow window;
    window.show();
    return application.exec();
}
