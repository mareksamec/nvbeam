#include "nvimprocess.h"
#include <QDebug>
#include <QtEndian>

// Msgpack format constants
namespace msgpack {
    constexpr uint8_t FIXMAP_PREFIX   = 0x80;
    constexpr uint8_t FIXARRAY_PREFIX = 0x90;
    constexpr uint8_t FIXSTR_PREFIX   = 0xa0;
    constexpr uint8_t NIL             = 0xc0;
    constexpr uint8_t FALSE_VAL       = 0xc2;
    constexpr uint8_t TRUE_VAL        = 0xc3;
    constexpr uint8_t UINT8           = 0xcc;
    constexpr uint8_t UINT16          = 0xcd;
    constexpr uint8_t UINT32          = 0xce;
    constexpr uint8_t UINT64          = 0xcf;
    constexpr uint8_t INT8            = 0xd0;
    constexpr uint8_t INT16           = 0xd1;
    constexpr uint8_t INT32           = 0xd2;
    constexpr uint8_t INT64           = 0xd3;
    constexpr uint8_t STR8            = 0xd9;
    constexpr uint8_t STR16           = 0xda;
    constexpr uint8_t STR32           = 0xdb;
    constexpr uint8_t ARRAY16         = 0xdc;
    constexpr uint8_t ARRAY32         = 0xdd;
    constexpr uint8_t MAP16           = 0xde;
    constexpr uint8_t MAP32           = 0xdf;
    constexpr uint8_t NEGATIVE_FIXINT = 0xe0;
    constexpr uint8_t BIN8            = 0xc4;
    constexpr uint8_t BIN16           = 0xc5;
    constexpr uint8_t BIN32           = 0xc6;
    constexpr uint8_t FLOAT32         = 0xca;
    constexpr uint8_t FLOAT64         = 0xcb;
    constexpr uint8_t EXT8            = 0xc7;
    constexpr uint8_t EXT16           = 0xc8;
    constexpr uint8_t EXT32           = 0xc9;
    constexpr uint8_t FIXEXT1         = 0xd4;
    constexpr uint8_t FIXEXT2         = 0xd5;
    constexpr uint8_t FIXEXT4         = 0xd6;
    constexpr uint8_t FIXEXT8         = 0xd7;
    constexpr uint8_t FIXEXT16        = 0xd8;
}

NvimProcess::NvimProcess(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &NvimProcess::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &NvimProcess::onProcessFinished);

    m_timeoutSweep = new QTimer(this);
    m_timeoutSweep->setInterval(5000);
    connect(m_timeoutSweep, &QTimer::timeout, this, [this]() {
        for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ) {
            if (it->deadline.hasExpired()) {
                qWarning() << "RPC request" << it.key() << "timed out, dropping callback";
                it = m_callbacks.erase(it);
            } else {
                ++it;
            }
        }
        if (m_callbacks.isEmpty())
            m_timeoutSweep->stop();
    });
}

NvimProcess::~NvimProcess()
{
    stop();
}

void NvimProcess::start(const QStringList &nvimArgs)
{
    QStringList args = {"--embed"};
    args.append(nvimArgs);
    m_process->start("nvim", args);
    if (!m_process->waitForStarted(3000)) {
        qCritical() << "Failed to start nvim:" << m_process->errorString();
        return;
    }
    qDebug() << "Neovim started, pid:" << m_process->processId();

    // Send a simple request to confirm nvim is ready, then emit ready on first response
    m_waitingForReady = true;
    request("nvim_get_api_info", {});
}

void NvimProcess::stop()
{
    if (m_process->state() != QProcess::NotRunning) {
        notify("nvim_command", {"qa!"});
        m_process->waitForFinished(1000);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    }
    // waitForFinished() is a blocking wait - onProcessFinished() may not have
    // fired yet via the event loop, so clean up explicitly here too.
    m_timeoutSweep->stop();
    m_callbacks.clear();
    m_buffer.clear();
}

void NvimProcess::request(const QString &method, const QVariantList &args)
{
    QVariantList msg = {0, m_msgid++, method, args};
    QByteArray encoded = encodeMsgpack(QVariant(msg));
    m_process->write(encoded);
}

