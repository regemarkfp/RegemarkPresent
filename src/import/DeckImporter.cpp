#include "import/DeckImporter.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>

#ifdef RP_HAVE_PDF
#include <QPdfDocument>
#endif

namespace DeckImporter {

static constexpr int kWidth = 1920;   // pages are rendered for a 1080p screen

bool pdfSupported()
{
#ifdef RP_HAVE_PDF
    return true;
#else
    return false;
#endif
}

QStringList fileFilters()
{
    return {QObject::tr("Presentations (*.pdf *.pptx *.ppt *.odp *.key)"),
            QObject::tr("PDF (*.pdf)"), QObject::tr("PowerPoint (*.pptx *.ppt)"), QObject::tr("All files (*)")};
}

QStringList renderPdf(const QString &pdf, const QString &outDir, QString *error)
{
#ifdef RP_HAVE_PDF
    QPdfDocument doc;
    if (doc.load(pdf) != QPdfDocument::Error::None) {
        if (error) *error = QObject::tr("Could not open %1").arg(QFileInfo(pdf).fileName());
        return {};
    }
    QDir().mkpath(outDir);
    QStringList files;
    for (int i = 0; i < doc.pageCount(); ++i) {
        const QSizeF pt = doc.pagePointSize(i);
        const QSize size(kWidth, qMax(1, qRound(kWidth * pt.height() / qMax(1.0, pt.width()))));
        QImage img = doc.render(i, size);
        if (img.isNull()) continue;
        // PDFs have transparent pages; slides should be opaque white like on paper.
        QImage opaque(img.size(), QImage::Format_RGB32);
        opaque.fill(Qt::white);
        QPainter(&opaque).drawImage(0, 0, img);
        const QString file = QDir(outDir).filePath(QStringLiteral("page-%1.png").arg(i + 1, 3, 10, QLatin1Char('0')));
        if (opaque.save(file)) files << file;
    }
    if (files.isEmpty() && error) *error = QObject::tr("No pages could be rendered from %1").arg(QFileInfo(pdf).fileName());
    return files;
#else
    // Without Qt PDF, use poppler's pdftoppm if it's installed.
    const QString tool = QStandardPaths::findExecutable("pdftoppm");
    if (tool.isEmpty()) {
        if (error) *error = QObject::tr("This build can't read PDF files (built without Qt PDF).");
        return {};
    }
    QDir().mkpath(outDir);
    QProcess proc;
    proc.start(tool, {"-png", "-scale-to-x", QString::number(kWidth), "-scale-to-y", "-1", pdf,
                      QDir(outDir).filePath("page")});
    if (!proc.waitForFinished(180000) || proc.exitCode() != 0) {
        if (error) *error = QObject::tr("Could not open %1").arg(QFileInfo(pdf).fileName());
        return {};
    }
    QStringList files = QDir(outDir).entryList({"page-*.png"}, QDir::Files, QDir::Name);
    for (QString &f : files) f = QDir(outDir).filePath(f);
    return files;
#endif
}

static QString findSoffice()
{
    QString exe = QStandardPaths::findExecutable("soffice");
#ifdef Q_OS_WIN
    if (exe.isEmpty())
        for (const QString &dir : {qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)")}) {
            const QString candidate = dir + "/LibreOffice/program/soffice.exe";
            if (QFileInfo::exists(candidate)) return candidate;
        }
#endif
    return exe;
}

#ifdef Q_OS_WIN
// PowerPoint's own exporter gives the most faithful result when it's installed.
static QStringList exportWithPowerPoint(const QString &pptx, const QString &outDir)
{
    QDir().mkpath(outDir);
    const QString script = QStringLiteral(
        "$ErrorActionPreference='Stop';"
        "$app=New-Object -ComObject PowerPoint.Application;"
        "$p=$app.Presentations.Open('%1',$true,$false,$false);"
        "$p.Export('%2','PNG',%3,%4);"
        "$p.Close();$app.Quit()")
        .arg(QDir::toNativeSeparators(pptx).replace("'", "''"),
             QDir::toNativeSeparators(outDir).replace("'", "''"))
        .arg(kWidth).arg(kWidth * 9 / 16);
    QProcess ps;
    ps.start("powershell", {"-NoProfile", "-NonInteractive", "-Command", script});
    if (!ps.waitForFinished(180000) || ps.exitCode() != 0) return {};

    // PowerPoint names them Slide1.PNG, Slide2.PNG, ...: sort numerically.
    QStringList files = QDir(outDir).entryList({"*.png", "*.PNG"}, QDir::Files);
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        static const QRegularExpression num("(\\d+)");
        return num.match(a).captured(1).toInt() < num.match(b).captured(1).toInt();
    });
    for (QString &f : files) f = QDir(outDir).filePath(f);
    return files;
}
#endif

QStringList renderPowerPoint(const QString &pptx, const QString &outDir, QString *error)
{
#ifdef Q_OS_WIN
    const QStringList viaPowerPoint = exportWithPowerPoint(pptx, outDir);
    if (!viaPowerPoint.isEmpty()) return viaPowerPoint;
#endif
    const QString soffice = findSoffice();
    if (soffice.isEmpty()) {
        if (error)
            *error = QObject::tr("To import PowerPoint files, install Microsoft PowerPoint or the free LibreOffice, "
                                 "or save the presentation as PDF and import that.");
        return {};
    }
    QTemporaryDir tmp;
    QProcess lo;
    // A private profile so a LibreOffice window the user has open doesn't block us.
    lo.start(soffice, {"-env:UserInstallation=file:///" + QDir::fromNativeSeparators(tmp.path()) + "/profile",
                       "--headless", "--convert-to", "pdf", "--outdir", tmp.path(), pptx});
    if (!lo.waitForFinished(180000) || lo.exitCode() != 0) {
        if (error) *error = QObject::tr("LibreOffice could not convert %1").arg(QFileInfo(pptx).fileName());
        return {};
    }
    const QString pdf = QDir(tmp.path()).filePath(QFileInfo(pptx).completeBaseName() + ".pdf");
    return renderPdf(pdf, outDir, error);
}

bool importFile(const QString &path, const QString &mediaRoot, Presentation *out, QString *error)
{
    Presentation p;
    p.id = newId();
    p.title = QFileInfo(path).completeBaseName();
    p.category = QStringLiteral("Presentation");
    p.defaultTransition.type = TransitionType::Fade;
    p.defaultTransition.durationMs = 300;

    const QString outDir = QDir(mediaRoot).filePath("Imported/" + p.id);
    const QString ext = QFileInfo(path).suffix().toLower();
    const QStringList pages = ext == "pdf" ? renderPdf(path, outDir, error) : renderPowerPoint(path, outDir, error);
    if (pages.isEmpty()) {
        QDir(outDir).removeRecursively();
        return false;
    }
    for (int i = 0; i < pages.size(); ++i) {
        Slide s;
        s.id = newId();
        s.label = QObject::tr("Slide %1").arg(i + 1);
        s.labelColor = QColor("#a259ff");
        s.background = Background::media(pages[i]);
        s.background.fit = FitMode::Fit;
        p.slides.append(s);
    }
    *out = p;
    return true;
}

}
