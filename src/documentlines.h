#pragma once

#include <QStringList>
#include <QTextBlockFormat>

#include <functional>

class QTextDocument;

/// Gives the block format for one line of text. Called for every rewritten
/// block, with the format the block has now.
using LineFormat = std::function<QTextBlockFormat(const QString &line,
                                                  const QTextBlockFormat &current)>;

/// Make `document` hold `lines`, one block per line, by rewriting only the
/// blocks that differ. Leading and trailing lines it already shares with
/// `lines` are kept; between them, lines that changed in place are replaced one
/// run at a time, and only the lines inserted or removed at the end of that
/// part change the block count.
///
/// A diff that grew by a few lines is then a small edit rather than a whole new
/// text: the layout, a QSyntaxHighlighter on the document and any selection or
/// scroll position outside the rewritten blocks are left alone. Each run goes
/// in as one edit block with its block formats, so the document reports one
/// contentsChange per run, covering that run only.
///
/// `format`, when given, sets the block format of each rewritten block.
/// Returns the number of lines written; removed lines are not counted.
int updateDocumentLines(QTextDocument *document, const QStringList &lines,
                        const LineFormat &format = {});
