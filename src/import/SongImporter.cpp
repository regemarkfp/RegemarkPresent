#include "import/SongImporter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>

namespace SongImporter {

static const QRegularExpression kSectionLabel(
    QStringLiteral("^(verse|chorus|pre-?chorus|post-?chorus|bridge|tag|intro|outro|ending|interlude|"
                   "refrain|vamp|misc|instrumental|turnaround|coda|breakdown|channel|hook)\\b[\\w\\s]{0,12}$"),
    QRegularExpression::CaseInsensitiveOption);

static QString titleCase(const QString &s)
{
    QString t = s.trimmed();
    if (!t.isEmpty()) t[0] = t[0].toUpper();
    return t;
}

static QString joinAuthors(const QString &s)
{
    QStringList names;
    for (const QString &n : s.split('|'))
        if (!n.trimmed().isEmpty()) names << n.trimmed();
    return names.join(", ");
}

static void addSection(Song &song, const QString &label, const QStringList &lines)
{
    QStringList clean;
    for (const QString &l : lines)
        if (!l.trimmed().isEmpty()) clean << l.trimmed();
    if (clean.isEmpty()) return;
    song.sections.append({label.isEmpty() ? QStringLiteral("Verse") : titleCase(label), clean});
}

static QString decode(const QByteArray &data)
{
    // SongSelect files are UTF-8 (sometimes with BOM) or UTF-16; older USR
    // files can be Windows-1252.
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        QStringDecoder d(QStringDecoder::Utf16);
        return d(data);
    }
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString s = utf8(data);
    if (!utf8.hasError()) return s.startsWith(QChar(0xFEFF)) ? s.mid(1) : s;
    return QString::fromLatin1(data);
}

bool parseFile(const QString &path, Song *song, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QString content = decode(f.readAll());
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "usr") *song = parseUsr(content);
    else if (ext == "cho" || ext == "chordpro" || ext == "chopro" || ext == "pro" || ext == "crd") *song = parseChordPro(content);
    else if (content.contains("[File]") && content.contains("Words=")) *song = parseUsr(content);
    else *song = parseSongSelectText(content);

    if (song->title.isEmpty()) song->title = QFileInfo(path).completeBaseName();
    if (song->sections.isEmpty()) {
        if (error) *error = QObject::tr("No lyrics found in %1").arg(QFileInfo(path).fileName());
        return false;
    }
    return true;
}

Song parseUsr(const QString &content)
{
    Song song;
    QString fields, words;
    static const QRegularExpression header(QStringLiteral("^\\[S A?(\\d+)\\]"));
    for (const QString &raw : content.split(QRegularExpression("\r?\n"))) {
        const QString line = raw.trimmed();
        const auto m = header.match(line);
        if (m.hasMatch()) { song.ccliNumber = m.captured(1); continue; }
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed().toLower();
        const QString value = line.mid(eq + 1);
        if (key == "title") song.title = value.trimmed();
        else if (key == "author") song.author = joinAuthors(value);
        else if (key == "copyright") song.copyright = value.trimmed();
        else if (key == "fields") fields = value;
        else if (key == "words") words = value;
        else if (key == "ccli" || key == "ccli number") song.ccliNumber = value.trimmed();
    }
    const QStringList labels = fields.split("/t");
    const QStringList bodies = words.split("/t");
    for (int i = 0; i < bodies.size(); ++i)
        addSection(song, labels.value(i, QStringLiteral("Verse")), bodies[i].split("/n"));
    return song;
}

