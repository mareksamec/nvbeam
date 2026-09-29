#pragma once

#include <QObject>
#include <QProcess>
#include <QVariant>
#include <QByteArray>
#include <QHash>
#include <functional>

/// Manages the embedded Neovim process and msgpack-RPC communication
class NvimProcess : public QObject
{
    Q_OBJECT

public:
    explicit NvimProcess(QObject *parent = nullptr);
    ~NvimProcess() override;

    void start(const QStringList &nvimArgs = {});
    void stop();

    /// Send an RPC request to Neovim
    void request(const QString &method, const QVariantList &args);

    /// Send an RPC request with a callback for the response
    void request(const QString &method, const QVariantList &args,
                 std::function<void(const QVariant &)> callback);

    /// Send an RPC notification to Neovim
    void notify(const QString &method, const QVariantList &args);

signals:
    /// Emitted when a redraw notification arrives with its event list
    void redraw(const QVariantList &events);

    /// Emitted for any non-"redraw" RPC notification, i.e. custom events
    /// pushed from Lua via vim.rpcnotify (e.g. "nvbeam_git_event").
    void notification(const QString &method, const QVariantList &args);

    /// Emitted when Neovim is ready to receive commands
    void ready();

    /// Emitted when Neovim process exits
    void finished();

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void parseMessages(const QByteArray &data);
    QByteArray encodeMsgpack(const QVariant &value);
    QVariant decodeMsgpack(const QByteArray &data, int &offset);

    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    uint32_t m_msgid = 0;
    bool m_waitingForReady = false;
    // Set by decodeMsgpack() when it runs out of bytes mid-value (the message
    // was only partially received). parseMessages() uses this to keep the
    // partial bytes buffered until the rest arrives, instead of misparsing or
    // discarding them. Necessary because nested containers (arrays/maps) would
    // otherwise swallow a truncated child and return a non-null partial value.
    bool m_decodeIncomplete = false;
    QHash<uint32_t, std::function<void(const QVariant &)>> m_callbacks;
};
