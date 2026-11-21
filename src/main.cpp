#include "core/AppSettings.h"
#include "bible/BibleDownloadDialog.h"
#include "bible/BibleLibrary.h"
#include "bible/BiblePanel.h"
#include "core/Library.h"
#include "ui/MainWindow.h"
#include "ui/MediaBin.h"
#include "ui/SlideCanvas.h"
#include "ui/SlideEditor.h"
#include "ui/ScreenSetupDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QListWidget>
#include <QTabWidget>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Regemark");
    QApplication::setApplicationName("Regemark Present");
    QApplication::setApplicationVersion(RP_VERSION);
    // Closing an output window must not quit the app; MainWindow quits explicitly.
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon(":/icon.png"));
    Theme::apply(app);

    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    QCommandLineOption grab("grab", "Save a screenshot of the main window to <file> and exit (diagnostics).", "file");
    args.addOption(grab);
    QCommandLineOption grabScreens("grab-screens", "Save a screenshot of the screen setup dialog to <file> and exit (diagnostics).", "file");
    args.addOption(grabScreens);
    QCommandLineOption grabEditor("grab-editor", "Save a screenshot of the slide editor (first song) to <file> and exit (diagnostics).", "file");
    args.addOption(grabEditor);
    QCommandLineOption grabBibles("grab-bibles", "Save a screenshot of Get Bibles (online catalogue) to <file> and exit (diagnostics).", "file");
    args.addOption(grabBibles);
    args.process(app);

    if (args.isSet(grabScreens)) {
        ScreenSetupDialog dlg(AppSettings::load().screens);
        dlg.show();
        QTimer::singleShot(1200, &dlg, [&] {
            dlg.grab().save(args.value(grabScreens));
            QApplication::exit(0);
        });
        return app.exec();
    }

    if (args.isSet(grabBibles)) {
        BibleLibrary bibles;
        BibleDownloadDialog dlg(&bibles);
        dlg.resize(1100, 700);
        dlg.show();
        QTimer::singleShot(8000, &dlg, [&] {   // give the catalogue time to download
            dlg.grab().save(args.value(grabBibles));
            QApplication::exit(0);
        });
        return app.exec();
    }

    if (args.isSet(grabEditor)) {
        Library library(qEnvironmentVariable("RP_LIBRARY_DIR"));
        library.load();
        const QList<Presentation> songs = library.presentations();
        if (songs.isEmpty()) return 1;
        MediaBin media;
        SlideEditor editor(songs.first(), 1, &media, &library);
        editor.show();
        // RP_GRAB_TYPING=1 opens the main text box for typing first.
        if (!qEnvironmentVariable("RP_GRAB_TYPING").isEmpty())
            QTimer::singleShot(500, &editor, [&] {
                const Slide &s = songs.first().slides.value(1);
                int firstText = 0;
                while (firstText < s.boxes.size() && !s.boxes[firstText].isText()) ++firstText;
                if (auto *canvas = editor.findChild<SlideCanvas *>()) emit canvas->boxEditRequested(firstText);
            });
        QTimer::singleShot(1500, &editor, [&] {
            editor.grab().save(args.value(grabEditor));
            QApplication::exit(0);
        });
        return app.exec();
    }

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
            // RP_GRAB_REF="John 3:16" shows the Bible tab with that passage instead.
            const QString ref = qEnvironmentVariable("RP_GRAB_REF");
            if (!ref.isEmpty()) {
                if (auto *tabs = w.findChild<QTabWidget *>()) tabs->setCurrentIndex(2);
                if (auto *bible = w.findChild<BiblePanel *>()) {
                    bible->showReference(ref);
                    emit bible->presentationReady(bible->currentPresentation());
                }
            }
            const QString tab = qEnvironmentVariable("RP_GRAB_TAB");   // e.g. 3 = SongSelect
            if (!tab.isEmpty())
                if (auto *tabs = w.findChild<QTabWidget *>()) tabs->setCurrentIndex(tab.toInt());
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
