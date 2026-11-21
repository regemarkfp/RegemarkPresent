#pragma once

#include "core/ScreenConfig.h"

#include <QStringList>

struct AppSettings {
    QList<ScreenConfig> screens;
    QStringList mediaFolders;     // e.g. a Sunday Screens download folder
    int linesPerSlide = 2;        // used when importing songs
    QString lastImportDir;
    QString defaultLookId;        // applied to new and imported songs

    static QString path();
    static AppSettings load();
    void save() const;
};