void NvimProcess::request(const QString &method, const QVariantList &args,
                           std::function<void(const QVariant &)> callback)
{
    uint32_t id = m_msgid;
    m_callbacks[id] = PendingRequest{std::move(callback),
                                     QDeadlineTimer(static_cast<qint64>(kRpcTimeoutMs))};
    if (!m_timeoutSweep->isActive())
        m_timeoutSweep->start();
    QVariantList msg = {0, m_msgid++, method, args};
    QByteArray encoded = encodeMsgpack(QVariant(msg));
    m_process->write(encoded);
}

void NvimProcess::notify(const QString &method, const QVariantList &args)
{
    // RPC notification: [type=2, method, params]
    QVariantList msg = {2, method, args};
    QByteArray encoded = encodeMsgpack(QVariant(msg));
    m_process->write(encoded);
}

void NvimProcess::onReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());
    parseMessages(m_buffer);
}

void NvimProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    qDebug() << "Neovim exited with code:" << exitCode;
    m_timeoutSweep->stop();
    m_callbacks.clear();
    m_buffer.clear();
    emit finished();
}

void NvimProcess::parseMessages(const QByteArray &data)
{
    int offset = 0;
    while (offset < data.size()) {
        int startOffset = offset;
        m_decodeIncomplete = false;
        QVariant msg = decodeMsgpack(data, offset);
        if (m_decodeIncomplete) {
            // The message was only partially received (decoder ran out of bytes
            // mid-value, possibly deep inside a nested container). Keep the
            // unconsumed tail and wait for the next read to complete it.
            m_buffer = data.mid(startOffset);
            return;
        }

        QVariantList msgList = msg.toList();
        if (msgList.isEmpty()) continue;

        int type = msgList[0].toInt();
        if (type == 1) {
            // Response: [1, msgid, error, result]
            if (msgList.size() < 4) {
                qWarning() << "Malformed RPC response (expected 4 fields, got" << msgList.size() << ")";
                continue;
            }
            uint32_t respId = msgList[1].toUInt();
            if (m_waitingForReady) {
                m_waitingForReady = false;
                qDebug() << "Neovim ready";
                emit ready();
            }
            if (m_callbacks.contains(respId)) {
                auto req = m_callbacks.take(respId);
                QVariant error = msgList[2];
                if (!error.isNull())
                    qWarning() << "Neovim RPC error for request" << respId << ":" << error;
                req.callback(msgList[3]);
            }
        } else if (type == 2) {
            // Notification: [2, method, params]
            QString method = msgList[1].toString();
            if (method == "redraw") {
                emit redraw(msgList[2].toList());
            }
        }
    }
    m_buffer.clear();
}

// --- Msgpack encoder ---

