#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QListWidget>
#include <QTimer>
#include <QPalette>
#include <QStyleFactory>

static void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor(37, 37, 38));
    p.setColor(QPalette::WindowText, QColor(220, 220, 220));
    p.setColor(QPalette::Base, QColor(28, 28, 28));
    p.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
    p.setColor(QPalette::ToolTipBase, QColor(50, 50, 50));
    p.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    p.setColor(QPalette::Text, QColor(220, 220, 220));
    p.setColor(QPalette::Button, QColor(50, 50, 52));
    p.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    p.setColor(QPalette::Highlight, QColor(220, 120, 20));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(240, 150, 50));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 110));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 110));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 110, 110));
    app.setPalette(p);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Regemark");
    QApplication::setApplicationName("Regemark Present");
    QApplication::setApplicationVersion(RP_VERSION);
    // Closing an output window must not quit the app; MainWindow quits explicitly.
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon(":/icon.png"));
    applyDarkTheme(app);

    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    QCommandLineOption grab("grab", "Save a screenshot of the main window to <file> and exit (diagnostics).", "file");
    args.addOption(grab);
    args.process(app);

    MainWindow w;
    w.show();
    if (args.isSet(grab)) {
        // Open the first song of the playlist and take a slide live, so the
        // screenshot shows a working show.
        QTimer::singleShot(800, &w, [&] {
            auto *playlist = w.findChild<QListWidget *>("playlist");
            for (int i = 0; playlist && i < playlist->count(); ++i)
                if (!playlist->item(i)->data(Qt::UserRole).toString().isEmpty()) {
                    emit playlist->itemClicked(playlist->item(i));
                    break;
                }
            auto *grid = w.findChild<QListWidget *>("slideGrid");
            if (grid && grid->count() > 1) emit grid->itemClicked(grid->item(1));
        });
        QTimer::singleShot(2500, &w, [&] {
            w.grab().save(args.value(grab));
            QApplication::exit(0);
        });
    }
    return app.exec();
}
