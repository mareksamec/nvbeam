#pragma once

#include <QObject>
#include <QProcess>
#include <QVariant>
#include <QByteArray>

/// Manages the embedded Neovim process and msgpack-RPC communication
class NvimProcess : public QObject
{
    Q_OBJECT

public:
    explicit NvimProcess(QObject *parent = nullptr);
    ~NvimProcess() override;

    void start();
    void stop();

    /// Send an RPC request to Neovim
    void request(const QString &method, const QVariantList &args);

    /// Send an RPC notification to Neovim
    void notify(const QString &method, const QVariantList &args);

signals:
    /// Emitted when a redraw notification arrives with its event list
    void redraw(const QVariantList &events);

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
};
