#include "documentlines.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace {

/// Set the block format of blocks [first, last] from their text.
void formatBlocks(QTextDocument *document, QTextCursor &cursor, int first, int last,
                  const LineFormat &format)
{
    if (!format)
        return;
    QTextBlock block = document->findBlockByNumber(first);
    for (int n = first; n <= last && block.isValid(); ++n, block = block.next()) {
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format(block.text(), block.blockFormat()));
    }
}

/// Replace the text of blocks [first, first + lines.size()) with `lines`, one
/// line per block. The block count does not change.
void replaceBlocks(QTextDocument *document, int first, const QStringList &lines,
                   const LineFormat &format)
{
    const int last = first + lines.size() - 1;
    const QTextBlock lastBlock = document->findBlockByNumber(last);
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.setPosition(document->findBlockByNumber(first).position());
    cursor.setPosition(lastBlock.position() + lastBlock.length() - 1, QTextCursor::KeepAnchor);
    cursor.insertText(lines.join(QLatin1Char('\n')));
    formatBlocks(document, cursor, first, last, format);
    cursor.endEditBlock();
}

/// Make blocks [prefix, oldCount - suffix) hold `middle` instead, where the
/// blocks before `prefix` and the last `suffix` blocks already match. The
/// number of blocks may change.
void replaceRange(QTextDocument *document, int prefix, int suffix, const QStringList &middle,
                  int newCount, const LineFormat &format)
{
    const int oldCount = document->blockCount();
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    if (suffix > 0) {
        // Rewrite from the start of the first differing block up to the start
        // of the first shared trailing one. Every new line brings its own
        // separator, which the shared block keeps after it.
        cursor.setPosition(document->findBlockByNumber(prefix).position());
        cursor.setPosition(document->findBlockByNumber(oldCount - suffix).position(),
                           QTextCursor::KeepAnchor);
        cursor.insertText(middle.isEmpty() ? QString()
                                           : middle.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    } else if (prefix > 0) {
        // Everything after the shared leading lines changes. The rewrite starts
        // at the separator ending the last shared line, so removing every
        // following line also removes that separator.
        const QTextBlock last = document->findBlockByNumber(prefix - 1);
        cursor.setPosition(last.position() + last.length() - 1);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        QString text;
        for (const QString &line : middle)
            text += QLatin1Char('\n') + line;
        cursor.insertText(text);
    } else {
        // Nothing in common.
        cursor.select(QTextCursor::Document);
        cursor.insertText(middle.join(QLatin1Char('\n')));
    }
    // One block past the rewritten range as well: a split at the range's end
    // can leave the first shared trailing line in a block created by the edit,
    // with the format of the line it was split from.
    formatBlocks(document, cursor, prefix, qMin(prefix + middle.size(), newCount - 1), format);
    cursor.endEditBlock();
}

} // namespace

int updateDocumentLines(QTextDocument *document, const QStringList &lines,
                        const LineFormat &format)
{
    // A document always has at least one block, and splitting text on '\n'
    // always gives at least one line, so an empty text is one empty line on
    // both sides.
    const QStringList wanted = lines.isEmpty() ? QStringList{QString()} : lines;
    const int oldCount = document->blockCount();
    const int newCount = wanted.size();

    // Lines both sides share at the start and at the end. The suffix stops
    // where the prefix ends, so a line is never counted in both.
    int prefix = 0;
    for (QTextBlock block = document->begin();
         prefix < oldCount && prefix < newCount && block.text() == wanted.at(prefix);
         block = block.next())
        ++prefix;
    if (prefix == oldCount && prefix == newCount)
        return 0;
    int suffix = 0;
    for (QTextBlock block = document->lastBlock();
         suffix < oldCount - prefix && suffix < newCount - prefix
         && block.text() == wanted.at(newCount - 1 - suffix);
         block = block.previous())
        ++suffix;

    // Between the two, compare line by line over the part both sides have.
    // Lines that only changed in place - a hunk header whose line count moved
    // as lines were appended below it - are replaced one run at a time, so the
    // unchanged lines between them are not rewritten. Each run is its own edit,
    // which keeps the range a highlighter is told to redo to that run.
    const int common = qMin(oldCount, newCount) - prefix - suffix;
    int rewritten = 0;
    QTextBlock block = document->findBlockByNumber(prefix);
    for (int n = prefix; n < prefix + common;) {
        if (block.text() == wanted.at(n)) {
            block = block.next();
            ++n;
            continue;
        }
        int end = n;
        for (QTextBlock run = block; end < prefix + common && run.text() != wanted.at(end);
             run = run.next())
            ++end;
        replaceBlocks(document, n, wanted.mid(n, end - n), format);
        rewritten += end - n;
        n = end;
        block = document->findBlockByNumber(n);
    }

    // What is left differs in length: lines inserted or removed at the end of
    // the compared part.
    const int start = prefix + common;
    if (oldCount - suffix > start || newCount - suffix > start) {
        const QStringList middle = wanted.mid(start, newCount - suffix - start);
        replaceRange(document, start, suffix, middle, newCount, format);
        rewritten += middle.size();
    }
    return rewritten;
}