QByteArray NvimProcess::encodeMsgpack(const QVariant &value)
{
    QByteArray out;

    if (value.isNull()) {
        out.append(static_cast<char>(msgpack::NIL));
    } else if (value.typeId() == QMetaType::Bool) {
        out.append(static_cast<char>(value.toBool() ? msgpack::TRUE_VAL : msgpack::FALSE_VAL));
    } else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong) {
        int64_t v = value.toLongLong();
        if (v >= 0 && v <= 127) {
            out.append(static_cast<char>(v));
        } else if (v >= -32 && v < 0) {
            out.append(static_cast<char>(v));
        } else if (v >= 0 && v <= 0xFF) {
            out.append(static_cast<char>(msgpack::UINT8));
            out.append(static_cast<char>(v));
        } else if (v >= 0 && v <= 0xFFFF) {
            out.append(static_cast<char>(msgpack::UINT16));
            uint16_t be = qToBigEndian(static_cast<uint16_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else if (v >= 0 && v <= 0xFFFFFFFF) {
            out.append(static_cast<char>(msgpack::UINT32));
            uint32_t be = qToBigEndian(static_cast<uint32_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 4);
        } else if (v >= -128 && v < 0) {
            out.append(static_cast<char>(msgpack::INT8));
            out.append(static_cast<char>(v));
        } else if (v >= -32768 && v < 0) {
            out.append(static_cast<char>(msgpack::INT16));
            int16_t be = qToBigEndian(static_cast<int16_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else if (v >= -2147483648LL && v < 0) {
            out.append(static_cast<char>(msgpack::INT32));
            int32_t be = qToBigEndian(static_cast<int32_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 4);
        } else {
            out.append(static_cast<char>(msgpack::INT64));
            int64_t be = qToBigEndian(v);
            out.append(reinterpret_cast<const char*>(&be), 8);
        }
    } else if (value.typeId() == QMetaType::UInt || value.typeId() == QMetaType::ULongLong) {
        uint64_t v = value.toULongLong();
        if (v <= 127) {
            out.append(static_cast<char>(v));
        } else if (v <= 0xFF) {
            out.append(static_cast<char>(msgpack::UINT8));
            out.append(static_cast<char>(v));
        } else if (v <= 0xFFFF) {
            out.append(static_cast<char>(msgpack::UINT16));
            uint16_t be = qToBigEndian(static_cast<uint16_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else if (v <= 0xFFFFFFFF) {
            out.append(static_cast<char>(msgpack::UINT32));
            uint32_t be = qToBigEndian(static_cast<uint32_t>(v));
            out.append(reinterpret_cast<const char*>(&be), 4);
        } else {
            out.append(static_cast<char>(msgpack::UINT64));
            uint64_t be = qToBigEndian(v);
            out.append(reinterpret_cast<const char*>(&be), 8);
        }
    } else if (value.typeId() == QMetaType::QString) {
        QByteArray utf8 = value.toString().toUtf8();
        int len = utf8.size();
        if (len <= 31) {
            out.append(static_cast<char>(msgpack::FIXSTR_PREFIX | len));
        } else if (len <= 0xFF) {
            out.append(static_cast<char>(msgpack::STR8));
            out.append(static_cast<char>(len));
        } else if (len <= 0xFFFF) {
            out.append(static_cast<char>(msgpack::STR16));
            uint16_t be = qToBigEndian(static_cast<uint16_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else {
            out.append(static_cast<char>(msgpack::STR32));
            uint32_t be = qToBigEndian(static_cast<uint32_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 4);
        }
        out.append(utf8);
    } else if (value.typeId() == QMetaType::QVariantList) {
        QVariantList list = value.toList();
        int len = list.size();
        if (len <= 15) {
            out.append(static_cast<char>(msgpack::FIXARRAY_PREFIX | len));
        } else if (len <= 0xFFFF) {
            out.append(static_cast<char>(msgpack::ARRAY16));
            uint16_t be = qToBigEndian(static_cast<uint16_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else {
            out.append(static_cast<char>(msgpack::ARRAY32));
            uint32_t be = qToBigEndian(static_cast<uint32_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 4);
        }
        for (const auto &item : list) {
            out.append(encodeMsgpack(item));
        }
    } else if (value.typeId() == QMetaType::QVariantMap) {
        QVariantMap map = value.toMap();
        int len = map.size();
        if (len <= 15) {
            out.append(static_cast<char>(msgpack::FIXMAP_PREFIX | len));
        } else if (len <= 0xFFFF) {
            out.append(static_cast<char>(msgpack::MAP16));
            uint16_t be = qToBigEndian(static_cast<uint16_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 2);
        } else {
            out.append(static_cast<char>(msgpack::MAP32));
            uint32_t be = qToBigEndian(static_cast<uint32_t>(len));
            out.append(reinterpret_cast<const char*>(&be), 4);
        }
        for (auto it = map.begin(); it != map.end(); ++it) {
            out.append(encodeMsgpack(it.key()));
            out.append(encodeMsgpack(it.value()));
        }
    } else if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
        out.append(static_cast<char>(msgpack::FLOAT64));
        double d = value.toDouble();
        uint64_t be;
        memcpy(&be, &d, 8);
        be = qToBigEndian(be);
        out.append(reinterpret_cast<const char*>(&be), 8);
    } else {
        // Fallback: try as string
        QByteArray utf8 = value.toString().toUtf8();
        int len = utf8.size();
        if (len <= 31) {
            out.append(static_cast<char>(msgpack::FIXSTR_PREFIX | len));
        } else {
            out.append(static_cast<char>(msgpack::STR8));
            out.append(static_cast<char>(len));
        }
        out.append(utf8);
    }

    return out;
}

// --- Msgpack decoder ---

QVariant NvimProcess::decodeMsgpack(const QByteArray &data, int &offset)
{
    if (offset >= data.size()) { m_decodeIncomplete = true; return QVariant(); }

    uint8_t byte = static_cast<uint8_t>(data[offset]);

    // Positive fixint
    if (byte <= 0x7f) {
        offset++;
        return QVariant(static_cast<int>(byte));
    }
    // Negative fixint
    if (byte >= msgpack::NEGATIVE_FIXINT) {
        offset++;
        return QVariant(static_cast<int>(static_cast<int8_t>(byte)));
    }
    // Fixmap
    if ((byte & 0xf0) == msgpack::FIXMAP_PREFIX) {
        int len = byte & 0x0f;
        offset++;
        QVariantMap map;
        for (int i = 0; i < len; i++) {
            QVariant key = decodeMsgpack(data, offset);
            QVariant val = decodeMsgpack(data, offset);
            if (m_decodeIncomplete) return QVariant();
            map[key.toString()] = val;
        }
        return QVariant(map);
    }
    // Fixarray
    if ((byte & 0xf0) == msgpack::FIXARRAY_PREFIX) {
        int len = byte & 0x0f;
        offset++;
        QVariantList list;
        list.reserve(len);
        for (int i = 0; i < len; i++) {
            list.append(decodeMsgpack(data, offset));
            if (m_decodeIncomplete) return QVariant();
        }
        return QVariant(list);
    }
    // Fixstr
    if ((byte & 0xe0) == msgpack::FIXSTR_PREFIX) {
        int len = byte & 0x1f;
        offset++;
        if (offset + len > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        QString str = QString::fromUtf8(data.mid(offset, len));
        offset += len;
        return QVariant(str);
    }

    offset++;
    switch (byte) {
    case msgpack::NIL:
        return QVariant();
    case msgpack::FALSE_VAL:
        return QVariant(false);
    case msgpack::TRUE_VAL:
        return QVariant(true);
    case msgpack::UINT8:
        if (offset + 1 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        return QVariant(static_cast<int>(static_cast<uint8_t>(data[offset++])));
    case msgpack::UINT16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t v = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        return QVariant(static_cast<int>(v));
    }
    case msgpack::UINT32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t v = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        return QVariant(static_cast<qint64>(v));
    }
    case msgpack::UINT64: {
        if (offset + 8 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint64_t v = qFromBigEndian<uint64_t>(data.constData() + offset);
        offset += 8;
        return QVariant(static_cast<qulonglong>(v));
    }
    case msgpack::INT8:
        if (offset + 1 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        return QVariant(static_cast<int>(static_cast<int8_t>(data[offset++])));
    case msgpack::INT16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int16_t v = qFromBigEndian<int16_t>(data.constData() + offset);
        offset += 2;
        return QVariant(static_cast<int>(v));
    }
    case msgpack::INT32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int32_t v = qFromBigEndian<int32_t>(data.constData() + offset);
        offset += 4;
        return QVariant(static_cast<int>(v));
    }
    case msgpack::INT64: {
        if (offset + 8 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int64_t v = qFromBigEndian<int64_t>(data.constData() + offset);
        offset += 8;
        return QVariant(static_cast<qlonglong>(v));
    }
    case msgpack::FLOAT32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t raw = qFromBigEndian<uint32_t>(data.constData() + offset);
        float f;
        memcpy(&f, &raw, 4);
        offset += 4;
        return QVariant(static_cast<double>(f));
    }
    case msgpack::FLOAT64: {
        if (offset + 8 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint64_t raw = qFromBigEndian<uint64_t>(data.constData() + offset);
        double d;
        memcpy(&d, &raw, 8);
        offset += 8;
        return QVariant(d);
    }
    case msgpack::STR8: {
        if (offset + 1 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int len = static_cast<uint8_t>(data[offset++]);
        if (offset + len > data.size()) { offset -= 2; m_decodeIncomplete = true; return QVariant(); }
        QString str = QString::fromUtf8(data.mid(offset, len));
        offset += len;
        return QVariant(str);
    }
    case msgpack::STR16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t len = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        if (offset + len > data.size()) { offset -= 3; m_decodeIncomplete = true; return QVariant(); }
        QString str = QString::fromUtf8(data.mid(offset, len));
        offset += len;
        return QVariant(str);
    }
    case msgpack::STR32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t len = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        if (offset + static_cast<int>(len) > data.size()) { offset -= 5; m_decodeIncomplete = true; return QVariant(); }
        QString str = QString::fromUtf8(data.mid(offset, len));
        offset += len;
        return QVariant(str);
    }
    case msgpack::BIN8: {
        if (offset + 1 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int len = static_cast<uint8_t>(data[offset++]);
        if (offset + len > data.size()) { offset -= 2; m_decodeIncomplete = true; return QVariant(); }
        QByteArray bin = data.mid(offset, len);
        offset += len;
        return QVariant(bin);
    }
    case msgpack::BIN16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t len = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        if (offset + len > data.size()) { offset -= 3; m_decodeIncomplete = true; return QVariant(); }
        QByteArray bin = data.mid(offset, len);
        offset += len;
        return QVariant(bin);
    }
    case msgpack::BIN32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t len = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        if (offset + static_cast<int>(len) > data.size()) { offset -= 5; m_decodeIncomplete = true; return QVariant(); }
        QByteArray bin = data.mid(offset, len);
        offset += len;
        return QVariant(bin);
    }
    case msgpack::ARRAY16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t len = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        QVariantList list;
        list.reserve(len);
        for (int i = 0; i < len; i++) {
            list.append(decodeMsgpack(data, offset));
            if (m_decodeIncomplete) return QVariant();
        }
        return QVariant(list);
    }
    case msgpack::ARRAY32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t len = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        QVariantList list;
        list.reserve(len);
        for (uint32_t i = 0; i < len; i++) {
            list.append(decodeMsgpack(data, offset));
            if (m_decodeIncomplete) return QVariant();
        }
        return QVariant(list);
    }
    case msgpack::MAP16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t len = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        QVariantMap map;
        for (int i = 0; i < len; i++) {
            QVariant key = decodeMsgpack(data, offset);
            QVariant val = decodeMsgpack(data, offset);
            if (m_decodeIncomplete) return QVariant();
            map[key.toString()] = val;
        }
        return QVariant(map);
    }
    case msgpack::MAP32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t len = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        QVariantMap map;
        for (uint32_t i = 0; i < len; i++) {
            QVariant key = decodeMsgpack(data, offset);
            QVariant val = decodeMsgpack(data, offset);
            if (m_decodeIncomplete) return QVariant();
            map[key.toString()] = val;
        }
        return QVariant(map);
    }
    // Ext types — skip the data
    // Ext types — Neovim encodes Buffer/Window/Tabpage handles as ext objects
    // whose payload is a msgpack-encoded integer. We skip the 1-byte ext type
    // id and decode the payload as a normal integer so handles survive decoding
    // (they are interchangeable with integers in the Neovim API).
    case msgpack::FIXEXT1:
    case msgpack::FIXEXT2:
    case msgpack::FIXEXT4:
    case msgpack::FIXEXT8:
    case msgpack::FIXEXT16: {
        int dataLen = 0;
        switch (byte) {
        case msgpack::FIXEXT1:  dataLen = 1;  break;
        case msgpack::FIXEXT2:  dataLen = 2;  break;
        case msgpack::FIXEXT4:  dataLen = 4;  break;
        case msgpack::FIXEXT8:  dataLen = 8;  break;
        case msgpack::FIXEXT16: dataLen = 16; break;
        }
        if (offset + 1 + dataLen > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        offset += 1; // skip ext type id
        // Payload is a self-describing msgpack integer.
        return decodeMsgpack(data, offset);
    }
    case msgpack::EXT8: {
        if (offset + 1 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        int len = static_cast<uint8_t>(data[offset++]);
        // Truncated payload: rewind to the format byte so parseMessages() sees
        // an unchanged offset and treats this as an incomplete (not corrupt)
        // message, keeping the buffer until more bytes arrive.
        if (offset + 1 + len > data.size()) { offset -= 2; m_decodeIncomplete = true; return QVariant(); }
        offset += 1; // skip ext type id
        return decodeMsgpack(data, offset);
    }
    case msgpack::EXT16: {
        if (offset + 2 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint16_t len = qFromBigEndian<uint16_t>(data.constData() + offset);
        offset += 2;
        // Truncated payload: rewind to the format byte (see EXT8 note).
        if (offset + 1 + len > data.size()) { offset -= 3; m_decodeIncomplete = true; return QVariant(); }
        offset += 1; // skip ext type id
        return decodeMsgpack(data, offset);
    }
    case msgpack::EXT32: {
        if (offset + 4 > data.size()) { offset--; m_decodeIncomplete = true; return QVariant(); }
        uint32_t len = qFromBigEndian<uint32_t>(data.constData() + offset);
        offset += 4;
        // Truncated payload: rewind to the format byte (see EXT8 note).
        if (offset + 1 + static_cast<int>(len) > data.size()) { offset -= 5; m_decodeIncomplete = true; return QVariant(); }
        offset += 1; // skip ext type id
        return decodeMsgpack(data, offset);
    }
    default:
        qWarning() << "Unknown msgpack byte:" << Qt::hex << byte;
        return QVariant();
    }
}
