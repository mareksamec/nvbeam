#pragma once

#include <QObject>
#include <QProcess>
#include <QVariant>
#include <QByteArray>
#include <QHash>
#include <QTimer>
#include <QDeadlineTimer>
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

    struct PendingRequest {
        std::function<void(const QVariant &)> callback;
        QDeadlineTimer deadline;
    };
    QHash<uint32_t, PendingRequest> m_callbacks;
    QTimer *m_timeoutSweep = nullptr;
    static constexpr int kRpcTimeoutMs = 30'000;
};