Song parseSongSelectText(const QString &content)
{
    Song song;
    QStringList lines = content.split(QRegularExpression("\r?\n"));

    // Footer: "CCLI Song # 1234" then authors, "©" copyright, terms, licence.
    int footer = -1;
    static const QRegularExpression ccliRe(QStringLiteral("^CCLI Song\\s*#\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < lines.size(); ++i) {
        const auto m = ccliRe.match(lines[i].trimmed());
        if (m.hasMatch()) {
            footer = i;
            song.ccliNumber = m.captured(1);
            break;
        }
    }
    if (footer >= 0) {
        for (int i = footer + 1; i < lines.size(); ++i) {
            const QString l = lines[i].trimmed();
            if (l.isEmpty() || l.startsWith("For use solely") || l.startsWith("CCLI Licen")) continue;
            if (l.startsWith(QChar(0x00A9)) || l.startsWith("(c)", Qt::CaseInsensitive)) song.copyright = l;
            else if (song.author.isEmpty()) song.author = joinAuthors(l);
        }
        lines = lines.mid(0, footer);
    }

    int i = 0;
    while (i < lines.size() && lines[i].trimmed().isEmpty()) ++i;
    if (i < lines.size()) song.title = lines[i++].trimmed();

    // Remaining body: blank-line separated blocks, each optionally led by a label.
    QString label;
    QStringList block;
    auto flush = [&] {
        if (block.isEmpty()) return;
        if (kSectionLabel.match(block.first().trimmed()).hasMatch()) {
            label = block.takeFirst().trimmed();
            addSection(song, label, block);
        } else if (!song.sections.isEmpty() && label.isEmpty()) {
            song.sections.last().lines += block;
        } else {
            addSection(song, label.isEmpty() ? QStringLiteral("Verse") : label, block);
        }
        label.clear();
        block.clear();
    };
    for (; i < lines.size(); ++i) {
        const QString l = lines[i].trimmed();
        if (l.isEmpty()) flush();
        else block << l;
    }
    flush();
    return song;
}

Song parseChordPro(const QString &content)
{
    Song song;
    static const QRegularExpression directive(QStringLiteral("^\\{\\s*([\\w_-]+)\\s*(?::\\s*(.*?))?\\s*\\}$"));
    static const QRegularExpression chords(QStringLiteral("\\[[^\\]]*\\]"));
    QString label;
    QStringList block;
    auto flush = [&] {
        addSection(song, label, block);
        block.clear();
    };
    for (const QString &raw : content.split(QRegularExpression("\r?\n"))) {
        const QString line = raw.trimmed();
        if (line.startsWith('#')) continue;
        const auto m = directive.match(line);
        if (m.hasMatch()) {
            const QString d = m.captured(1).toLower();
            const QString v = m.captured(2).trimmed();
            if (d == "title" || d == "t") song.title = v;
            else if (d == "artist" || d == "subtitle" || d == "st" || d == "composer") { if (song.author.isEmpty()) song.author = v; }
            else if (d == "copyright") song.copyright = v;
            else if (d == "ccli") song.ccliNumber = v;
            else if (d.startsWith("start_of_") || d == "sov" || d == "soc" || d == "sob") {
                flush();
                QString kind = d == "sov" ? "verse" : d == "soc" ? "chorus" : d == "sob" ? "bridge" : d.mid(9);
                label = v.isEmpty() ? titleCase(kind) : v;
            } else if (d.startsWith("end_of_") || d == "eov" || d == "eoc" || d == "eob") {
                flush();
                label.clear();
            } else if (d == "comment" || d == "c" || d == "ci" || d == "comment_italic") {
                flush();
                label = v;
            }
            continue;
        }
        const QString text = QString(line).remove(chords).simplified();
        if (text.isEmpty()) {
            if (!block.isEmpty()) { flush(); label.clear(); }
            continue;
        }
        // Some ChordPro files label sections with a bare "Chorus:" line.
        const QString bare = text.endsWith(':') ? text.chopped(1) : text;
        if (block.isEmpty() && label.isEmpty() && kSectionLabel.match(bare).hasMatch()) {
            label = bare;
            continue;
        }
        block << text;
    }
    flush();
    return song;
}

Presentation toPresentation(const Song &song, int linesPerSlide, const Presentation &templ)
{
    Presentation p = templ;
    p.id = newId();
    p.slides.clear();
    p.title = song.title;
    p.author = song.author;
    p.copyright = song.copyright;
    p.ccliNumber = song.ccliNumber;
    p.category = QStringLiteral("Song");
    for (const Section &s : song.sections) {
        const int step = linesPerSlide > 0 ? linesPerSlide : int(s.lines.size());
        for (int i = 0; i < s.lines.size(); i += step)
            p.slides.append(p.makeSlide(s.label, s.lines.mid(i, step).join('\n')));
    }
    return p;
}

QStringList fileFilters()
{
    return {QObject::tr("Song files (*.usr *.txt *.cho *.chordpro *.chopro *.pro)"),
            QObject::tr("SongSelect USR (*.usr)"),
            QObject::tr("SongSelect text (*.txt)"),
            QObject::tr("ChordPro (*.cho *.chordpro *.chopro *.pro)"),
            QObject::tr("All files (*)")};
}

}
